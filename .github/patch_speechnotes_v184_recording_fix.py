from pathlib import Path


def rep(path, old, new, count=1):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'Expected block not found in {path}: {old[:180]!r}')
    p.write_text(s.replace(old, new, count))

# v1.8.4: keep the proven Q4/ARMv9/4T/ctx1280 runtime, but centralize the
# productive configuration and instrument the recording lifecycle.
main = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/MainActivity.java')
s = main.read_text()
s = s.replace('    private SharedPreferences prefs;\n',
              '    private SharedPreferences prefs;\n    private String startupDiagnostics = "";\n', 1)
s = s.replace('''        prefs = getSharedPreferences("settings", MODE_PRIVATE);\n        getWindow().setStatusBarColor(BG);''',
'''        prefs = getSharedPreferences("settings", MODE_PRIVATE);\n        startupDiagnostics = CrashDiagnostics.summary(this);\n        CrashDiagnostics.mark(this, "ui:onCreate");\n        getWindow().setStatusBarColor(BG);''', 1)
s = s.replace('''        root.addView(text("Lokale Whisper-Diktate · vollständig offline", 14, MUTED, false));\n        root.addView(space(22));''',
'''        root.addView(text("Lokale Whisper-Diktate · vollständig offline", 14, MUTED, false));\n        if (startupDiagnostics != null && !startupDiagnostics.isEmpty()) {\n            root.addView(space(10));\n            TextView crashInfo = text("Letzter Runtime-Status (antippen = kopieren)\\n" + startupDiagnostics, 11, MUTED, false);\n            crashInfo.setPadding(dp(12), dp(10), dp(12), dp(10));\n            crashInfo.setBackground(roundRect(CARD, 14));\n            crashInfo.setOnClickListener(v -> copyText(startupDiagnostics));\n            root.addView(crashInfo, matchWrap());\n        }\n        root.addView(space(22));''', 1)
s = s.replace('WhisperBridge.BACKEND_V90', 'SpeechRuntimeConfig.BACKEND')
s = s.replace('''    private void toggleRecording() {\n''',
'''    private void toggleRecording() {\n        CrashDiagnostics.mark(this, RecordingService.isRecording() ? "ui:stop_pressed" : "ui:record_pressed model=" + selectedModelName());\n''', 1)
s = s.replace('''        new Thread(() -> {\n            try {\n                long extractStart = SystemClock.elapsedRealtime();''',
'''        new Thread(() -> {\n            try {\n                CrashDiagnostics.mark(this, "model_load:begin model=" + model);\n                long extractStart = SystemClock.elapsedRealtime();''', 1)
s = s.replace('''                long loadMs = WhisperBridge.loadModel(file.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, SpeechRuntimeConfig.BACKEND);\n                if (loadMs < 0) throw new IllegalStateException("Whisper-Context konnte nicht initialisiert werden");''',
'''                long loadMs = WhisperBridge.loadModel(file.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, SpeechRuntimeConfig.backendFor(model));\n                if (loadMs < 0) throw new IllegalStateException("Whisper-Context konnte nicht initialisiert werden");\n                CrashDiagnostics.mark(this, "model_load:ready model=" + model + " backend=" + WhisperBridge.currentBackendName() + " load_ms=" + loadMs);''', 1)
main.write_text(s)

service = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/RecordingService.java')
s = service.read_text()
s = s.replace('    private String model = "base-q5_1";', '    private String model = SpeechRuntimeConfig.DEFAULT_MODEL;', 1)
s = s.replace('            if (model == null) model = "base-q5_1";', '            if (model == null) model = SpeechRuntimeConfig.DEFAULT_MODEL;', 1)
s = s.replace('''    @Override public int onStartCommand(Intent intent, int flags, int startId) {\n        if (intent == null) return START_NOT_STICKY;''',
'''    @Override public int onStartCommand(Intent intent, int flags, int startId) {\n        CrashDiagnostics.mark(this, "service:onStartCommand action=" + (intent == null ? "null" : intent.getAction()));\n        if (intent == null) return START_NOT_STICKY;''', 1)

