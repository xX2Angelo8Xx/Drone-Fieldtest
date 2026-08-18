from pathlib import Path


def rep(path, old, new, count=1):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'Expected block not found in {path}: {old[:160]!r}')
    p.write_text(s.replace(old, new, count))

bridge = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/WhisperBridge.java'
rep(bridge,
    'public static final String BACKEND_V90 = "v90";',
    'public static final String BACKEND_V90 = "v90";\n    public static final String BACKEND_V90_KAI = "v90_kai";')

jni = 'SpeechNotes/app/src/main/cpp/whisper_jni.cpp'
rep(jni,
'''    if (mode == "v90") {
        if (try_backend_locked(dir, "android_armv9.0_1")) {
            g_backend_mode = mode;
            return true;
        }
        return false;
    }''',
'''    if (mode == "v90") {
        if (try_backend_locked(dir, "android_armv9.0_1")) {
            g_backend_mode = mode;
            return true;
        }
        return false;
    }
    if (mode == "v90_kai") {
        if (try_backend_locked(dir, "android_armv9.0_1_kai")) {
            g_backend_mode = mode;
            g_backend_name += " + KleidiAI";
            return true;
        }
        return false;
    }''')

bench = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/BenchmarkActivity.java')
s = bench.read_text()

old_steps_start = s.index('    private static final String[] CPU_LAB_STEPS = {')
old_steps_end = s.index('    };', old_steps_start) + len('    };')
new_steps = '''    private static final String[] CPU_LAB_STEPS = {
            "1 · Q5_0 · ARMv9 · 8T · ctx1280",
            "2 · Q5_0 · KleidiAI · 8T · ctx1280",
            "3 · Q4_0 · ARMv9 · 8T · ctx1280",
            "4 · Q4_0 · KleidiAI · 8T · ctx1280",
            "5 · Q4_0 · KleidiAI · 8T · ctx1152",
            "6 · Q4_0 · KleidiAI · 8T · ctx1024",
            "7 · Q4_0 · KleidiAI · Fast4/4T · ctx1280",
            "8 · Q4_0 · KleidiAI · Fast6/6T · ctx1280"
    };'''
s = s[:old_steps_start] + new_steps + s[old_steps_end:]

s = s.replace('"CPU Performance Lab · 9 Runs"', '"Encoder Lab · Q5/Q4/KleidiAI · 8 Runs"', 1)
s = s.replace('Large-v3-Turbo Q5_0 · identische 25 s · ARM-only · Backend/Threads/FlashAttn/audio_ctx/Affinity',
              'Large-v3-Turbo · Q5_0 vs Q4_0 · Stock ARMv9 vs isoliertes KleidiAI · ctx/Affinity · identische 25 s', 1)
s = s.replace('"CPU Performance Lab starten"', '"Encoder Lab starten"', 1)

old_run = '''            File modelFile = ModelManager.ensureModel(this, model);
            ArrayList<RunResult> runResults = suite == 2
                    ? runCpuLab(modelFile, pcm, info.durationMs)
                    : (suite == 1 ? runArmSuite(modelFile, pcm, info.durationMs)
                    : runThreadSuite(modelFile, pcm, info.durationMs));'''
new_run = '''            ArrayList<RunResult> runResults;
            if (suite == 2) {
                File q5 = ModelManager.ensureModel(this, "large-v3-turbo-q5_0");
                File q4 = ModelManager.ensureModel(this, "large-v3-turbo-q4_0");
                runResults = runEncoderLab(q5, q4, pcm, info.durationMs);
            } else {
                File modelFile = ModelManager.ensureModel(this, model);
                runResults = suite == 1 ? runArmSuite(modelFile, pcm, info.durationMs)
                        : runThreadSuite(modelFile, pcm, info.durationMs);
            }'''
if old_run not in s:
    raise SystemExit('runSuite v1.7 block not found')
s = s.replace(old_run, new_run, 1)

