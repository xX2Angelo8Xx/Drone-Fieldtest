from pathlib import Path


def rep(path, old, new, count=1):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'Expected block not found in {path}: {old[:180]!r}')
    p.write_text(s.replace(old, new, count))

# ---------- Productive runtime default: Q4_0 / stock ARMv9 / 4T / ctx1280 / no affinity ----------
bridge = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/WhisperBridge.java'
rep(bridge,
'''    public static Result transcribeLoadedBenchmarkProfile(String pcmPath, String language,
                                                           String initialPrompt, int threads,
                                                           int audioCtx, int fastCoreCount) {
        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, true,
                audioCtx, fastCoreCount));
    }''',
'''    public static Result transcribeLoadedProfile(String pcmPath, String language,
                                                  String initialPrompt, int threads,
                                                  int audioCtx, int fastCoreCount) {
        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, false,
                audioCtx, fastCoreCount));
    }

    public static Result transcribeLoadedBenchmarkProfile(String pcmPath, String language,
                                                           String initialPrompt, int threads,
                                                           int audioCtx, int fastCoreCount) {
        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, true,
                audioCtx, fastCoreCount));
    }''')

main = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/MainActivity.java'
rep(main,
'''        String[] labels = {
                "Whisper Base · Q5_1 · ~57 MiB",
                "Whisper Large-v3-Turbo · Q5_0 · ~547 MiB"
        };''',
'''        String[] labels = {
                "Whisper Base · Q5_1 · ~57 MiB",
                "Whisper Large-v3-Turbo · Q5_0 · ~547 MiB",
                "Whisper Large-v3-Turbo · Q4_0 · Performance Default"
        };''')
rep(main,
'''        int savedModel = prefs.getInt("model_v2", 0);
        modelSpinner.setSelection(Math.max(0, Math.min(1, savedModel)));''',
'''        int savedModel = prefs.getInt("model_v183", 2);
        modelSpinner.setSelection(Math.max(0, Math.min(2, savedModel)));''')
rep(main, 'prefs.edit().putInt("model_v2", position).apply();', 'prefs.edit().putInt("model_v183", position).apply();')
rep(main,
'''    private String selectedModelName() {
        return modelSpinner != null && modelSpinner.getSelectedItemPosition() == 1
                ? "large-v3-turbo-q5_0" : "base-q5_1";
    }''',
'''    private String selectedModelName() {
        if (modelSpinner == null) return "large-v3-turbo-q4_0";
        int position = modelSpinner.getSelectedItemPosition();
        if (position == 1) return "large-v3-turbo-q5_0";
        if (position == 2) return "large-v3-turbo-q4_0";
        return "base-q5_1";
    }''')
p = Path(main); s = p.read_text(); s = s.replace('WhisperBridge.BACKEND_BEST', 'WhisperBridge.BACKEND_V90'); p.write_text(s)

service = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/RecordingService.java'
p = Path(service); s = p.read_text(); s = s.replace('WhisperBridge.BACKEND_BEST', 'WhisperBridge.BACKEND_V90'); p.write_text(s)
rep(service,
'''                int cores = Math.max(1, Runtime.getRuntime().availableProcessors());
                int threads = model.startsWith("large-v3-turbo")
                        ? Math.min(8, cores)
                        : Math.max(2, Math.min(6, Math.max(2, cores - 2)));''',
'''                int cores = Math.max(1, Runtime.getRuntime().availableProcessors());
                int threads = model.startsWith("large-v3-turbo")
                        ? Math.min(4, cores)
                        : Math.max(2, Math.min(6, Math.max(2, cores - 2)));''')
rep(service,
'''                profile = WhisperBridge.transcribeLoaded(
                        pcmFile.getAbsolutePath(), "de", prompt, threads);''',
'''                profile = WhisperBridge.transcribeLoadedProfile(
                        pcmFile.getAbsolutePath(), "de", prompt, threads,
                        model.startsWith("large-v3-turbo") ? 1280 : 0, 0);''')
rep(service,
'''    private static String modelLabel(String m) {
        if (m.startsWith("large-v3-turbo")) return "Whisper Large-v3-Turbo Q5_0";
        return "Whisper Base Q5_1";
    }''',
'''    private static String modelLabel(String m) {
        if ("large-v3-turbo-q4_0".equals(m)) return "Whisper Large-v3-Turbo Q4_0";
        if (m.startsWith("large-v3-turbo")) return "Whisper Large-v3-Turbo Q5_0";
        return "Whisper Base Q5_1";
    }''')

