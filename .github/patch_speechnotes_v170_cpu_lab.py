from pathlib import Path


def rep(path, old, new, count=1):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'Expected block not found in {path}: {old[:160]!r}')
    p.write_text(s.replace(old, new, count))

bridge = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/WhisperBridge.java'
rep(bridge,
    'public static final String BACKEND_BEST = "best";',
    'public static final String BACKEND_BEST = "best";\n    public static final String BACKEND_V86 = "v86";\n    public static final String BACKEND_V90 = "v90";\n    public static final String BACKEND_V90_FLASH = "v90_flash";')
rep(bridge,
'''    private static native String transcribeLoadedRaw(
            String pcmPath, String language, String initialPrompt, int threads, boolean noContext);''',
'''    private static native String transcribeLoadedRaw(
            String pcmPath, String language, String initialPrompt, int threads, boolean noContext,
            int audioCtx, int fastCoreCount);''')
rep(bridge,
'''        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, false));''',
'''        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, false, 0, 0));''')
rep(bridge,
'''        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, true));
    }''',
'''        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, true, 0, 0));
    }

    public static Result transcribeLoadedBenchmarkProfile(String pcmPath, String language,
                                                           String initialPrompt, int threads,
                                                           int audioCtx, int fastCoreCount) {
        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, true,
                audioCtx, fastCoreCount));
    }''')

jni = 'SpeechNotes/app/src/main/cpp/whisper_jni.cpp'
rep(jni,
    '#include <sstream>\n#include "whisper.h"',
    '#include <sstream>\n#include <sched.h>\n#include <unistd.h>\n#include <cstdlib>\n#include "whisper.h"')
rep(jni,
'''    if (mode == "best") {
        // Highest feature set first.''',
'''    if (mode == "v90") {
        if (try_backend_locked(dir, "android_armv9.0_1")) {
            g_backend_mode = mode;
            return true;
        }
        return false;
    }
    if (mode == "v86") {
        if (try_backend_locked(dir, "android_armv8.6_1")) {
            g_backend_mode = mode;
            return true;
        }
        return false;
    }

    if (mode == "best") {
        // Highest feature set first.''')
rep(jni,
'''    std::string backendMode = jstr(env, backendModeJ);
    if (backendMode.empty()) backendMode = "generic";

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ctx && g_model_path == modelPath && g_backend_mode == backendMode) return 0;

    free_model_locked();
    unload_backend_locked();
    if (!load_cpu_backend_locked(backendDir, backendMode)) {
        LOGE("Could not load CPU backend mode=%s dir=%s", backendMode.c_str(), backendDir.c_str());
        return -2;
    }

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    cparams.flash_attn = false;''',
'''    std::string backendMode = jstr(env, backendModeJ);
    if (backendMode.empty()) backendMode = "generic";
    const std::string requestedMode = backendMode;
    bool flashAttn = false;
    const std::string flashSuffix = "_flash";
    if (backendMode.size() > flashSuffix.size() &&
        backendMode.compare(backendMode.size() - flashSuffix.size(), flashSuffix.size(), flashSuffix) == 0) {
        flashAttn = true;
        backendMode.resize(backendMode.size() - flashSuffix.size());
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ctx && g_model_path == modelPath && g_backend_mode == requestedMode) return 0;

    free_model_locked();
    unload_backend_locked();
    if (!load_cpu_backend_locked(backendDir, backendMode)) {
        LOGE("Could not load CPU backend mode=%s dir=%s", backendMode.c_str(), backendDir.c_str());
        return -2;
    }
    g_backend_mode = requestedMode;
    if (flashAttn) g_backend_name += " + flash-attn";

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    cparams.flash_attn = flashAttn;''')

# Add runtime fast-core affinity helper before transcribe JNI.
rep(jni,
'''extern "C" JNIEXPORT jstring JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_transcribeLoadedRaw(''',
'''static long read_cpu_max_freq(int cpu) {
    std::ifstream f("/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/cpuinfo_max_freq");
    long v = 0;
    if (f) f >> v;
    return v;
}

static bool apply_fast_core_affinity(int count, cpu_set_t * oldMask) {
    if (count <= 0 || !oldMask) return false;
    if (sched_getaffinity(0, sizeof(cpu_set_t), oldMask) != 0) return false;
    const int ncpu = (int) sysconf(_SC_NPROCESSORS_CONF);
    if (ncpu <= 0) return false;
    struct CpuInfo { int id; long freq; };
    std::vector<CpuInfo> cpus;
    for (int i = 0; i < ncpu && i < CPU_SETSIZE; ++i) {
        if (!CPU_ISSET(i, oldMask)) continue;
        cpus.push_back({i, read_cpu_max_freq(i)});
    }
    if (cpus.empty()) return false;
    std::sort(cpus.begin(), cpus.end(), [](const CpuInfo & a, const CpuInfo & b) {
        if (a.freq != b.freq) return a.freq > b.freq;
        return a.id > b.id;
    });
    cpu_set_t mask;
    CPU_ZERO(&mask);
    const int use = std::min(count, (int) cpus.size());
    for (int i = 0; i < use; ++i) CPU_SET(cpus[i].id, &mask);
    return sched_setaffinity(0, sizeof(cpu_set_t), &mask) == 0;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_transcribeLoadedRaw(''')
