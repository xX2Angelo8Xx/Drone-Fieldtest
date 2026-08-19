from pathlib import Path


def rep(path, old, new, count=1):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'Expected block not found in {path}: {old[:180]!r}')
    p.write_text(s.replace(old, new, count))

bridge = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/WhisperBridge.java'
rep(bridge,
'''    public static native long currentModelLoadMs();
    public static native String currentBackendName();''',
'''    public static native long currentModelLoadMs();
    public static native String currentBackendName();
    public static native String currentBackendMode();
    public static native String currentModelPath();''')

jni = 'SpeechNotes/app/src/main/cpp/whisper_jni.cpp'
rep(jni,
'''extern "C" JNIEXPORT jstring JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_currentBackendName(JNIEnv *env, jclass) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return env->NewStringUTF(g_backend_name.c_str());
}
''',
'''extern "C" JNIEXPORT jstring JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_currentBackendName(JNIEnv *env, jclass) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return env->NewStringUTF(g_backend_name.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_currentBackendMode(JNIEnv *env, jclass) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return env->NewStringUTF(g_backend_mode.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_currentModelPath(JNIEnv *env, jclass) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return env->NewStringUTF(g_model_path.c_str());
}
''')

main = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/MainActivity.java'
rep(main,
'''        boolean loaded = WhisperBridge.isModelLoaded(selectedModelFile().getAbsolutePath(), SpeechRuntimeConfig.BACKEND);''',
'''        boolean loaded = SpeechRuntimeSession.isReady(selectedModelFile(), selectedModelName());''')

p = Path(main)
s = p.read_text()
old = '''                long extractStart = SystemClock.elapsedRealtime();
                File file = ModelManager.ensureModel(this, model);
                long extractMs = SystemClock.elapsedRealtime() - extractStart;
                long loadMs = WhisperBridge.loadModel(file.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, SpeechRuntimeConfig.backendFor(model));
                if (loadMs < 0) throw new IllegalStateException("Whisper-Context konnte nicht initialisiert werden");
                CrashDiagnostics.mark(this, "model_load:ready model=" + model + " backend=" + WhisperBridge.currentBackendName() + " load_ms=" + loadMs);'''
new = '''                long prepareStart = SystemClock.elapsedRealtime();
                long loadMs = SpeechRuntimeSession.ensureLoaded(this, model);
                long prepareMs = SystemClock.elapsedRealtime() - prepareStart;
                SpeechRuntimeSession.Snapshot runtime = SpeechRuntimeSession.snapshot();
                CrashDiagnostics.mark(this, "model_load:ready " + runtime.describe());'''
if old not in s:
    raise SystemExit('v1.8.5 MainActivity model-load block not found')
s = s.replace(old, new, 1)

# Keep this replacement deliberately token-level: preceding patches may alter
# whitespace/labels around the status text, but the stale variable must never
# survive after extractMs was replaced by prepareMs.
if 'formatMsPrecise(extractMs)' not in s:
    raise SystemExit('v1.8.5 MainActivity stale extractMs status token not found')
s = s.replace('formatMsPrecise(extractMs)', 'formatMsPrecise(prepareMs)', 1)
s = s.replace('"Bereit · Datei "', '"Bereit · Prepare "', 1)

# Android 14+ requires an explicit export policy for context-registered
# receivers. This receiver only consumes RecordingService state from this app,
# so keep it non-exported on every supported Android version via AndroidX Core.
if 'import androidx.core.content.ContextCompat;' not in s:
    import_anchor = 'import android.widget.Toast;\n\n'
    if import_anchor not in s:
        raise SystemExit('v1.8.5 MainActivity AndroidX import anchor not found')
    s = s.replace(import_anchor,
                  'import android.widget.Toast;\n\nimport androidx.core.content.ContextCompat;\n\n', 1)

receiver_old = '''        IntentFilter filter = new IntentFilter(RecordingService.ACTION_STATE);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(stateReceiver, filter, RECEIVER_NOT_EXPORTED);
        else registerReceiver(stateReceiver, filter);'''
receiver_new = '''        IntentFilter filter = new IntentFilter(RecordingService.ACTION_STATE);
        ContextCompat.registerReceiver(this, stateReceiver, filter,
                ContextCompat.RECEIVER_NOT_EXPORTED);'''
if receiver_old not in s:
    raise SystemExit('v1.8.5 MainActivity receiver registration block not found')
s = s.replace(receiver_old, receiver_new, 1)
p.write_text(s)

# ContextCompat.registerReceiver is the Android-recommended compatibility path
# for explicit receiver export semantics while retaining minSdk 28.
gradle_file = 'SpeechNotes/app/build.gradle'
p = Path(gradle_file)
s = p.read_text()
core_dependency = "implementation 'androidx.core:core:1.13.1'"
if core_dependency not in s:
    if 'dependencies {' in s:
        raise SystemExit('v1.8.5 unexpected pre-existing dependencies block; review before modifying')
    s = s.rstrip() + "\n\ndependencies {\n    " + core_dependency + "\n}\n"
p.write_text(s)

service = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/RecordingService.java'
p = Path(service)
s = p.read_text()
old = '''            File modelFile = ModelManager.ensureModel(this, model);
            CrashDiagnostics.mark(this, "record:model_file_ready bytes=" + modelFile.length());
            String backend = SpeechRuntimeConfig.backendFor(model);
            if (!WhisperBridge.isModelLoaded(modelFile.getAbsolutePath(), backend)) {
                throw new IllegalStateException("Geladener Modell-Context passt nicht zum Recording-Runtime-Profil.");
            }
            CrashDiagnostics.mark(this, "record:model_context_ready backend=" + WhisperBridge.currentBackendName());'''
new = '''            File modelFile = ModelManager.ensureModel(this, model);
            CrashDiagnostics.mark(this, "record:model_file_ready bytes=" + modelFile.length());
            long restoredLoadMs = SpeechRuntimeSession.ensureLoaded(this, model);
            SpeechRuntimeSession.Snapshot runtime = SpeechRuntimeSession.snapshot();
            CrashDiagnostics.mark(this, "record:model_context_ready restored_load_ms=" + restoredLoadMs + " " + runtime.describe());'''
if old not in s:
    raise SystemExit('v1.8.5 RecordingService start guard block not found')
s = s.replace(old, new, 1)

# v1.8.3 converts the historical BEST guard to V90 before the v1.8.4 patch runs.
old2 = '''                File modelFile = ModelManager.ensureModel(this, model);
                if (!WhisperBridge.isModelLoaded(modelFile.getAbsolutePath(), WhisperBridge.BACKEND_V90)) {
                    throw new IllegalStateException("Geladener Modell-Context ist nicht mehr verfügbar.");
                }'''
new2 = '''                File modelFile = ModelManager.ensureModel(this, model);
                long restoredLoadMs = SpeechRuntimeSession.ensureLoaded(this, model);
                CrashDiagnostics.mark(this, "transcribe:runtime_ready restored_load_ms=" + restoredLoadMs
                        + " " + SpeechRuntimeSession.snapshot().describe());'''
if old2 not in s:
    raise SystemExit('v1.8.5 RecordingService transcription guard block not found')
s = s.replace(old2, new2, 1)
p.write_text(s)

print('Applied v1.8.5 unified native runtime session + self-healing recording guards + safe receiver registration')
