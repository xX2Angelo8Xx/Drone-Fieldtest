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
import java.io.InputStream;
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
            startRecording();
        }
        return START_NOT_STICKY;
    }

    private void startRecording() {
        int min = AudioRecord.getMinBufferSize(SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        int bufferSize = Math.max(min, SAMPLE_RATE * 2);
        try {
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

            PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SpeechNotes:Recording");
            wakeLock.acquire(6L * 60L * 60L * 1000L);

            audioRecord.startRecording();
            broadcast("recording", null);

            final int finalBufferSize = bufferSize;
            recordThread = new Thread(() -> captureLoop(finalBufferSize), "speech-capture");
            recordThread.start();
        } catch (Throwable t) {
            recording = false;
            releaseAudio();
            broadcast("error", t.getMessage());
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
        }
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
            releaseAudio();
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
        broadcast("transcribing", null);
        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        nm.notify(NOTIFICATION_ID, buildNotification("Transkription läuft", modelLabel(model), null, false));

        final long durationMs = Math.max(0, System.currentTimeMillis() - startedWall);
        worker.execute(() -> {
            String result;
            long inferenceStart = SystemClock.elapsedRealtime();
            try {
                File modelFile = ensureModel(model);
                int threads = Math.max(2, Math.min(6, Runtime.getRuntime().availableProcessors() - 2));
                result = WhisperBridge.transcribePcm16(modelFile.getAbsolutePath(),
                        pcmFile.getAbsolutePath(), "auto", threads);
                if (result == null) result = "";
                result = result.trim();
            } catch (Throwable t) {
                result = "[Transkriptionsfehler: " + t.getMessage() + "]";
            }
            long inferenceMs = SystemClock.elapsedRealtime() - inferenceStart;

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

            int words = countWords(result);
            new TranscriptDb(this).insert(startedWall, durationMs, inferenceMs,
                    model, result, words, wavPath);

            if (pcmFile != null) pcmFile.delete();
            transcribing = false;
            broadcast("done", result);
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
        });
    }

    private File ensureModel(String modelName) throws IOException {
        String assetName = "models/ggml-" + modelName + ".bin";
        File dir = new File(getFilesDir(), "models");
        if (!dir.exists() && !dir.mkdirs()) throw new IOException("Modellordner konnte nicht erstellt werden");
        File dst = new File(dir, "ggml-" + modelName + ".bin");
        if (dst.exists() && dst.length() > 10_000_000) return dst;

        File tmp = new File(dir, dst.getName() + ".tmp");
        try (InputStream in = new BufferedInputStream(getAssets().open(assetName), 1024 * 1024);
             BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(tmp), 1024 * 1024)) {
            byte[] buffer = new byte[1024 * 1024];
            int n;
            while ((n = in.read(buffer)) >= 0) out.write(buffer, 0, n);
        }
        if (!tmp.renameTo(dst)) throw new IOException("Modell konnte nicht finalisiert werden");
        return dst;
    }

    private static int countWords(String text) {
        String t = text == null ? "" : text.trim();
        return t.isEmpty() ? 0 : t.split("\\s+").length;
    }

    private void releaseAudio() {
        if (audioRecord != null) {
            try { audioRecord.release(); } catch (Throwable ignored) { }
            audioRecord = null;
        }
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
        channel.setDescription("Zeigt eine laufende Sprachaufnahme an");
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
        if (m.startsWith("tiny")) return "Whisper Tiny Q5_1";
        if (m.startsWith("small")) return "Whisper Small Q5_1";
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
        releaseAudio();
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
