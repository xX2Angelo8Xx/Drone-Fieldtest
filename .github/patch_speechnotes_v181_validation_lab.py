from pathlib import Path

bench = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/BenchmarkActivity.java')
s = bench.read_text()

steps_start = s.index('    private static final String[] CPU_LAB_STEPS = {')
steps_end = s.index('    };', steps_start) + len('    };')
labels = [
    'Q4_0 · Stock · 8T · no affinity',
    'Q4_0 · Stock · 4T · no affinity',
    'Q4_0 · Stock · Fast4/4T',
    'Q4_0 · Stock · Fast6/6T',
    'Q4_0 · KleidiAI · 4T · no affinity',
    'Q4_0 · KleidiAI · Fast4/4T',
]
step_lines = []
idx = 1
for rep in range(1, 4):
    for label in labels:
        step_lines.append(f'            "{idx} · R{rep} · {label} · ctx1280"')
        idx += 1
new_steps = '    private static final String[] CPU_LAB_STEPS = {\n' + ',\n'.join(step_lines) + '\n    };'
s = s[:steps_start] + new_steps + s[steps_end:]

s = s.replace('"Encoder Lab · Q5/Q4/KleidiAI · 8 Runs"',
              '"Encoder Validation Lab · v1.8.1 · 18 Runs"', 1)
s = s.replace('Large-v3-Turbo · Q5_0 vs Q4_0 · Stock ARMv9 vs isoliertes KleidiAI · ctx/Affinity · identische 25 s',
              'Large-v3-Turbo Q4_0 · Stock/KleidiAI · Threads/Affinity isoliert · 3 Wiederholungen · ctx1280 · identische 25 s', 1)
s = s.replace('"Encoder Lab starten"', '"v1.8.1 Validation starten"', 1)

start = s.index('    private ArrayList<RunResult> runEncoderLab(')
end = s.index('    private void updateProgressStart', start)
new_method = '''    private ArrayList<RunResult> runEncoderLab(File q5, File q4, File pcm, long audioMs) {
        ArrayList<RunResult> list = new ArrayList<>();
        String[] baseLabels = {
                "Q4_0 · Stock · 8T · no affinity",
                "Q4_0 · Stock · 4T · no affinity",
                "Q4_0 · Stock · Fast4/4T",
                "Q4_0 · Stock · Fast6/6T",
                "Q4_0 · KleidiAI · 4T · no affinity",
                "Q4_0 · KleidiAI · Fast4/4T"
        };
        String[] modes = {
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90_KAI,
                WhisperBridge.BACKEND_V90_KAI
        };
        int[] threads = {8, 4, 4, 6, 4, 4};
        int[] fastCores = {0, 0, 4, 6, 0, 4};
        final int audioCtx = 1280;
        int step = 0;
        for (int repeat = 1; repeat <= 3; repeat++) {
            for (int i = 0; i < baseLabels.length; i++) {
                final int uiStep = step;
                String label = "R" + repeat + " · " + baseLabels[i] + " · ctx1280";
                ui.post(() -> updateProgressStart(uiStep, label));
                long loadMs = WhisperBridge.loadModel(q4.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, modes[i]);
                if (loadMs < 0) throw new IllegalStateException(label + " Backend/Modell konnte nicht geladen werden (" + loadMs + ")");
                String backend = WhisperBridge.currentBackendName();
                long startMs = SystemClock.elapsedRealtime();
                WhisperBridge.Result r = WhisperBridge.transcribeLoadedBenchmarkProfile(
                        pcm.getAbsolutePath(), "de", "", threads[i], audioCtx, fastCores[i]);
                long wall = SystemClock.elapsedRealtime() - startMs;
                RunResult rr = new RunResult(label, backend, threads[i], wall, audioMs, r);
                list.add(rr);
                ui.post(() -> completeStep(uiStep, rr));
                step++;
            }
        }
        return list;
    }

'''
s = s[:start] + new_method + s[end:]
bench.write_text(s)
print('Applied v1.8.1 Q4 thread/affinity/backend validation lab (18 controlled runs)')
