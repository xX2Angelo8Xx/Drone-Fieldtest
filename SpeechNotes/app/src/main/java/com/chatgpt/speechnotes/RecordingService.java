package com.chatgpt.speechnotes;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.graphics.drawable.Icon;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
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
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
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

    private static volatile boolean recording = false;
    private static volatile boolean transcribing = false;
    private static volatile long startedElapsed = 0L;

    private final AtomicBoolean stopRequested = new AtomicBoolean(false);
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private AudioRecord audioRecord;
    private File pcmFile;
    private Thread recordThread;
    private String model = "base-q5_1";
    private boolean saveWav = false;
    private boolean aviationPrompt = false;
    private long startedWall;
    private PowerManager.WakeLock wakeLock;

    public static boolean isRecording() { return recording; }
    public static boolean isTranscribing() { return transcribing; }
    public static long getStartedElapsed() { return startedElapsed; }

    @Override public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) return START_NOT_STICKY;
        String action = intent.getAction();
        if (ACTION_STOP.equals(action)) {
            requestStop();
            return START_NOT_STICKY;
        }
        if (ACTION_START.equals(action) && !recording && !transcribing) {
            model = intent.getStringExtra(EXTRA_MODEL);
            if (model == null) model = "base-q5_1";
            saveWav = intent.getBooleanExtra(EXTRA_SAVE_WAV, false);
            aviationPrompt = intent.getBooleanExtra(EXTRA_AVIATION_PROMPT, false);
            startRecording();
        }
        return START_NOT_STICKY;
    }

    private void startRecording() {
        int min = AudioRecord.getMinBufferSize(SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        int bufferSize = Math.max(min, SAMPLE_RATE * 2);
        try {
            File modelFile = ModelManager.ensureModel(this, model);
            if (!WhisperBridge.isModelLoaded(modelFile.getAbsolutePath())) {
                throw new IllegalStateException("Bitte das ausgewählte Modell zuerst laden.");
            }

            audioRecord = new AudioRecord(MediaRecorder.AudioSource.VOICE_RECOGNITION,
                    SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT, bufferSize);
            if (audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
                throw new IllegalStateException("AudioRecord konnte nicht initialisiert werden");
            }

            pcmFile = new File(getCacheDir(), "recording-" + System.currentTimeMillis() + ".pcm");
            stopRequested.set(false);
            startedWall = System.currentTimeMillis();
            startedElapsed = SystemClock.elapsedRealtime();
            recording = true;

            Intent stopIntent = new Intent(this, RecordingService.class).setAction(ACTION_STOP);
            PendingIntent stopPending = PendingIntent.getService(this, 2, stopIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            Notification notification = buildNotification("Aufnahme läuft", "Tippen zum Öffnen", stopPending, true);
            if (Build.VERSION.SDK_INT >= 29) {
                startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE);
            } else {
                startForeground(NOTIFICATION_ID, notification);
            }

            acquireWakeLock();
            audioRecord.startRecording();
            broadcast("recording", null);

            final int finalBufferSize = bufferSize;
            recordThread = new Thread(() -> captureLoop(finalBufferSize), "speech-capture");
            recordThread.start();
        } catch (Throwable t) {
            recording = false;
            releaseRecorder();
            releaseWakeLock();
            broadcast("error", t.getMessage());
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
        }
    }

    private void acquireWakeLock() {
        if (wakeLock != null && wakeLock.isHeld()) return;
        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SpeechNotes:RecordAndTranscribe");
        wakeLock.setReferenceCounted(false);
        wakeLock.acquire(6L * 60L * 60L * 1000L);
    }

    private void captureLoop(int bufferSize) {
        byte[] buffer = new byte[bufferSize];
        try (BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(pcmFile), bufferSize * 2)) {
            while (!stopRequested.get()) {
                int n = audioRecord.read(buffer, 0, buffer.length);
                if (n > 0) out.write(buffer, 0, n);
                else if (n < 0) throw new IOException("AudioRecord read error: " + n);
            }
        } catch (Throwable t) {
            broadcast("error", t.getMessage());
        } finally {
            try { audioRecord.stop(); } catch (Throwable ignored) { }
            recording = false;
            releaseRecorder();
            beginTranscription();
        }
    }

    private synchronized void requestStop() {
        if (recording) {
            stopRequested.set(true);
            broadcast("stopping", null);
        }
    }

    private void beginTranscription() {
        transcribing = true;
        acquireWakeLock();
        broadcast("transcribing", null);
        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        String promptLabel = aviationPrompt ? " · Aviation Prompt" : " · Pure";
        nm.notify(NOTIFICATION_ID, buildNotification("Transkription läuft", modelLabel(model) + " · Deutsch" + promptLabel, null, true));

        final long durationMs = Math.max(0, System.currentTimeMillis() - startedWall);
        worker.execute(() -> {
            String resultText;
            WhisperBridge.Result profile = new WhisperBridge.Result();
            long modelLoadMs = WhisperBridge.currentModelLoadMs();
            try {
                File modelFile = ModelManager.ensureModel(this, model);
                if (!WhisperBridge.isModelLoaded(modelFile.getAbsolutePath())) {
                    throw new IllegalStateException("Geladener Modell-Context ist nicht mehr verfügbar.");
                }
                int threads = Math.max(2, Math.min(6, Runtime.getRuntime().availableProcessors() - 2));
                String prompt = aviationPrompt ? AviationVocabulary.PROMPT : "";
                profile = WhisperBridge.transcribeLoaded(
                        pcmFile.getAbsolutePath(), "de", prompt, threads);
                resultText = profile.text == null ? "" : profile.text.trim();
            } catch (Throwable t) {
                resultText = "[Transkriptionsfehler: " + t.getMessage() + "]";
            }

            String wavPath = null;
            if (saveWav) {
                try {
                    File wavDir = new File(getFilesDir(), "recordings");
                    if (!wavDir.exists()) wavDir.mkdirs();
                    File wav = new File(wavDir, "SpeechNotes-" + startedWall + ".wav");
                    writeWav(pcmFile, wav, SAMPLE_RATE);
                    wavPath = wav.getAbsolutePath();
                } catch (Throwable ignored) { }
            }

            try {
                int words = countWords(resultText);
                new TranscriptDb(this).insert(
                        startedWall, durationMs, profile, modelLoadMs,
                        model, resultText, words, wavPath);
                if (pcmFile != null) pcmFile.delete();
                transcribing = false;
                broadcast("done", resultText);
            } finally {
                releaseWakeLock();
                stopForeground(STOP_FOREGROUND_REMOVE);
                stopSelf();
            }
        });
    }

    private static int countWords(String text) {
        String t = text == null ? "" : text.trim();
        return t.isEmpty() ? 0 : t.split("\\s+").length;
    }

    private void releaseRecorder() {
        if (audioRecord != null) {
            try { audioRecord.release(); } catch (Throwable ignored) { }
            audioRecord = null;
        }
    }

    private void releaseWakeLock() {
        if (wakeLock != null && wakeLock.isHeld()) {
            try { wakeLock.release(); } catch (Throwable ignored) { }
        }
        wakeLock = null;
    }

    private void broadcast(String state, String text) {
        Intent i = new Intent(ACTION_STATE).setPackage(getPackageName());
        i.putExtra(EXTRA_STATE, state);
        if (text != null) i.putExtra(EXTRA_TEXT, text);
        sendBroadcast(i);
    }

    private void createNotificationChannel() {
        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        NotificationChannel channel = new NotificationChannel(CHANNEL_ID,
                "Aufnahme", NotificationManager.IMPORTANCE_LOW);
        channel.setDescription("Zeigt Aufnahme und lokale Transkription an");
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

    private static String modelLabel(String m) {
        if (m.startsWith("large-v3-turbo")) return "Whisper Large-v3-Turbo Q5_0";
        return "Whisper Base Q5_1";
    }

    @Override public void onTaskRemoved(Intent rootIntent) {
        if (recording) requestStop();
        super.onTaskRemoved(rootIntent);
    }

    @Override public void onDestroy() {
        if (recording) {
            stopRequested.set(true);
            if (recordThread != null) {
                try { recordThread.join(1200); } catch (InterruptedException ignored) { }
            }
        }
        releaseRecorder();
        if (!transcribing) releaseWakeLock();
        worker.shutdown();
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }

    private static void writeWav(File pcm, File wav, int sampleRate) throws IOException {
        long dataSize = pcm.length();
        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(pcm));
             BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(wav))) {
            writeAscii(out, "RIFF");
            writeLe32(out, 36 + dataSize);
            writeAscii(out, "WAVE");
            writeAscii(out, "fmt ");
            writeLe32(out, 16);
            writeLe16(out, 1);
            writeLe16(out, 1);
            writeLe32(out, sampleRate);
            writeLe32(out, sampleRate * 2L);
            writeLe16(out, 2);
            writeLe16(out, 16);
            writeAscii(out, "data");
            writeLe32(out, dataSize);
            byte[] buffer = new byte[128 * 1024];
            int n;
            while ((n = in.read(buffer)) >= 0) out.write(buffer, 0, n);
        }
    }

    private static void writeAscii(BufferedOutputStream out, String s) throws IOException {
        out.write(s.getBytes(java.nio.charset.StandardCharsets.US_ASCII));
    }
    private static void writeLe16(BufferedOutputStream out, long v) throws IOException {
        out.write((int)(v & 0xff)); out.write((int)((v >> 8) & 0xff));
    }
    private static void writeLe32(BufferedOutputStream out, long v) throws IOException {
        out.write((int)(v & 0xff)); out.write((int)((v >> 8) & 0xff));
        out.write((int)((v >> 16) & 0xff)); out.write((int)((v >> 24) & 0xff));
    }
}
