from pathlib import Path


def rep(path, old, new, count=1):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'Expected block not found in {path}: {old[:120]!r}')
    p.write_text(s.replace(old, new, count))

# Java bridge: expose Vulkan mode.
bridge = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/WhisperBridge.java'
rep(bridge,
    'public static final String BACKEND_BEST = "best";',
    'public static final String BACKEND_BEST = "best";\n    public static final String BACKEND_VULKAN = "vulkan";')

# Native loader: keep Vulkan and CPU modules alive together and enable GPU only for that mode.
jni = 'SpeechNotes/app/src/main/cpp/whisper_jni.cpp'
rep(jni,
    'static ggml_backend_reg_t g_cpu_backend = nullptr;',
    'static ggml_backend_reg_t g_cpu_backend = nullptr;\nstatic ggml_backend_reg_t g_gpu_backend = nullptr;')
rep(jni,
'''static void unload_backend_locked() {
    if (g_cpu_backend) {
        ggml_backend_unload(g_cpu_backend);
        g_cpu_backend = nullptr;
    }
    g_backend_mode.clear();
    g_backend_name.clear();
}''',
'''static void unload_backend_locked() {
    if (g_gpu_backend) {
        ggml_backend_unload(g_gpu_backend);
        g_gpu_backend = nullptr;
    }
    if (g_cpu_backend) {
        ggml_backend_unload(g_cpu_backend);
        g_cpu_backend = nullptr;
    }
    g_backend_mode.clear();
    g_backend_name.clear();
}''')
rep(jni,
    'static bool load_cpu_backend_locked(const std::string &dir, const std::string &mode) {\n    unload_backend_locked();\n',
    'static bool load_cpu_backend_locked(const std::string &dir, const std::string &mode) {\n')
rep(jni,
'''    return false;
}

extern "C" JNIEXPORT jlong JNICALL''',
'''    return false;
}

static bool load_vulkan_backend_locked(const std::string &dir) {
    const std::string path = join_path(dir, "libggml-vulkan.so");
    ggml_backend_reg_t reg = ggml_backend_load(path.c_str());
    if (!reg) {
        LOGE("Could not load Vulkan backend: %s", path.c_str());
        return false;
    }
    g_gpu_backend = reg;
    LOGI("Loaded Vulkan backend: %s", path.c_str());
    return true;
}

extern "C" JNIEXPORT jlong JNICALL''')
rep(jni,
'''    free_model_locked();
    unload_backend_locked();
    if (!load_cpu_backend_locked(backendDir, backendMode)) {
        LOGE("Could not load CPU backend mode=%s dir=%s", backendMode.c_str(), backendDir.c_str());
        return -2;
    }

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;''',
'''    free_model_locked();
    unload_backend_locked();
    bool useGpu = false;
    if (backendMode == "vulkan") {
        if (!load_cpu_backend_locked(backendDir, "best")) {
            LOGE("Could not load ARM Best CPU fallback for Vulkan dir=%s", backendDir.c_str());
            return -2;
        }
        const std::string cpuName = g_backend_name;
        if (!load_vulkan_backend_locked(backendDir)) {
            unload_backend_locked();
            return -3;
        }
        g_backend_mode = "vulkan";
        g_backend_name = "vulkan + " + cpuName;
        useGpu = true;
    } else if (!load_cpu_backend_locked(backendDir, backendMode)) {
        LOGE("Could not load CPU backend mode=%s dir=%s", backendMode.c_str(), backendDir.c_str());
        return -2;
    }

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = useGpu;''')

# Benchmark UI: third suite ARM Best vs Vulkan.
bench = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/BenchmarkActivity.java'
rep(bench,
    'private static final String[] ARM_STEPS = {"Generic ARMv8.0 · 8 Threads", "ARM Best · 8 Threads"};',
    'private static final String[] ARM_STEPS = {"Generic ARMv8.0 · 8 Threads", "ARM Best · 8 Threads"};\n    private static final String[] VULKAN_STEPS = {"ARM Best CPU · 8 Threads", "Vulkan GPU + ARM Best fallback"};')
rep(bench,
    'new String[]{"Threading · 2 / 4 / 6 / 8", "CPU Backend · Generic vs ARM Best"}',
    'new String[]{"Threading · 2 / 4 / 6 / 8", "CPU Backend · Generic vs ARM Best", "GPU Backend · ARM Best vs Vulkan"}')
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
        boolean vulkan = position == 2;
        activeSteps = vulkan ? VULKAN_STEPS : (arm ? ARM_STEPS : THREAD_STEPS);
        if (suiteDescription != null) suiteDescription.setText(vulkan
                ? "ARM Best CPU vs Vulkan GPU · identisches Turbo-Modell · 25 s · Deutsch · no-context"
                : (arm ? "Generic ARMv8.0 vs höchste sicher unterstützte GGML-ARM-Variante · jeweils 8 Threads"
                : "2 → 4 → 6 → 8 Threads · Generic CPU · Deutsch · no-context"));
        if (startButton != null) startButton.setText(vulkan ? "Vulkan-Benchmark starten" : (arm ? "ARM-Benchmark starten" : "Thread-Benchmark starten"));''')
rep(bench,
'''        final boolean armSuite = suiteSpinner.getSelectedItemPosition() == 1;
        activeSteps = armSuite ? ARM_STEPS : THREAD_STEPS;''',
'''        final int suite = suiteSpinner.getSelectedItemPosition();
        activeSteps = suite == 2 ? VULKAN_STEPS : (suite == 1 ? ARM_STEPS : THREAD_STEPS);''')
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
                    ? runVulkanSuite(modelFile, pcm, info.durationMs)
                    : (suite == 1 ? runArmSuite(modelFile, pcm, info.durationMs)
                    : runThreadSuite(modelFile, pcm, info.durationMs));''')
rep(bench,
'''        return list;
    }

    private void updateProgressStart''',
'''        return list;
    }

    private ArrayList<RunResult> runVulkanSuite(File modelFile, File pcm, long audioMs) {
        ArrayList<RunResult> list = new ArrayList<>();
        String[] modes = {WhisperBridge.BACKEND_BEST, WhisperBridge.BACKEND_VULKAN};
        String[] labels = {"ARM Best CPU", "Vulkan"};
        for (int i = 0; i < modes.length; i++) {
            int step = i; String mode = modes[i]; String label = labels[i];
            ui.post(() -> updateProgressStart(step, label));
            long loadMs = WhisperBridge.loadModel(modelFile.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, mode);
            if (loadMs < 0) throw new IllegalStateException(label + " Backend konnte nicht geladen werden (" + loadMs + ")");
            String backend = WhisperBridge.currentBackendName();
            long start = SystemClock.elapsedRealtime();
            WhisperBridge.Result r = WhisperBridge.transcribeLoadedBenchmark(pcm.getAbsolutePath(), "de", "", 8);
            long wall = SystemClock.elapsedRealtime() - start;
            RunResult rr = new RunResult(label, backend, 8, wall, audioMs, r);
            list.add(rr); ui.post(() -> completeStep(step, rr));
        }
        return list;
    }

    private void updateProgressStart''')

print('Applied v1.6 Vulkan runtime + benchmark suite patch')
