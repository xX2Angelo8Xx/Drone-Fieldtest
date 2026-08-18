from pathlib import Path

bench = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/BenchmarkActivity.java')
s = bench.read_text()

s = s.replace('import android.content.Intent;\n', 'import android.content.Intent;\nimport android.content.ClipData;\nimport android.content.ClipboardManager;\n', 1)
s = s.replace('import android.os.Build;\n', 'import android.os.Build;\nimport android.os.Debug;\n', 1)
s = s.replace('import java.io.File;\n', 'import java.io.File;\nimport java.io.BufferedReader;\nimport java.io.FileReader;\n', 1)
s = s.replace('    private LinearLayout results;\n', '    private LinearLayout results;\n    private Button copyButton;\n    private final StringBuilder benchmarkExport = new StringBuilder();\n', 1)

steps_start = s.index('    private static final String[] CPU_LAB_STEPS = {')
steps_end = s.index('    };', steps_start) + len('    };')
steps = [
    '1 · Stock4 baseline-1 · ctx1280',
    '2 · Stock4 baseline-2 · ctx1280',
    '3 · Stock4 baseline-3 · ctx1280',
    '4 · Stock4 baseline-4 · ctx1280',
    '5 · Stock4 baseline-5 · ctx1280',
    '6 · KleidiAI4 trigger · ctx1280',
    '7 · Stock4 post-KAI-1 · ctx1280',
    '8 · Stock4 post-KAI-2 · ctx1280',
    '9 · Stock4 post-KAI-3 · ctx1280',
]
new_steps = '    private static final String[] CPU_LAB_STEPS = {\n' + ',\n'.join(f'            "{x}"' for x in steps) + '\n    };'
s = s[:steps_start] + new_steps + s[steps_end:]

s = s.replace('"Encoder Validation Lab · v1.8.1 · 18 Runs"', '"Runtime Diagnostics Lab · v1.8.2 · 9 Runs"', 1)
s = s.replace('Large-v3-Turbo Q4_0 · Stock/KleidiAI · Threads/Affinity isoliert · 3 Wiederholungen · ctx1280 · identische 25 s',
              'Large-v3-Turbo Q4_0 · 5× Stock4 → 1× KleidiAI4 → 3× Stock4 · Memory/CPU/Thermal telemetry · ctx1280', 1)
s = s.replace('"v1.8.1 Validation starten"', '"v1.8.2 Diagnostics starten"', 1)

old_ui = '''        results = column(); content.addView(results, matchWrap()); results.addView(text("Noch kein Benchmark durchgeführt.", 13, MUTED, false));
        content.addView(space(14));'''
new_ui = '''        results = column(); content.addView(results, matchWrap()); results.addView(text("Noch kein Benchmark durchgeführt.", 13, MUTED, false));
        content.addView(space(12));
        copyButton = new Button(this); copyButton.setAllCaps(false); copyButton.setText("Benchmark-Daten kopieren");
        copyButton.setTextSize(14); copyButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        copyButton.setTextColor(TEXT); copyButton.setBackground(roundRect(CARD_2, 16)); copyButton.setEnabled(false); copyButton.setAlpha(0.45f);
        copyButton.setOnClickListener(v -> copyBenchmarkExport());
        content.addView(copyButton, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(52)));
        content.addView(space(14));'''
if old_ui not in s:
    raise SystemExit('v1.8.2 UI insertion anchor not found')
s = s.replace(old_ui, new_ui, 1)

old_start = '        results.removeAllViews(); rebuildStepList(0, -1); acquireWakeLock();'
new_start = '''        results.removeAllViews(); benchmarkExport.setLength(0);
        if (copyButton != null) { copyButton.setEnabled(false); copyButton.setAlpha(0.45f); }
        rebuildStepList(0, -1); acquireWakeLock();'''
if old_start not in s:
    raise SystemExit('startBenchmark export reset anchor not found')
s = s.replace(old_start, new_start, 1)