# ---------- Benchmark v1.8.3: isolate thread-count, affinity, and combined trigger ----------
bench = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/BenchmarkActivity.java')
s = bench.read_text()

steps_start = s.index('    private static final String[] CPU_LAB_STEPS = {')
steps_end = s.index('    };', steps_start) + len('    };')
new_steps = '''    private static final String[] THREAD_TRIGGER_STEPS = {
            "1 · Stock4 control-1 · ctx1280",
            "2 · Stock4 control-2 · ctx1280",
            "3 · Stock8 trigger · ctx1280",
            "4 · Stock4 post-8T-1 · ctx1280",
            "5 · Stock4 post-8T-2 · ctx1280"
    };
    private static final String[] AFFINITY_TRIGGER_STEPS = {
            "1 · Stock4 control · ctx1280",
            "2 · Stock Fast4/4T · ctx1280",
            "3 · Stock4 after-Fast4 · ctx1280",
            "4 · Stock Fast6/6T · ctx1280",
            "5 · Stock4 after-Fast6 · ctx1280"
    };
    private static final String[] REPRO_TRIGGER_STEPS = {
            "1 · Stock8 · ctx1280",
            "2 · Stock4 · ctx1280",
            "3 · Stock Fast4/4T · ctx1280",
            "4 · Stock Fast6/6T · ctx1280",
            "5 · KleidiAI4 · ctx1280",
            "6 · Stock4 post-sequence · ctx1280"
    };'''
s = s[:steps_start] + new_steps + s[steps_end:]

s = s.replace(
    'new String[]{"Threading · 2 / 4 / 6 / 8", "CPU Backend · Generic vs ARM Best", "Runtime Diagnostics Lab · v1.8.2 · 9 Runs"}',
    'new String[]{"Threading · 2 / 4 / 6 / 8", "CPU Backend · Generic vs ARM Best", "v1.8.3 · Thread-count trigger", "v1.8.3 · Affinity trigger", "v1.8.3 · v1.8.1 reproduction"}',
    1)

start = s.index('    private void configureSuite(int position) {')
end = s.index('    private void loadWavs()', start)
new_configure = '''    private void configureSuite(int position) {
        boolean arm = position == 1;
        boolean diag = position >= 2;
        activeSteps = stepsForSuite(position);
        if (suiteDescription != null) {
            if (position == 2) suiteDescription.setText("Q4_0 Stock ARMv9 · 4T → 8T → 4T · isoliert Thread-count als Trigger · ctx1280");
            else if (position == 3) suiteDescription.setText("Q4_0 Stock ARMv9 · Fast4/Fast6 zwischen 4T Controls · isoliert sched_setaffinity · ctx1280");
            else if (position == 4) suiteDescription.setText("Reproduktion der kritischen v1.8.1 Reihenfolge · 8T/4T/Fast4/Fast6/KAI4/Stock4 · ctx1280");
            else if (arm) suiteDescription.setText("Generic ARMv8.0 vs höchste sicher unterstützte GGML-ARM-Variante · jeweils 8 Threads");
            else suiteDescription.setText("2 → 4 → 6 → 8 Threads · Generic CPU · Deutsch · no-context");
        }
        if (startButton != null) startButton.setText(diag ? "v1.8.3 Diagnose starten" : (arm ? "ARM-Benchmark starten" : "Thread-Benchmark starten"));
        if (progress != null) { progress.setMax(activeSteps.length); progress.setProgress(0); }
        if (progressLabel != null) progressLabel.setText("Bereit");
        if (stepList != null) rebuildStepList(-1, -1);
    }

    private String[] stepsForSuite(int position) {
        if (position == 2) return THREAD_TRIGGER_STEPS;
        if (position == 3) return AFFINITY_TRIGGER_STEPS;
        if (position == 4) return REPRO_TRIGGER_STEPS;
        if (position == 1) return ARM_STEPS;
        return THREAD_STEPS;
    }

'''
s = s[:start] + new_configure + s[end:]

rep_text_old = '''        final int suite = suiteSpinner.getSelectedItemPosition();
        activeSteps = suite == 2 ? CPU_LAB_STEPS : (suite == 1 ? ARM_STEPS : THREAD_STEPS);'''
rep_text_new = '''        final int suite = suiteSpinner.getSelectedItemPosition();
        activeSteps = stepsForSuite(suite);'''
if rep_text_old not in s:
    raise SystemExit('v1.8.3 startBenchmark suite anchor not found')
s = s.replace(rep_text_old, rep_text_new, 1)