rep(jni,
'''        jint threadsJ,
        jboolean noContextJ) {''',
'''        jint threadsJ,
        jboolean noContextJ,
        jint audioCtxJ,
        jint fastCoreCountJ) {''')
rep(jni,
'''    p.no_context = (noContextJ == JNI_TRUE);
    p.single_segment = false;''',
'''    p.no_context = (noContextJ == JNI_TRUE);
    p.single_segment = false;
    p.audio_ctx = std::max(0, (int) audioCtxJ);''')
rep(jni,
'''    whisper_reset_timings(g_ctx);
    auto whisperStart = std::chrono::steady_clock::now();
    const int rc = whisper_full(g_ctx, p, pcmf32.data(), (int) pcmf32.size());
    const long long whisperMs = elapsed_ms(whisperStart);
    if (rc != 0) return env->NewStringUTF("[Whisper-Inferenz fehlgeschlagen]");''',
'''    cpu_set_t oldMask;
    const bool affinityChanged = apply_fast_core_affinity((int) fastCoreCountJ, &oldMask);
    whisper_reset_timings(g_ctx);
    auto whisperStart = std::chrono::steady_clock::now();
    const int rc = whisper_full(g_ctx, p, pcmf32.data(), (int) pcmf32.size());
    const long long whisperMs = elapsed_ms(whisperStart);
    if (affinityChanged) sched_setaffinity(0, sizeof(cpu_set_t), &oldMask);
    if (rc != 0) return env->NewStringUTF("[Whisper-Inferenz fehlgeschlagen]");''')

bench = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/BenchmarkActivity.java'
rep(bench,
    'private static final String[] ARM_STEPS = {"Generic ARMv8.0 · 8 Threads", "ARM Best · 8 Threads"};',
'''private static final String[] ARM_STEPS = {"Generic ARMv8.0 · 8 Threads", "ARM Best · 8 Threads"};
    private static final String[] CPU_LAB_STEPS = {
            "1 · ARMv9 baseline · 8T · ctx1500",
            "2 · ARMv8.6 · 8T · ctx1500",
            "3 · ARMv9 · 6T · ctx1500",
            "4 · ARMv9 · 7T · ctx1500",
            "5 · ARMv9 · 8T · FlashAttn",
            "6 · ARMv9 · 8T · ctx1344",
            "7 · ARMv9 · 8T · ctx1280",
            "8 · ARMv9 · 8T · FlashAttn + ctx1280",
            "9 · Fast4 · 4T · FlashAttn + ctx1280"
    };''')
rep(bench,
    'new String[]{"Threading · 2 / 4 / 6 / 8", "CPU Backend · Generic vs ARM Best"}',
    'new String[]{"Threading · 2 / 4 / 6 / 8", "CPU Backend · Generic vs ARM Best", "CPU Performance Lab · 9 Runs"}')
rep(bench,
'''    private void configureSuite(int position) {
        boolean arm = position == 1;
        activeSteps = arm ? ARM_STEPS : THREAD_STEPS;
        if (suiteDescription != null) suiteDescription.setText(arm
                ? "Generic ARMv8.0 vs höchste sicher unterstützte GGML-ARM-Variante · jeweils 8 Threads"
                : "2 → 4 → 6 → 8 Threads · Generic CPU · Deutsch · no-context");
        if (startButton != null) startButton.setText(arm ? "ARM-Benchmark starten" : "Thread-Benchmark starten");''',
'''    private void configureSuite(int position) {
        boolean arm = position == 1;
        boolean lab = position == 2;
        activeSteps = lab ? CPU_LAB_STEPS : (arm ? ARM_STEPS : THREAD_STEPS);
        if (suiteDescription != null) suiteDescription.setText(lab
                ? "Large-v3-Turbo Q5_0 · identische 25 s · ARM-only · Backend/Threads/FlashAttn/audio_ctx/Affinity"
                : (arm ? "Generic ARMv8.0 vs höchste sicher unterstützte GGML-ARM-Variante · jeweils 8 Threads"
                : "2 → 4 → 6 → 8 Threads · Generic CPU · Deutsch · no-context"));
        if (startButton != null) startButton.setText(lab ? "CPU Performance Lab starten" : (arm ? "ARM-Benchmark starten" : "Thread-Benchmark starten"));''')