start = s.index('    private ArrayList<RunResult> runEncoderLab(')
end = s.index('    private void updateProgressStart', start)
new_method = r'''    private ArrayList<RunResult> runEncoderLab(File q5, File q4, File pcm, long audioMs) {
        ArrayList<RunResult> list = new ArrayList<>();
        String[] labels = {
                "Stock4 baseline-1 · ctx1280",
                "Stock4 baseline-2 · ctx1280",
                "Stock4 baseline-3 · ctx1280",
                "Stock4 baseline-4 · ctx1280",
                "Stock4 baseline-5 · ctx1280",
                "KleidiAI4 trigger · ctx1280",
                "Stock4 post-KAI-1 · ctx1280",
                "Stock4 post-KAI-2 · ctx1280",
                "Stock4 post-KAI-3 · ctx1280"
        };
        String[] modes = {
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90_KAI,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90,
                WhisperBridge.BACKEND_V90
        };
        final int threads = 4;
        final int audioCtx = 1280;
        final int fastCores = 0;

        benchmarkExport.append("Speech Notes Benchmark Diagnostics v1.8.2\n");
        benchmarkExport.append("device=").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append("\n");
        benchmarkExport.append("android=").append(Build.VERSION.RELEASE).append(" sdk=").append(Build.VERSION.SDK_INT).append("\n");
        benchmarkExport.append("audio_ms=").append(audioMs).append("\n");
        benchmarkExport.append("model=large-v3-turbo-q4_0 ctx=1280 threads=4 affinity=none\n\n");

        for (int i = 0; i < labels.length; i++) {
            final int step = i;
            final String label = labels[i];
            final String mode = modes[i];
            ui.post(() -> updateProgressStart(step, label));

            String memBeforeLoad = memorySnapshot();
            String cpuBeforeLoad = cpuFreqSnapshot();
            String thermBeforeLoad = thermalSnapshot();
            long loadStart = SystemClock.elapsedRealtime();
            long loadMs = WhisperBridge.loadModel(q4.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, mode);
            long loadWallMs = SystemClock.elapsedRealtime() - loadStart;
            if (loadMs < 0) throw new IllegalStateException(label + " Backend/Modell konnte nicht geladen werden (" + loadMs + ")");
            String backend = WhisperBridge.currentBackendName();
            String memAfterLoad = memorySnapshot();
            String cpuAfterLoad = cpuFreqSnapshot();
            String thermAfterLoad = thermalSnapshot();

            DiagSampler sampler = new DiagSampler();
            sampler.start();
            long startMs = SystemClock.elapsedRealtime();
            WhisperBridge.Result r = WhisperBridge.transcribeLoadedBenchmarkProfile(
                    pcm.getAbsolutePath(), "de", "", threads, audioCtx, fastCores);
            long wall = SystemClock.elapsedRealtime() - startMs;
            String samplerSummary = sampler.finish();
            String memAfterInfer = memorySnapshot();
            String cpuAfterInfer = cpuFreqSnapshot();
            String thermAfterInfer = thermalSnapshot();

            RunResult rr = new RunResult(label, backend, threads, wall, audioMs, r);
            list.add(rr);

            synchronized (benchmarkExport) {
                benchmarkExport.append("RUN ").append(step + 1).append(" | ").append(label).append("\n");
                benchmarkExport.append("backend=").append(backend).append(" mode=").append(mode).append("\n");
                benchmarkExport.append("load_api_ms=").append(loadMs).append(" load_wall_ms=").append(loadWallMs).append("\n");
                benchmarkExport.append("wall_ms=").append(wall)
                        .append(" rtf=").append(String.format(Locale.US, "%.4f", rr.rtf())).append("\n");
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
                benchmarkExport.append("thermal_before_load=").append(thermBeforeLoad).append("\n");
                benchmarkExport.append("thermal_after_load=").append(thermAfterLoad).append("\n");
                benchmarkExport.append("thermal_after_infer=").append(thermAfterInfer).append("\n");
                benchmarkExport.append("sampler=").append(samplerSummary).append("\n");
                String transcript = r.profile.text == null ? "" : r.profile.text.trim().replace('\n', ' ');
                benchmarkExport.append("transcript=").append(transcript).append("\n\n");
            }
            ui.post(() -> completeStep(step, rr));
        }
        return list;
    }

    private String memorySnapshot() {
        StringBuilder out = new StringBuilder();
        String[] keys = {"VmRSS:", "VmHWM:", "VmSize:", "VmSwap:", "RssAnon:", "RssFile:", "RssShmem:", "Threads:"};
        for (String key : keys) {
            String v = procValue("/proc/self/status", key);
            if (!v.isEmpty()) out.append(key.substring(0, key.length() - 1)).append('=').append(v.replace(' ', '_')).append(',');
        }
        String[] sys = {"MemAvailable:", "SwapTotal:", "SwapFree:", "AnonPages:", "Mapped:", "Shmem:"};
        for (String key : sys) {
            String v = procValue("/proc/meminfo", key);
            if (!v.isEmpty()) out.append(key.substring(0, key.length() - 1)).append('=').append(v.replace(' ', '_')).append(',');
        }
        out.append("NativeHeapAllocated=").append(Debug.getNativeHeapAllocatedSize())
                .append(",NativeHeapSize=").append(Debug.getNativeHeapSize())
                .append(",NativeHeapFree=").append(Debug.getNativeHeapFreeSize());
        return out.toString();
    }

    private String procValue(String path, String key) {
        try (BufferedReader br = new BufferedReader(new FileReader(path))) {
            String line;
            while ((line = br.readLine()) != null) {
                if (line.startsWith(key)) return line.substring(key.length()).trim();
            }
        } catch (Throwable ignored) { }
        return "";
    }

    private long readLongFile(String path) {
        try (BufferedReader br = new BufferedReader(new FileReader(path))) {
            String line = br.readLine();
            if (line == null) return -1;
            return Long.parseLong(line.trim());
        } catch (Throwable ignored) { return -1; }
    }

    private String readTextFile(String path) {
        try (BufferedReader br = new BufferedReader(new FileReader(path))) {
            String line = br.readLine();
            return line == null ? "" : line.trim();
        } catch (Throwable ignored) { return ""; }
    }

    private String cpuFreqSnapshot() {
        StringBuilder out = new StringBuilder();
        for (int cpu = 0; cpu < 16; cpu++) {
            String base = "/sys/devices/system/cpu/cpu" + cpu + "/cpufreq/";
            long cur = readLongFile(base + "scaling_cur_freq");
            if (cur < 0) cur = readLongFile(base + "cpuinfo_cur_freq");
            long max = readLongFile(base + "scaling_max_freq");
            if (max < 0) max = readLongFile(base + "cpuinfo_max_freq");
            if (cur >= 0 || max >= 0) {
                if (out.length() > 0) out.append(',');
                out.append("cpu").append(cpu).append('=').append(cur).append('/').append(max);
            }
        }
        return out.length() == 0 ? "unavailable" : out.toString();
    }

    private String thermalSnapshot() {
        StringBuilder out = new StringBuilder();
        long maxTemp = Long.MIN_VALUE;
        String maxType = "none";
        for (int z = 0; z < 64; z++) {
            String base = "/sys/class/thermal/thermal_zone" + z + "/";
            long temp = readLongFile(base + "temp");
            if (temp < 0) continue;
            String type = readTextFile(base + "type");
            if (type.isEmpty()) type = "zone" + z;
            if (temp > maxTemp) { maxTemp = temp; maxType = type; }
            if (out.length() < 700) {
                if (out.length() > 0) out.append(',');
                out.append(type).append('=').append(temp);
            }
        }
        if (maxTemp == Long.MIN_VALUE) return "unavailable";
        return "max=" + maxType + ':' + maxTemp + ";zones=" + out;
    }

    private final class DiagSampler {
        private volatile boolean stop;
        private Thread thread;
        private final long[] minFreq = new long[16];
        private final long[] maxFreq = new long[16];
        private int samples;

        DiagSampler() {
            Arrays.fill(minFreq, Long.MAX_VALUE);
            Arrays.fill(maxFreq, -1L);
        }

        void start() {
            stop = false;
            thread = new Thread(() -> {
                while (!stop) {
                    for (int cpu = 0; cpu < 16; cpu++) {
                        String base = "/sys/devices/system/cpu/cpu" + cpu + "/cpufreq/";
                        long cur = readLongFile(base + "scaling_cur_freq");
                        if (cur < 0) cur = readLongFile(base + "cpuinfo_cur_freq");
                        if (cur >= 0) {
                            if (cur < minFreq[cpu]) minFreq[cpu] = cur;
                            if (cur > maxFreq[cpu]) maxFreq[cpu] = cur;
                        }
                    }
                    samples++;
                    try { Thread.sleep(1000); } catch (InterruptedException ignored) { break; }
                }
            }, "speech-diag-sampler");
            thread.start();
        }

        String finish() {
            stop = true;
            if (thread != null) { thread.interrupt(); try { thread.join(1200); } catch (InterruptedException ignored) { } }
            StringBuilder out = new StringBuilder("samples=").append(samples);
            for (int cpu = 0; cpu < 16; cpu++) {
                if (maxFreq[cpu] >= 0) out.append(",cpu").append(cpu).append("_minmax=").append(minFreq[cpu]).append('/').append(maxFreq[cpu]);
            }
            return out.toString();
        }
    }

    private void copyBenchmarkExport() {
        String text;
        synchronized (benchmarkExport) { text = benchmarkExport.toString(); }
        if (text.isEmpty()) {
            Toast.makeText(this, "Noch keine Benchmark-Daten vorhanden.", Toast.LENGTH_SHORT).show();
            return;
        }
        ClipboardManager clipboard = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(ClipData.newPlainText("Speech Notes Benchmark v1.8.2", text));
        Toast.makeText(this, "Benchmark-Daten kopiert.", Toast.LENGTH_SHORT).show();
    }

'''
s = s[:start] + new_method + s[end:]