start = s.index('    private ArrayList<RunResult> runCpuLab(')
end = s.index('    private void updateProgressStart', start)
new_method = '''    private ArrayList<RunResult> runEncoderLab(File q5, File q4, File pcm, long audioMs) {
        ArrayList<RunResult> list = new ArrayList<>();
        String[] labels = {
                "Q5_0 · ARMv9 · 8T · ctx1280",
                "Q5_0 · KleidiAI · 8T · ctx1280",
                "Q4_0 · ARMv9 · 8T · ctx1280",
                "Q4_0 · KleidiAI · 8T · ctx1280",
                "Q4_0 · KleidiAI · 8T · ctx1152",
                "Q4_0 · KleidiAI · 8T · ctx1024",
                "Q4_0 · KleidiAI · Fast4/4T · ctx1280",
                "Q4_0 · KleidiAI · Fast6/6T · ctx1280"
        };
        String[] modes = {
                WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90_KAI,
                WhisperBridge.BACKEND_V90, WhisperBridge.BACKEND_V90_KAI,
                WhisperBridge.BACKEND_V90_KAI, WhisperBridge.BACKEND_V90_KAI,
                WhisperBridge.BACKEND_V90_KAI, WhisperBridge.BACKEND_V90_KAI
        };
        File[] models = {q5, q5, q4, q4, q4, q4, q4, q4};
        int[] threads = {8, 8, 8, 8, 8, 8, 4, 6};
        int[] audioCtx = {1280, 1280, 1280, 1280, 1152, 1024, 1280, 1280};
        int[] fastCores = {0, 0, 0, 0, 0, 0, 4, 6};
        for (int i = 0; i < labels.length; i++) {
            int step = i;
            String label = labels[i];
            ui.post(() -> updateProgressStart(step, label));
            long loadMs = WhisperBridge.loadModel(models[i].getAbsolutePath(), getApplicationInfo().nativeLibraryDir, modes[i]);
            if (loadMs < 0) throw new IllegalStateException(label + " Backend/Modell konnte nicht geladen werden (" + loadMs + ")");
            String backend = WhisperBridge.currentBackendName();
            long startMs = SystemClock.elapsedRealtime();
            WhisperBridge.Result r = WhisperBridge.transcribeLoadedBenchmarkProfile(
                    pcm.getAbsolutePath(), "de", "", threads[i], audioCtx[i], fastCores[i]);
            long wall = SystemClock.elapsedRealtime() - startMs;
            RunResult rr = new RunResult(label, backend, threads[i], wall, audioMs, r);
            list.add(rr);
            ui.post(() -> completeStep(step, rr));
        }
        return list;
    }

'''
s = s[:start] + new_method + s[end:]

old_card = '''        c.addView(space(3)); c.addView(text("Decode Σ " + formatMs(rr.profile.decodeTotalMs) + " · Mel " + formatMs(rr.profile.melMs) + " · PCM " + formatMs(rr.profile.pcmMs), 12, MUTED, false));
        LinearLayout.LayoutParams p = matchWrap(); p.bottomMargin = dp(9); results.addView(c, p);'''
new_card = '''        c.addView(space(3)); c.addView(text("Decode Σ " + formatMs(rr.profile.decodeTotalMs) + " · Mel " + formatMs(rr.profile.melMs) + " · PCM " + formatMs(rr.profile.pcmMs), 12, MUTED, false));
        String transcript = rr.profile.text == null ? "" : rr.profile.text.trim();
        if (!transcript.isEmpty()) {
            c.addView(space(8));
            c.addView(text("Transkript: " + transcript, 11, MUTED, false));
        }
        LinearLayout.LayoutParams p = matchWrap(); p.bottomMargin = dp(9); results.addView(c, p);'''
if old_card not in s:
    raise SystemExit('result card block not found')
s = s.replace(old_card, new_card, 1)

bench.write_text(s)
print('Applied v1.8 Q4_0 + isolated KleidiAI encoder lab')

# v1.8.1 keeps the v1.8 backend/model work and replaces only the benchmark matrix.
exec(Path('.github/patch_speechnotes_v181_validation_lab.py').read_text())