old_run_route = '''            ArrayList<RunResult> runResults;
            if (suite == 2) {
                File q5 = ModelManager.ensureModel(this, "large-v3-turbo-q5_0");
                File q4 = ModelManager.ensureModel(this, "large-v3-turbo-q4_0");
                runResults = runEncoderLab(q5, q4, pcm, info.durationMs);
            } else {
                File modelFile = ModelManager.ensureModel(this, model);
                runResults = suite == 1 ? runArmSuite(modelFile, pcm, info.durationMs)
                        : runThreadSuite(modelFile, pcm, info.durationMs);
            }'''
new_run_route = '''            ArrayList<RunResult> runResults;
            if (suite >= 2) {
                File q4 = ModelManager.ensureModel(this, "large-v3-turbo-q4_0");
                runResults = runTriggerLab(q4, pcm, info.durationMs, suite);
            } else {
                File modelFile = ModelManager.ensureModel(this, model);
                runResults = suite == 1 ? runArmSuite(modelFile, pcm, info.durationMs)
                        : runThreadSuite(modelFile, pcm, info.durationMs);
            }'''
if old_run_route not in s:
    raise SystemExit('v1.8.3 runSuite routing anchor not found')
s = s.replace(old_run_route, new_run_route, 1)

start = s.index('    private ArrayList<RunResult> runEncoderLab(')
end = s.index('    private String memorySnapshot()', start)
new_runner = r'''    private ArrayList<RunResult> runTriggerLab(File q4, File pcm, long audioMs, int suite) {
        ArrayList<RunResult> list = new ArrayList<>();
        String[] labels;
        String[] modes;
        int[] threads;
        int[] fastCores;
        String suiteName;
        if (suite == 2) {
            suiteName = "thread_count_trigger";
            labels = new String[]{"Stock4 control-1", "Stock4 control-2", "Stock8 trigger", "Stock4 post-8T-1", "Stock4 post-8T-2"};
            modes = new String[]{WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90};
            threads = new int[]{4, 4, 8, 4, 4};
            fastCores = new int[]{0, 0, 0, 0, 0};
        } else if (suite == 3) {
            suiteName = "affinity_trigger";
            labels = new String[]{"Stock4 control", "Stock Fast4/4T", "Stock4 after-Fast4", "Stock Fast6/6T", "Stock4 after-Fast6"};
            modes = new String[]{WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90};
            threads = new int[]{4, 4, 4, 6, 4};
            fastCores = new int[]{0, 4, 0, 6, 0};
        } else {
            suiteName = "v181_reproduction";
            labels = new String[]{"Stock8", "Stock4", "Stock Fast4/4T", "Stock Fast6/6T", "KleidiAI4", "Stock4 post-sequence"};
            modes = new String[]{WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90_KAI, WhisperBridge.BACKEND_V90};
            threads = new int[]{8, 4, 4, 6, 4, 4};
            fastCores = new int[]{0, 0, 4, 6, 0, 0};
        }
        final int audioCtx = 1280;
        benchmarkExport.append("Speech Notes Trigger Diagnostics v1.8.3\n");
        benchmarkExport.append("suite=").append(suiteName).append("\n");
        benchmarkExport.append("device=").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append("\n");
        benchmarkExport.append("android=").append(Build.VERSION.RELEASE).append(" sdk=").append(Build.VERSION.SDK_INT).append("\n");
        benchmarkExport.append("audio_ms=").append(audioMs).append("\n");
        benchmarkExport.append("model=large-v3-turbo-q4_0 ctx=1280\n\n");

        for (int i = 0; i < labels.length; i++) {
            final int step = i;
            final String label = labels[i] + " · ctx1280";
            final String mode = modes[i];
            final int runThreads = threads[i];
            final int runFastCores = fastCores[i];
            ui.post(() -> updateProgressStart(step, label));

            String memBeforeLoad = memorySnapshot();
            String cpuBeforeLoad = cpuFreqSnapshot();
            String affinityBeforeLoad = threadAffinitySnapshot();
            long loadStart = SystemClock.elapsedRealtime();
            long loadMs = WhisperBridge.loadModel(q4.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, mode);
            long loadWallMs = SystemClock.elapsedRealtime() - loadStart;
            if (loadMs < 0) throw new IllegalStateException(label + " Backend/Modell konnte nicht geladen werden (" + loadMs + ")");
            String backend = WhisperBridge.currentBackendName();
            String memAfterLoad = memorySnapshot();
            String cpuAfterLoad = cpuFreqSnapshot();
            String affinityAfterLoad = threadAffinitySnapshot();

            DiagSampler sampler = new DiagSampler();
            sampler.start();
            long startMs = SystemClock.elapsedRealtime();
            WhisperBridge.Result r = WhisperBridge.transcribeLoadedBenchmarkProfile(
                    pcm.getAbsolutePath(), "de", "", runThreads, audioCtx, runFastCores);
            long wall = SystemClock.elapsedRealtime() - startMs;
            String samplerSummary = sampler.finish();
            String memAfterInfer = memorySnapshot();
            String cpuAfterInfer = cpuFreqSnapshot();
            String affinityAfterInfer = threadAffinitySnapshot();

            RunResult rr = new RunResult(label, backend, runThreads, wall, audioMs, r);
            list.add(rr);
            synchronized (benchmarkExport) {
                benchmarkExport.append("RUN ").append(step + 1).append(" | ").append(label).append("\n");
                benchmarkExport.append("backend=").append(backend).append(" mode=").append(mode)
                        .append(" threads=").append(runThreads).append(" fast_cores=").append(runFastCores).append("\n");
                benchmarkExport.append("load_api_ms=").append(loadMs).append(" load_wall_ms=").append(loadWallMs).append("\n");
                benchmarkExport.append("wall_ms=").append(wall).append(" rtf=").append(String.format(Locale.US, "%.4f", rr.rtf())).append("\n");
                benchmarkExport.append("native_ms=").append(r.profile.nativeTotalMs)
                        .append(" encode_ms=").append(r.profile.encodeTotalMs)
                        .append(" encode_runs=").append(r.profile.encodeRuns)
                        .append(" decode_ms=").append(r.profile.decodeTotalMs)
                        .append(" mel_ms=").append(r.profile.melMs)
                        .append(" pcm_ms=").append(r.profile.pcmMs).append("\n");
                benchmarkExport.append("mem_before_load=").append(memBeforeLoad).append("\n");
                benchmarkExport.append("mem_after_load=").append(memAfterLoad).append("\n");
                benchmarkExport.append("mem_after_infer=").append(memAfterInfer).append("\n");
                benchmarkExport.append("cpu_before_load=").append(cpuBeforeLoad).append("\n");
                benchmarkExport.append("cpu_after_load=").append(cpuAfterLoad).append("\n");
                benchmarkExport.append("cpu_after_infer=").append(cpuAfterInfer).append("\n");
                benchmarkExport.append("affinity_before_load=").append(affinityBeforeLoad).append("\n");
                benchmarkExport.append("affinity_after_load=").append(affinityAfterLoad).append("\n");
                benchmarkExport.append("affinity_after_infer=").append(affinityAfterInfer).append("\n");
                benchmarkExport.append("sampler=").append(samplerSummary).append("\n");
                String transcript = r.profile.text == null ? "" : r.profile.text.trim().replace('\n', ' ');
                benchmarkExport.append("transcript=").append(transcript).append("\n\n");
            }
            ui.post(() -> completeStep(step, rr));
        }
        return list;
    }

    private String threadAffinitySnapshot() {
        StringBuilder out = new StringBuilder();
        File taskDir = new File("/proc/self/task");
        File[] tasks = taskDir.listFiles();
        if (tasks == null) return "unavailable";
        Arrays.sort(tasks, Comparator.comparing(File::getName));
        int written = 0;
        for (File task : tasks) {
            if (written >= 48 || out.length() > 2400) break;
            String status = task.getAbsolutePath() + "/status";
            String name = procValue(status, "Name:");
            String allowed = procValue(status, "Cpus_allowed_list:");
            if (allowed.isEmpty()) continue;
            if (out.length() > 0) out.append(';');
            out.append(task.getName()).append(':').append(name.replace(' ', '_')).append('@').append(allowed.replace(' ', '_'));
            written++;
        }
        return out.length() == 0 ? "unavailable" : out.toString();
    }

'''
s = s[:start] + new_runner + s[end:]

s = s.replace('Runtime Diagnostics Lab · v1.8.2 · 9 Runs', 'v1.8.3 · Trigger Diagnostics', 1)
s = s.replace('Large-v3-Turbo Q4_0 · 5× Stock4 → 1× KleidiAI4 → 3× Stock4 · Memory/CPU/Thermal telemetry · ctx1280',
              'Large-v3-Turbo Q4_0 · Thread/Affinity trigger isolation · copyable diagnostics · ctx1280', 1)
s = s.replace('"v1.8.2 Diagnostics starten"', '"v1.8.3 Diagnose starten"', 1)
bench.write_text(s)
print('Applied v1.8.3 productive Q4/4T/ctx1280 default + trigger diagnostics suites')