old_finish_tail = '''            LinearLayout.LayoutParams p = matchWrap(); p.topMargin = dp(8); results.addView(winner, p);
        }
    }'''
new_finish_tail = '''            LinearLayout.LayoutParams p = matchWrap(); p.topMargin = dp(8); results.addView(winner, p);
        }
        if (copyButton != null && benchmarkExport.length() > 0) { copyButton.setEnabled(true); copyButton.setAlpha(1f); }
    }'''
if old_finish_tail not in s:
    raise SystemExit('finishBenchmark export enable anchor not found')
s = s.replace(old_finish_tail, new_finish_tail, 1)

old_fail = '''        results.removeAllViews(); results.addView(text(t.getMessage() == null ? t.toString() : t.getMessage(), 13, DANGER, false));
    }'''
new_fail = '''        results.removeAllViews(); results.addView(text(t.getMessage() == null ? t.toString() : t.getMessage(), 13, DANGER, false));
        if (copyButton != null && benchmarkExport.length() > 0) { copyButton.setEnabled(true); copyButton.setAlpha(1f); }
    }'''
if old_fail not in s:
    raise SystemExit('failBenchmark export enable anchor not found')
s = s.replace(old_fail, new_fail, 1)

bench.write_text(s)
print('Applied v1.8.2 runtime diagnostics lab + copyable benchmark export')