start = s.index('    private void startRecording() {')
end = s.index('    private void acquireWakeLock()', start)
new_start = r'''    private void startRecording() {
        CrashDiagnostics.mark(this, "record:start_enter model=" + model);
        int min = AudioRecord.getMinBufferSize(SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        int bufferSize = Math.max(min, SAMPLE_RATE * 2);
        try {
            // Runtime permission is revocable at any time. Check it in the service itself
            // before touching AudioRecord instead of relying only on the Activity flow.
            if (checkSelfPermission(android.Manifest.permission.RECORD_AUDIO)
                    != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                throw new SecurityException("Mikrofonberechtigung RECORD_AUDIO fehlt");
            }

            // Android 16 robustness: satisfy the foreground-service contract immediately,
            // before model validation / AudioRecord setup can do any non-trivial work.
            Intent stopIntent = new Intent(this, RecordingService.class).setAction(ACTION_STOP);
            PendingIntent stopPending = PendingIntent.getService(this, 2, stopIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            Notification preparing = buildNotification("Aufnahme wird vorbereitet",
                    modelLabel(model) + " · " + SpeechRuntimeConfig.runtimeLabel(model), stopPending, true);
            if (Build.VERSION.SDK_INT >= 29) {
                startForeground(NOTIFICATION_ID, preparing, ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE);
            } else {
                startForeground(NOTIFICATION_ID, preparing);
            }
            CrashDiagnostics.mark(this, "record:foreground_started");

            File modelFile = ModelManager.ensureModel(this, model);
            CrashDiagnostics.mark(this, "record:model_file_ready bytes=" + modelFile.length());
            String backend = SpeechRuntimeConfig.backendFor(model);
            if (!WhisperBridge.isModelLoaded(modelFile.getAbsolutePath(), backend)) {
                throw new IllegalStateException("Geladener Modell-Context passt nicht zum Recording-Runtime-Profil.");
            }
            CrashDiagnostics.mark(this, "record:model_context_ready backend=" + WhisperBridge.currentBackendName());

            audioRecord = new AudioRecord(MediaRecorder.AudioSource.VOICE_RECOGNITION,
                    SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT, bufferSize);
            CrashDiagnostics.mark(this, "record:audiorecord_created state=" + audioRecord.getState() + " min=" + min + " buffer=" + bufferSize);
            if (audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
                throw new IllegalStateException("AudioRecord konnte nicht initialisiert werden");
            }

            pcmFile = new File(getCacheDir(), "recording-" + System.currentTimeMillis() + ".pcm");
            stopRequested.set(false);
            startedWall = System.currentTimeMillis();
            startedElapsed = SystemClock.elapsedRealtime();
            acquireWakeLock();
            CrashDiagnostics.mark(this, "record:wake_lock_ready");
            audioRecord.startRecording();
            CrashDiagnostics.mark(this, "record:audiorecord_started recording_state=" + audioRecord.getRecordingState());
            if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                throw new IllegalStateException("AudioRecord ist nach startRecording nicht im RECORDING-State");
            }
            recording = true;

            NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            nm.notify(NOTIFICATION_ID, buildNotification("Aufnahme läuft", "Tippen zum Öffnen", stopPending, true));
            broadcast("recording", null);

            final int finalBufferSize = bufferSize;
            recordThread = new Thread(() -> captureLoop(finalBufferSize), "speech-capture");
            recordThread.start();
            CrashDiagnostics.mark(this, "record:capture_thread_started");
        } catch (Throwable t) {
            CrashDiagnostics.markError(this, "record:start_failed", t);
            recording = false;
            releaseRecorder();
            releaseWakeLock();
            broadcast("error", t.getClass().getSimpleName() + ": " + t.getMessage());
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
        }
    }

'''
s = s[:start] + new_start + s[end:]

s = s.replace('''    private void captureLoop(int bufferSize) {\n        byte[] buffer = new byte[bufferSize];''',
'''    private void captureLoop(int bufferSize) {\n        CrashDiagnostics.mark(this, "record:capture_loop_enter");\n        byte[] buffer = new byte[bufferSize];''', 1)
s = s.replace('''        } catch (Throwable t) {\n            broadcast("error", t.getMessage());\n        } finally {\n            try { audioRecord.stop(); } catch (Throwable ignored) { }''',
'''        } catch (Throwable t) {\n            CrashDiagnostics.markError(this, "record:capture_failed", t);\n            broadcast("error", t.getMessage());\n        } finally {\n            CrashDiagnostics.mark(this, "record:capture_stopping");\n            try { audioRecord.stop(); } catch (Throwable ignored) { }''', 1)
s = s.replace('''    private void beginTranscription() {\n        transcribing = true;''',
'''    private void beginTranscription() {\n        CrashDiagnostics.mark(this, "transcribe:begin model=" + model);\n        transcribing = true;''', 1)
# Replace the v1.8.3 productive inference block with the centralized runtime config.
old = '''                int cores = Math.max(1, Runtime.getRuntime().availableProcessors());\n                int threads = model.startsWith("large-v3-turbo")\n                        ? Math.min(4, cores)\n                        : Math.max(2, Math.min(6, Math.max(2, cores - 2)));\n                String prompt = aviationPrompt ? AviationVocabulary.PROMPT : "";\n                profile = WhisperBridge.transcribeLoadedProfile(\n                        pcmFile.getAbsolutePath(), "de", prompt, threads,\n                        model.startsWith("large-v3-turbo") ? 1280 : 0, 0);'''
new = '''                int threads = SpeechRuntimeConfig.threadsFor(model);\n                String prompt = aviationPrompt ? AviationVocabulary.PROMPT : "";\n                CrashDiagnostics.mark(this, "transcribe:inference_enter " + SpeechRuntimeConfig.runtimeLabel(model));\n                profile = SpeechRuntimeConfig.transcribe(pcmFile.getAbsolutePath(), model, prompt);\n                CrashDiagnostics.mark(this, "transcribe:inference_done native_ms=" + profile.nativeTotalMs + " encode_ms=" + profile.encodeTotalMs);'''
if old not in s:
    raise SystemExit('v1.8.4 inference block not found')
s = s.replace(old, new, 1)
s = s.replace('''            } catch (Throwable t) {\n                resultText = "[Transkriptionsfehler: " + t.getMessage() + "]";''',
'''            } catch (Throwable t) {\n                CrashDiagnostics.markError(this, "transcribe:failed", t);\n                resultText = "[Transkriptionsfehler: " + t.getMessage() + "]";''', 1)
s = s.replace('''                transcribing = false;\n                broadcast("done", resultText);''',
'''                transcribing = false;\n                CrashDiagnostics.mark(this, "transcribe:done words=" + words);\n                broadcast("done", resultText);''', 1)
service.write_text(s)

print('Applied v1.8.4 recording crash diagnostics + centralized runtime pipeline fix')