rep(bench,
'''        final boolean armSuite = suiteSpinner.getSelectedItemPosition() == 1;
        activeSteps = armSuite ? ARM_STEPS : THREAD_STEPS;''',
'''        final int suite = suiteSpinner.getSelectedItemPosition();
        activeSteps = suite == 2 ? CPU_LAB_STEPS : (suite == 1 ? ARM_STEPS : THREAD_STEPS);''')
rep(bench,
    'new Thread(() -> runSuite(wav, model, armSuite), "speech-benchmark").start();',
    'new Thread(() -> runSuite(wav, model, suite), "speech-benchmark").start();')
rep(bench,
'''    private void runSuite(File wav, String model, boolean armSuite) {
        File pcm = new File(getCacheDir(), "benchmark-input.pcm");
        try {
            WavBenchmarkUtils.Info info = WavBenchmarkUtils.wavToPcm16kMono(wav, pcm);
            File modelFile = ModelManager.ensureModel(this, model);
            ArrayList<RunResult> runResults = armSuite
                    ? runArmSuite(modelFile, pcm, info.durationMs)
                    : runThreadSuite(modelFile, pcm, info.durationMs);''',
'''    private void runSuite(File wav, String model, int suite) {
        File pcm = new File(getCacheDir(), "benchmark-input.pcm");
        try {
            WavBenchmarkUtils.Info info = WavBenchmarkUtils.wavToPcm16kMono(wav, pcm);
            File modelFile = ModelManager.ensureModel(this, model);
            ArrayList<RunResult> runResults = suite == 2
                    ? runCpuLab(modelFile, pcm, info.durationMs)
                    : (suite == 1 ? runArmSuite(modelFile, pcm, info.durationMs)
                    : runThreadSuite(modelFile, pcm, info.durationMs));''')
rep(bench,
'''        return list;
    }

    private void updateProgressStart''',
'''        return list;
    }

    private ArrayList<RunResult> runCpuLab(File modelFile, File pcm, long audioMs) {
        ArrayList<RunResult> list = new ArrayList<>();
        String[] labels = {
                "ARMv9 baseline · 8T · ctx1500",
                "ARMv8.6 · 8T · ctx1500",
                "ARMv9 · 6T · ctx1500",
                "ARMv9 · 7T · ctx1500",
                "ARMv9 · 8T · FlashAttn",
                "ARMv9 · 8T · ctx1344",
                "ARMv9 · 8T · ctx1280",
                "ARMv9 · 8T · FlashAttn + ctx1280",
                "Fast4 · 4T · FlashAttn + ctx1280"
        };
        String[] modes = {
                WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V86,
                WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90_FLASH, WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90_FLASH,
                WhisperBridge.BACKEND_V90_FLASH
        };
        int[] threads = {8, 8, 6, 7, 8, 8, 8, 8, 4};
        int[] audioCtx = {0, 0, 0, 0, 0, 1344, 1280, 1280, 1280};
        int[] fastCores = {0, 0, 0, 0, 0, 0, 0, 0, 4};
        for (int i = 0; i < labels.length; i++) {
            int step = i;
            String label = labels[i];
            ui.post(() -> updateProgressStart(step, label));
            long loadMs = WhisperBridge.loadModel(modelFile.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, modes[i]);
            if (loadMs < 0) throw new IllegalStateException(label + " Backend konnte nicht geladen werden (" + loadMs + ")");
            String backend = WhisperBridge.currentBackendName();
            long start = SystemClock.elapsedRealtime();
            WhisperBridge.Result r = WhisperBridge.transcribeLoadedBenchmarkProfile(
                    pcm.getAbsolutePath(), "de", "", threads[i], audioCtx[i], fastCores[i]);
            long wall = SystemClock.elapsedRealtime() - start;
            RunResult rr = new RunResult(label, backend, threads[i], wall, audioMs, r);
            list.add(rr);
            ui.post(() -> completeStep(step, rr));
        }
        return list;
    }

    private void updateProgressStart''')

print('Applied v1.7 ARM CPU performance lab patch')
