from pathlib import Path

service = r'''package com.chatgpt.speechnotes;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.graphics.drawable.Icon;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.SystemClock;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.atomic.AtomicBoolean;

public class RecordingService extends Service {
    public static final String ACTION_START = "com.chatgpt.speechnotes.START";
    public static final String ACTION_STOP = "com.chatgpt.speechnotes.STOP";
    public static final String ACTION_STATE = "com.chatgpt.speechnotes.STATE";
    public static final String EXTRA_STATE = "state";
    public static final String EXTRA_TEXT = "text";
    public static final String EXTRA_MODEL = "model";
    public static final String EXTRA_SAVE_WAV = "save_wav";
    public static final String EXTRA_AVIATION_PROMPT = "aviation_prompt";

    private static final String CHANNEL_ID = "speech_notes_recording";
    private static final int NOTIFICATION_ID = 41;
    private static final int SAMPLE_RATE = 16000;
    private static final int BYTES_PER_SAMPLE = 2;
    private static final int WINDOW_SECONDS = 25;
    private static final int STRIDE_SECONDS = 22;
    private static final int RING_SECONDS = 90;
    private static final long WINDOW_SAMPLES = (long) SAMPLE_RATE * WINDOW_SECONDS;
    private static final long STRIDE_SAMPLES = (long) SAMPLE_RATE * STRIDE_SECONDS;

    private static volatile boolean recording;
    private static volatile boolean transcribing;
    private static volatile long startedElapsed;
    private static volatile String streamingStatus = "";

    private final AtomicBoolean stopRequested = new AtomicBoolean(false);
    private final LinkedBlockingDeque<WindowJob> inferenceQueue = new LinkedBlockingDeque<>(2);
    private final StreamingTranscriptAssembler assembler = new StreamingTranscriptAssembler();
    private final WhisperBridge.Result aggregate = new WhisperBridge.Result();

    private AudioRecord audioRecord;
    private File pcmFile;
    private Thread recordThread;
    private Thread inferenceThread;
    private StreamingAudioBuffer ring;
    private String model = SpeechRuntimeConfig.DEFAULT_MODEL;
    private boolean saveWav;
    private boolean aviationPrompt;
    private long startedWall;
    private PowerManager.WakeLock wakeLock;
    private long nextWindowEnd = WINDOW_SAMPLES;
    private long lastEnqueuedEnd;
    private long lastProcessedEnd;
    private int windowsProcessed;
    private int windowsDropped;
    private long totalInferenceWallMs;
    private volatile boolean finalQueued;

    private static final class WindowJob {
        final long startSample;
        final long endSample;
        final short[] pcm;
        final boolean terminal;
        WindowJob(long startSample, long endSample, short[] pcm, boolean terminal) {
            this.startSample = startSample;
            this.endSample = endSample;
            this.pcm = pcm;
            this.terminal = terminal;
        }
    }

    public static boolean isRecording() { return recording; }
    public static boolean isTranscribing() { return transcribing; }
    public static long getStartedElapsed() { return startedElapsed; }
    public static String getStreamingStatus() { return streamingStatus; }

    @Override public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) return START_NOT_STICKY;
        CrashDiagnostics.mark(this, "service:onStartCommand action=" + intent.getAction());
        String action = intent.getAction();
        if (ACTION_STOP.equals(action)) {
            requestStop();
            return START_NOT_STICKY;
        }
        if (ACTION_START.equals(action) && !recording && !transcribing) {
            model = intent.getStringExtra(EXTRA_MODEL);
            if (model == null) model = SpeechRuntimeConfig.DEFAULT_MODEL;
            saveWav = intent.getBooleanExtra(EXTRA_SAVE_WAV, false);
            aviationPrompt = intent.getBooleanExtra(EXTRA_AVIATION_PROMPT, false);
            startRecording();
        }
        return START_NOT_STICKY;
    }

    private void startRecording() {
        CrashDiagnostics.mark(this, "record:start_enter model=" + model);
        try {
            Intent stopIntent = new Intent(this, RecordingService.class).setAction(ACTION_STOP);
            PendingIntent stopPending = PendingIntent.getService(this, 2, stopIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            Notification preparing = buildNotification("Aufnahme wird vorbereitet", "Streaming-Runtime initialisiert", stopPending, true);
            if (Build.VERSION.SDK_INT >= 29) {
                startForeground(NOTIFICATION_ID, preparing, ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE);
            } else {
                startForeground(NOTIFICATION_ID, preparing);
            }
            CrashDiagnostics.mark(this, "record:foreground_started");

            long restoredLoadMs = SpeechRuntimeSession.ensureLoaded(this, model);
            CrashDiagnostics.mark(this, "record:runtime_ready restored_load_ms=" + restoredLoadMs + " "
                    + SpeechRuntimeSession.snapshot().describe());

            if (checkSelfPermission(android.Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                throw new SecurityException("Mikrofonberechtigung RECORD_AUDIO fehlt");
            }
            int min = AudioRecord.getMinBufferSize(SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
            if (min <= 0) throw new IllegalStateException("Ungültige AudioRecord buffer size: " + min);
            int bufferSize = Math.max(min, SAMPLE_RATE * BYTES_PER_SAMPLE);
            audioRecord = new AudioRecord(MediaRecorder.AudioSource.VOICE_RECOGNITION,
                    SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT, bufferSize);
            if (audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
                throw new IllegalStateException("AudioRecord konnte nicht initialisiert werden");
            }

            pcmFile = new File(getCacheDir(), "recording-" + System.currentTimeMillis() + ".pcm");
            ring = new StreamingAudioBuffer(SAMPLE_RATE * RING_SECONDS);
            inferenceQueue.clear();
            stopRequested.set(false);
            nextWindowEnd = WINDOW_SAMPLES;
            lastEnqueuedEnd = 0L;
            lastProcessedEnd = 0L;
            windowsProcessed = 0;
            windowsDropped = 0;
            totalInferenceWallMs = 0L;
            finalQueued = false;
            streamingStatus = "Warte auf erstes 25-s-Fenster";
            startedWall = System.currentTimeMillis();
            startedElapsed = SystemClock.elapsedRealtime();
            recording = true;
            transcribing = false;

            acquireWakeLock();
            audioRecord.startRecording();
            if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                throw new IllegalStateException("AudioRecord ist nicht im RECORDSTATE_RECORDING");
            }
            CrashDiagnostics.mark(this, "record:audiorecord_started buffer=" + bufferSize);

            inferenceThread = new Thread(this::inferenceLoop, "speech-stream-inference");
            inferenceThread.start();
            final int finalBufferSize = bufferSize;
            recordThread = new Thread(() -> captureLoop(finalBufferSize), "speech-capture");
            recordThread.start();
            broadcast("recording", null);
            updateNotification("Live-Aufnahme", streamingStatus);
        } catch (Throwable t) {
            failSession("record:start_failed", t);
        }
    }

    private void captureLoop(int bufferSize) {
        byte[] buffer = new byte[bufferSize];
        try (BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(pcmFile), bufferSize * 2)) {
            while (!stopRequested.get()) {
                int n = audioRecord.read(buffer, 0, buffer.length);
                if (n > 0) {
                    out.write(buffer, 0, n);
                    long captured = ring.appendPcm16Le(buffer, n);
                    scheduleReadyWindows(captured);
                } else if (n < 0) {
                    throw new IOException("AudioRecord read error: " + n);
                }
            }
            out.flush();
        } catch (Throwable t) {
            CrashDiagnostics.markError(this, "record:capture_failed", t);
            broadcast("error", "Capture: " + safeMessage(t));
        } finally {
            try { audioRecord.stop(); } catch (Throwable ignored) { }
            recording = false;
            releaseRecorder();
            queueFinalWindow();
        }
    }

    private void scheduleReadyWindows(long capturedSamples) {
        while (capturedSamples >= nextWindowEnd && !stopRequested.get()) {
            long start = Math.max(0L, nextWindowEnd - WINDOW_SAMPLES);
            short[] window = ring.snapshot(start, nextWindowEnd);
            enqueueWindow(new WindowJob(start, nextWindowEnd, window, false));
            lastEnqueuedEnd = nextWindowEnd;
            nextWindowEnd += STRIDE_SAMPLES;
        }
    }

    private void enqueueWindow(WindowJob job) {
        if (job.pcm.length == 0) return;
        if (!inferenceQueue.offerLast(job)) {
            WindowJob dropped = inferenceQueue.pollFirst();
            if (dropped != null && !dropped.terminal) windowsDropped++;
            if (!inferenceQueue.offerLast(job)) {
                windowsDropped++;
                CrashDiagnostics.mark(this, "stream:queue_reject terminal=" + job.terminal);
                if (job.terminal) {
                    inferenceQueue.clear();
                    inferenceQueue.offerLast(job);
                }
            }
        }
        updateStreamingStatus(ring.totalSamples());
    }

    private synchronized void queueFinalWindow() {
        if (finalQueued) return;
        finalQueued = true;
        transcribing = true;
        long end = ring == null ? 0L : ring.totalSamples();
        if (end < SAMPLE_RATE / 2L) {
            finishSession();
            return;
        }
        long start = Math.max(0L, end - WINDOW_SAMPLES);
        if (end == lastEnqueuedEnd) {
            // Last scheduled window already ends exactly at stop. Mark a terminal copy only
            // if it is no longer pending; otherwise replace pending job with terminal flag.
            WindowJob replacement = new WindowJob(start, end, ring.snapshot(start, end), true);
            inferenceQueue.clear();
            enqueueWindow(replacement);
        } else {
            enqueueWindow(new WindowJob(start, end, ring.snapshot(start, end), true));
        }
        broadcast("transcribing", assembler.text());
        updateNotification("Finalisiere Transkript", "Letztes Fenster wird verarbeitet");
        CrashDiagnostics.mark(this, "stream:final_queued samples=" + end + " queue=" + inferenceQueue.size());
    }

    private void inferenceLoop() {
        try {
            while (true) {
                WindowJob job = inferenceQueue.takeFirst();
                processWindow(job);
                if (job.terminal) return;
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            if (!finalQueued) failSession("stream:worker_interrupted", e);
        } catch (Throwable t) {
            failSession("stream:worker_failed", t);
        }
    }

    private void processWindow(WindowJob job) throws Exception {
        long queueStartMs = SystemClock.elapsedRealtime();
        SpeechRuntimeSession.ensureLoaded(this, model);
        File temp = new File(getCacheDir(), "stream-window-" + job.endSample + ".pcm");
        writePcm16(temp, job.pcm);
        WhisperBridge.Result result;
        long wallStart = SystemClock.elapsedRealtime();
        try {
            String prompt = aviationPrompt ? AviationVocabulary.PROMPT : "";
            result = SpeechRuntimeConfig.transcribe(temp.getAbsolutePath(), model, prompt);
        } finally {
            temp.delete();
        }
        long wallMs = SystemClock.elapsedRealtime() - wallStart;
        totalInferenceWallMs += wallMs;
        mergeProfile(aggregate, result, wallMs);
        String text = result.text == null ? "" : result.text.trim();
        assembler.merge(text);
        lastProcessedEnd = job.endSample;
        windowsProcessed++;
        long captured = ring == null ? job.endSample : ring.totalSamples();
        updateStreamingStatus(captured);
        CrashDiagnostics.mark(this, "stream:window_done n=" + windowsProcessed
                + " start=" + samplesToMs(job.startSample)
                + " end=" + samplesToMs(job.endSample)
                + " wall_ms=" + wallMs
                + " queue_ms=" + Math.max(0L, SystemClock.elapsedRealtime() - queueStartMs - wallMs)
                + " backlog_ms=" + samplesToMs(Math.max(0L, captured - lastProcessedEnd))
                + " dropped=" + windowsDropped);
        if (recording) {
            broadcast("streaming", assembler.text());
            updateNotification("Live-Aufnahme", streamingStatus);
        }
        if (job.terminal) finishSession();
    }

    private void updateStreamingStatus(long capturedSamples) {
        long backlogSamples = Math.max(0L, capturedSamples - lastProcessedEnd);
        double rtf = windowsProcessed == 0 ? 0.0
                : totalInferenceWallMs / (windowsProcessed * WINDOW_SECONDS * 1000.0);
        streamingStatus = "Fenster " + windowsProcessed
                + " · Queue " + inferenceQueue.size()
                + " · Backlog " + String.format(java.util.Locale.ROOT, "%.1fs", backlogSamples / (double) SAMPLE_RATE)
                + " · RTF " + String.format(java.util.Locale.ROOT, "%.2f", rtf)
                + (windowsDropped > 0 ? " · Drops " + windowsDropped : "");
    }

    private synchronized void finishSession() {
        if (!transcribing && !finalQueued) return;
        transcribing = false;
        String resultText = assembler.text();
        String wavPath = null;
        long durationMs = Math.max(0L, System.currentTimeMillis() - startedWall);
        try {
            if (saveWav && pcmFile != null && pcmFile.exists()) {
                File wavDir = new File(getFilesDir(), "recordings");
                if (!wavDir.exists() && !wavDir.mkdirs()) throw new IOException("WAV-Verzeichnis konnte nicht erstellt werden");
                File wav = new File(wavDir, "SpeechNotes-" + startedWall + ".wav");
                writeWav(pcmFile, wav, SAMPLE_RATE);
                wavPath = wav.getAbsolutePath();
            }
            new TranscriptDb(this).insert(startedWall, durationMs, aggregate,
                    WhisperBridge.currentModelLoadMs(), model, resultText, countWords(resultText), wavPath);
            CrashDiagnostics.mark(this, "stream:done windows=" + windowsProcessed
                    + " drops=" + windowsDropped + " total_inference_ms=" + totalInferenceWallMs);
            broadcast("done", resultText);
        } catch (Throwable t) {
            CrashDiagnostics.markError(this, "stream:finish_failed", t);
            broadcast("error", "Finalisierung: " + safeMessage(t));
        } finally {
            if (pcmFile != null) pcmFile.delete();
            releaseWakeLock();
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
        }
    }

    private synchronized void requestStop() {
        if (recording) {
            stopRequested.set(true);
            broadcast("stopping", assembler.text());
            CrashDiagnostics.mark(this, "record:stop_requested");
        }
    }

    private void failSession(String marker, Throwable t) {
        CrashDiagnostics.markError(this, marker, t);
        recording = false;
        transcribing = false;
        stopRequested.set(true);
        releaseRecorder();
        releaseWakeLock();
        broadcast("error", safeMessage(t));
        stopForeground(STOP_FOREGROUND_REMOVE);
        stopSelf();
    }

    private void acquireWakeLock() {
        if (wakeLock != null && wakeLock.isHeld()) return;
        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SpeechNotes:Streaming");
        wakeLock.setReferenceCounted(false);
        wakeLock.acquire(6L * 60L * 60L * 1000L);
    }

    private void releaseRecorder() {
        AudioRecord r = audioRecord;
        audioRecord = null;
        if (r != null) try { r.release(); } catch (Throwable ignored) { }
    }

    private void releaseWakeLock() {
        if (wakeLock != null && wakeLock.isHeld()) try { wakeLock.release(); } catch (Throwable ignored) { }
        wakeLock = null;
    }

    private void broadcast(String state, String text) {
        Intent i = new Intent(ACTION_STATE).setPackage(getPackageName());
        i.putExtra(EXTRA_STATE, state);
        if (text != null) i.putExtra(EXTRA_TEXT, text);
        sendBroadcast(i);
    }

    private void updateNotification(String title, String text) {
        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        Intent stopIntent = new Intent(this, RecordingService.class).setAction(ACTION_STOP);
        PendingIntent stopPending = PendingIntent.getService(this, 2, stopIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        nm.notify(NOTIFICATION_ID, buildNotification(title, text, stopPending, true));
    }

    private void createNotificationChannel() {
        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        NotificationChannel channel = new NotificationChannel(CHANNEL_ID,
                "Aufnahme", NotificationManager.IMPORTANCE_LOW);
        channel.setDescription("Zeigt Aufnahme und lokale Streaming-Transkription an");
        channel.setSound(null, null);
        nm.createNotificationChannel(channel);
    }

    private Notification buildNotification(String title, String text, PendingIntent stop, boolean ongoing) {
        Intent open = new Intent(this, MainActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        PendingIntent openPi = PendingIntent.getActivity(this, 1, open,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder b = new Notification.Builder(this, CHANNEL_ID)
                .setSmallIcon(com.chatgpt.speechnotes.R.drawable.ic_mic)
                .setContentTitle(title)
                .setContentText(text)
                .setContentIntent(openPi)
                .setOngoing(ongoing)
                .setOnlyAlertOnce(true)
                .setCategory(Notification.CATEGORY_SERVICE)
                .setVisibility(Notification.VISIBILITY_PUBLIC);
        if (stop != null) b.addAction(new Notification.Action.Builder(
                Icon.createWithResource(this, com.chatgpt.speechnotes.R.drawable.ic_mic),
                "Stop", stop).build());
        return b.build();
    }

    @Override public void onTaskRemoved(Intent rootIntent) {
        if (recording) requestStop();
        super.onTaskRemoved(rootIntent);
    }

    @Override public void onDestroy() {
        if (recording) stopRequested.set(true);
        releaseRecorder();
        if (!transcribing) releaseWakeLock();
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }

    private static void writePcm16(File file, short[] samples) throws IOException {
        try (BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(file), 128 * 1024)) {
            for (short sample : samples) {
                out.write(sample & 0xff);
                out.write((sample >>> 8) & 0xff);
            }
        }
    }

    private static void mergeProfile(WhisperBridge.Result a, WhisperBridge.Result r, long wallMs) {
        a.whisperMs += wallMs;
        a.pcmMs += r.pcmMs;
        a.nativeTotalMs += r.nativeTotalMs;
        a.melMs += r.melMs;
        a.encodeTotalMs += r.encodeTotalMs; a.encodeRuns += r.encodeRuns;
        a.decodeTotalMs += r.decodeTotalMs; a.decodeRuns += r.decodeRuns;
        a.sampleTotalMs += r.sampleTotalMs; a.sampleRuns += r.sampleRuns;
        a.batchTotalMs += r.batchTotalMs; a.batchRuns += r.batchRuns;
        a.promptTotalMs += r.promptTotalMs; a.promptRuns += r.promptRuns;
        a.encodeMs = avg(a.encodeTotalMs, a.encodeRuns);
        a.decodeMs = avg(a.decodeTotalMs, a.decodeRuns);
        a.sampleMs = avg(a.sampleTotalMs, a.sampleRuns);
        a.batchMs = avg(a.batchTotalMs, a.batchRuns);
        a.promptMs = avg(a.promptTotalMs, a.promptRuns);
    }

    private static long avg(long total, int n) { return n <= 0 ? 0L : total / n; }
    private static long samplesToMs(long samples) { return samples * 1000L / SAMPLE_RATE; }
    private static int countWords(String text) {
        String t = text == null ? "" : text.trim();
        return t.isEmpty() ? 0 : t.split("\\s+").length;
    }
    private static String safeMessage(Throwable t) {
        String m = t == null ? null : t.getMessage();
        return (m == null || m.trim().isEmpty()) ? (t == null ? "Unbekannter Fehler" : t.getClass().getSimpleName()) : m;
    }

    private static void writeWav(File pcm, File wav, int sampleRate) throws IOException {
        long dataSize = pcm.length();
        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(pcm));
             BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(wav))) {
            writeAscii(out, "RIFF"); writeLe32(out, 36 + dataSize); writeAscii(out, "WAVE");
            writeAscii(out, "fmt "); writeLe32(out, 16); writeLe16(out, 1); writeLe16(out, 1);
            writeLe32(out, sampleRate); writeLe32(out, sampleRate * 2L); writeLe16(out, 2); writeLe16(out, 16);
            writeAscii(out, "data"); writeLe32(out, dataSize);
            byte[] buffer = new byte[128 * 1024]; int n;
            while ((n = in.read(buffer)) >= 0) out.write(buffer, 0, n);
        }
    }
    private static void writeAscii(BufferedOutputStream out, String s) throws IOException { out.write(s.getBytes(java.nio.charset.StandardCharsets.US_ASCII)); }
    private static void writeLe16(BufferedOutputStream out, long v) throws IOException { out.write((int)(v & 0xff)); out.write((int)((v >> 8) & 0xff)); }
    private static void writeLe32(BufferedOutputStream out, long v) throws IOException {
        out.write((int)(v & 0xff)); out.write((int)((v >> 8) & 0xff)); out.write((int)((v >> 16) & 0xff)); out.write((int)((v >> 24) & 0xff));
    }
}
'''

Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/RecordingService.java').write_text(service)

main = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/MainActivity.java')
s = main.read_text()

field_anchor = '    private TextView modelLoadStatus;\n'
if field_anchor not in s:
    raise SystemExit('v1.9.0 MainActivity field anchor missing')
s = s.replace(field_anchor, field_anchor + '    private TextView streamingText;\n', 1)

ui_anchor = '''        timer.setGravity(Gravity.CENTER);\n        recorderCard.addView(timer, matchWrap());\n        recorderCard.addView(space(22));\n'''
ui_new = '''        timer.setGravity(Gravity.CENTER);\n        recorderCard.addView(timer, matchWrap());\n        recorderCard.addView(space(12));\n\n        streamingText = text("Live-Transkript erscheint nach dem ersten 25-s-Fenster.", 14, MUTED, false);\n        streamingText.setPadding(dp(12), dp(10), dp(12), dp(10));\n        streamingText.setBackground(roundRect(CARD_2, 12));\n        recorderCard.addView(streamingText, matchWrap());\n        recorderCard.addView(space(14));\n'''
if ui_anchor not in s:
    raise SystemExit('v1.9.0 MainActivity recorder UI anchor missing')
s = s.replace(ui_anchor, ui_new, 1)

record_state = '''        if ("recording".equals(state)) {\n            status.setText("● AUFNAHME AKTIV"); status.setTextColor(DANGER);'''
record_new = '''        if ("recording".equals(state)) {\n            status.setText("● STREAMING AKTIV · " + RecordingService.getStreamingStatus()); status.setTextColor(DANGER);\n            if (streamingText != null && (text == null || text.isEmpty()))\n                streamingText.setText("Erstes Live-Fenster nach 25 Sekunden …");'''
if record_state not in s:
    raise SystemExit('v1.9.0 MainActivity recording state anchor missing')
s = s.replace(record_state, record_new, 1)

trans_anchor = '''        } else if ("stopping".equals(state) || "transcribing".equals(state)) {'''
stream_block = '''        } else if ("streaming".equals(state)) {\n            status.setText("● STREAMING AKTIV · " + RecordingService.getStreamingStatus());\n            status.setTextColor(DANGER);\n            if (streamingText != null) streamingText.setText(text == null || text.isEmpty() ? "(noch kein Text)" : text);\n            recordButton.setText("Aufnahme beenden"); recordButton.setEnabled(true); recordButton.setAlpha(1f);\n            modelSpinner.setEnabled(false); wavSwitch.setEnabled(false); aviationSwitch.setEnabled(false);\n            if (loadModelButton != null) loadModelButton.setEnabled(false);\n'''
if trans_anchor not in s:
    raise SystemExit('v1.9.0 MainActivity transcribing anchor missing')
s = s.replace(trans_anchor, stream_block + trans_anchor, 1)

stop_status = '            ui.removeCallbacks(timerTick); status.setText("TRANSKRIBIERE…"); status.setTextColor(ACCENT);\n'
stop_new = '            ui.removeCallbacks(timerTick); status.setText("FINALISIERE STREAM…"); status.setTextColor(ACCENT);\n            if (streamingText != null && text != null && !text.isEmpty()) streamingText.setText(text);\n'
if stop_status not in s:
    raise SystemExit('v1.9.0 MainActivity stop status anchor missing')
s = s.replace(stop_status, stop_new, 1)

done_anchor = '''            status.setText("Fertig · im Verlauf gespeichert"); status.setTextColor(ACCENT);'''
done_new = '''            status.setText("Fertig · Stream im Verlauf gespeichert"); status.setTextColor(ACCENT);\n            if (streamingText != null) streamingText.setText(text == null || text.isEmpty() ? "(Kein Text erkannt)" : text);'''
if done_anchor not in s:
    raise SystemExit('v1.9.0 MainActivity done anchor missing')
s = s.replace(done_anchor, done_new, 1)

main.write_text(s)

# Version bump after previous patches have established v1.8.5.
gradle = Path('SpeechNotes/app/build.gradle')
g = gradle.read_text()
if 'versionName "1.8.5"' not in g:
    raise SystemExit('v1.9.0 expected versionName 1.8.5 not found')
g = g.replace('versionName "1.8.5"', 'versionName "1.9.0"', 1)
# Accept whichever versionCode the v1.8.5 patch produced, but increment exactly once.
import re
m = re.search(r'versionCode\s+(\d+)', g)
if not m:
    raise SystemExit('v1.9.0 versionCode missing')
old_code = int(m.group(1))
g = g[:m.start(1)] + str(old_code + 1) + g[m.end(1):]
gradle.write_text(g)

print('Applied v1.9.0 integrated streaming core + overlap assembly + backlog telemetry + final flush + live UI')
