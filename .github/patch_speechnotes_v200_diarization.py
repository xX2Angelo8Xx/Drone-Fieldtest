from pathlib import Path
import re


def require_replace(text, old, new, label, count=1):
    if old not in text:
        raise SystemExit(f'v2.0.0 anchor missing: {label}')
    return text.replace(old, new, count)

# -----------------------------------------------------------------------------
# Whisper segment timestamps: keep the existing one-pass inference and transport
# segment t0/t1/text metadata together with the timing profile and full text.
# -----------------------------------------------------------------------------
bridge_path = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/WhisperBridge.java')
s = bridge_path.read_text()
s = require_replace(s,
'''    public static final class Result {\n''',
'''    public static final class Segment {\n        public long t0;\n        public long t1;\n        public String text = "";\n    }\n\n    public static final class Result {\n''', 'WhisperBridge Segment class')
s = require_replace(s,
'''        public final Result profile = this;\n        public String text = "";''',
'''        public final Result profile = this;\n        public final java.util.List<Segment> segments = new java.util.ArrayList<>();\n        public String text = "";''', 'WhisperBridge Result.segments')
old_parse_head = '''        final String marker = "\\n__SN_TEXT__\\n";\n        int cut = raw.indexOf(marker);\n        if (cut < 0) { r.text = raw; return r; }\n        String profile = raw.substring(0, cut);\n        r.text = raw.substring(cut + marker.length());'''
new_parse_head = '''        final String textMarker = "\\n__SN_TEXT__\\n";\n        final String segmentMarker = "\\n__SN_SEGMENTS__\\n";\n        int cut = raw.indexOf(textMarker);\n        if (cut < 0) { r.text = raw; return r; }\n        String head = raw.substring(0, cut);\n        r.text = raw.substring(cut + textMarker.length());\n        int segCut = head.indexOf(segmentMarker);\n        String profile = segCut >= 0 ? head.substring(0, segCut) : head;\n        if (segCut >= 0) {\n            String encoded = head.substring(segCut + segmentMarker.length());\n            for (String line : encoded.split("\\n")) {\n                if (line.isEmpty()) continue;\n                String[] p = line.split(",", 3);\n                if (p.length != 3) continue;\n                try {\n                    Segment seg = new Segment();\n                    seg.t0 = Long.parseLong(p[0]);\n                    seg.t1 = Long.parseLong(p[1]);\n                    seg.text = hexDecode(p[2]);\n                    r.segments.add(seg);\n                } catch (RuntimeException ignored) { }\n            }\n        }'''
s = require_replace(s, old_parse_head, new_parse_head, 'WhisperBridge parse segment metadata')
end_anchor = '''        return r;\n    }\n}'''
end_new = '''        return r;\n    }\n\n    private static String hexDecode(String hex) {\n        if (hex == null || (hex.length() & 1) != 0) return "";\n        byte[] bytes = new byte[hex.length() / 2];\n        for (int i = 0; i < bytes.length; i++) {\n            int hi = Character.digit(hex.charAt(i * 2), 16);\n            int lo = Character.digit(hex.charAt(i * 2 + 1), 16);\n            if (hi < 0 || lo < 0) return "";\n            bytes[i] = (byte) ((hi << 4) | lo);\n        }\n        return new String(bytes, java.nio.charset.StandardCharsets.UTF_8);\n    }\n}'''
s = require_replace(s, end_anchor, end_new, 'WhisperBridge hex decoder')
bridge_path.write_text(s)

jni_path = Path('SpeechNotes/app/src/main/cpp/whisper_jni.cpp')
s = jni_path.read_text()
hex_anchor = '''static long long elapsed_ms(std::chrono::steady_clock::time_point start) {\n    return std::chrono::duration_cast<std::chrono::milliseconds>(\n            std::chrono::steady_clock::now() - start).count();\n}\n'''
hex_new = hex_anchor + '''\nstatic std::string sn_hex(const std::string &value) {\n    static const char *digits = "0123456789abcdef";\n    std::string out;\n    out.reserve(value.size() * 2);\n    for (unsigned char c : value) {\n        out.push_back(digits[(c >> 4) & 0x0f]);\n        out.push_back(digits[c & 0x0f]);\n    }\n    return out;\n}\n'''
s = require_replace(s, hex_anchor, hex_new, 'JNI hex encoder')
segment_loop = '''    std::string text;\n    const int n = whisper_full_n_segments(g_ctx);\n    for (int i = 0; i < n; ++i) {\n        const char *seg = whisper_full_get_segment_text(g_ctx, i);\n        if (seg) text += seg;\n    }'''
segment_loop_new = '''    std::string text;\n    std::ostringstream segmentMeta;\n    const int n = whisper_full_n_segments(g_ctx);\n    for (int i = 0; i < n; ++i) {\n        const char *seg = whisper_full_get_segment_text(g_ctx, i);\n        if (seg) {\n            text += seg;\n            segmentMeta << whisper_full_get_segment_t0(g_ctx, i) << ","\n                        << whisper_full_get_segment_t1(g_ctx, i) << ","\n                        << sn_hex(seg) << "\\n";\n        }\n    }'''
s = require_replace(s, segment_loop, segment_loop_new, 'JNI segment timestamp collection')
s = require_replace(s,
'''        << "\\n__SN_TEXT__\\n" << text;''',
'''        << "\\n__SN_SEGMENTS__\\n" << segmentMeta.str()\n        << "__SN_TEXT__\\n" << text;''', 'JNI segment payload marker')
jni_path.write_text(s)

# -----------------------------------------------------------------------------
# RecordingService: neural speaker embeddings + generic List<SpeakerCluster>
# online tracking, final global refinement, and Whisper timestamp fusion.
# -----------------------------------------------------------------------------
service_path = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/RecordingService.java')
s = service_path.read_text()
s = require_replace(s,
'''    public static final String EXTRA_AVIATION_PROMPT = "aviation_prompt";''',
'''    public static final String EXTRA_AVIATION_PROMPT = "aviation_prompt";\n    public static final String EXTRA_DIARIZATION = "diarization";''', 'RecordingService diarization extra')
s = require_replace(s,
'''    private boolean aviationPrompt;\n    private long startedWall;''',
'''    private boolean aviationPrompt;\n    private boolean speakerDiarization = true;\n    private SpeakerDiarizationEngine diarizer;\n    private SpeakerTranscriptFusion speakerFusion;\n    private boolean speakerRuntimeFailed;\n    private long startedWall;''', 'RecordingService speaker fields')
s = require_replace(s,
'''            aviationPrompt = intent.getBooleanExtra(EXTRA_AVIATION_PROMPT, false);\n            startRecording();''',
'''            aviationPrompt = intent.getBooleanExtra(EXTRA_AVIATION_PROMPT, false);\n            speakerDiarization = intent.getBooleanExtra(EXTRA_DIARIZATION, true);\n            startRecording();''', 'RecordingService read diarization option')
init_anchor = '''            ring = new StreamingAudioBuffer(SAMPLE_RATE * RING_SECONDS);\n            inferenceQueue.clear();'''
init_new = '''            ring = new StreamingAudioBuffer(SAMPLE_RATE * RING_SECONDS);\n            speakerFusion = new SpeakerTranscriptFusion();\n            speakerRuntimeFailed = false;\n            closeSpeakerRuntime();\n            if (speakerDiarization) {\n                long speakerLoadStart = SystemClock.elapsedRealtime();\n                diarizer = new SpeakerDiarizationEngine(this);\n                CrashDiagnostics.mark(this, "speaker:runtime_ready load_ms="\n                        + (SystemClock.elapsedRealtime() - speakerLoadStart) + " max=3");\n            }\n            inferenceQueue.clear();'''
s = require_replace(s, init_anchor, init_new, 'RecordingService speaker runtime init')
process_anchor = '''        String text = result.text == null ? "" : result.text.trim();\n        assembler.merge(text);\n        lastProcessedEnd = job.endSample;'''
process_new = '''        String text = result.text == null ? "" : result.text.trim();\n        assembler.merge(text);\n        if (speakerDiarization && diarizer != null && !speakerRuntimeFailed) {\n            try {\n                diarizer.analyzeWindow(job.pcm, job.startSample);\n                speakerFusion.acceptWindow(result, job.startSample, diarizer);\n                if (job.terminal) diarizer.refineGlobal();\n            } catch (Throwable speakerError) {\n                speakerRuntimeFailed = true;\n                CrashDiagnostics.markError(this, "speaker:runtime_failed_fallback_plain", speakerError);\n                closeSpeakerRuntime();\n            }\n        }\n        lastProcessedEnd = job.endSample;'''
s = require_replace(s, process_anchor, process_new, 'RecordingService speaker analysis in inference worker')
# Route every live/final transcript through one policy function; plain streaming remains fallback.
s = s.replace('assembler.text()', 'currentTranscript()')
status_anchor = '''    private void updateStreamingStatus(long capturedSamples) {'''
helpers = '''    private String currentTranscript() {\n        if (speakerDiarization && !speakerRuntimeFailed && diarizer != null && speakerFusion != null) {\n            String speakerText = speakerFusion.text(diarizer);\n            if (!speakerText.isEmpty()) return speakerText;\n        }\n        return assembler.text();\n    }\n\n    private void closeSpeakerRuntime() {\n        SpeakerDiarizationEngine d = diarizer;\n        diarizer = null;\n        if (d != null) {\n            try { d.close(); } catch (Throwable ignored) { }\n        }\n    }\n\n'''
if status_anchor not in s:
    raise SystemExit('v2.0.0 anchor missing: RecordingService status method')
s = s.replace(status_anchor, helpers + status_anchor, 1)
status_tail = '''                + (windowsDropped > 0 ? " · Drops " + windowsDropped : "");'''
status_tail_new = '''                + (windowsDropped > 0 ? " · Drops " + windowsDropped : "")\n                + (speakerDiarization\n                    ? (speakerRuntimeFailed ? " · Speaker Fallback"\n                        : (diarizer == null ? " · Speaker init"\n                            : " · Spk " + diarizer.speakerCount() + "/3 · Emb "\n                                + diarizer.embeddingRuns() + "×/" + diarizer.embeddingWallMs() + "ms"))\n                    : "");'''
s = require_replace(s, status_tail, status_tail_new, 'RecordingService speaker telemetry')
finish_diag = '''            CrashDiagnostics.mark(this, "stream:done windows=" + windowsProcessed\n                    + " drops=" + windowsDropped + " total_inference_ms=" + totalInferenceWallMs);'''
finish_diag_new = '''            CrashDiagnostics.mark(this, "stream:done windows=" + windowsProcessed\n                    + " drops=" + windowsDropped + " total_inference_ms=" + totalInferenceWallMs\n                    + " speakers=" + (diarizer == null ? 0 : diarizer.speakerCount())\n                    + " speaker_embed_runs=" + (diarizer == null ? 0 : diarizer.embeddingRuns())\n                    + " speaker_embed_ms=" + (diarizer == null ? 0 : diarizer.embeddingWallMs())\n                    + " speaker_fallback=" + speakerRuntimeFailed);'''
s = require_replace(s, finish_diag, finish_diag_new, 'RecordingService final speaker telemetry')
# Clean native ONNX session on all terminal paths.
s = require_replace(s,
'''            if (pcmFile != null) pcmFile.delete();\n            releaseWakeLock();''',
'''            if (pcmFile != null) pcmFile.delete();\n            closeSpeakerRuntime();\n            releaseWakeLock();''', 'RecordingService finish speaker cleanup')
s = require_replace(s,
'''        releaseRecorder();\n        releaseWakeLock();\n        broadcast("error", safeMessage(t));''',
'''        releaseRecorder();\n        closeSpeakerRuntime();\n        releaseWakeLock();\n        broadcast("error", safeMessage(t));''', 'RecordingService failure speaker cleanup')
service_path.write_text(s)

# -----------------------------------------------------------------------------
# MainActivity: explicit user-facing diarization switch. Default ON; max=3 is
# fixed in this revision, while the engine itself remains generic.
# -----------------------------------------------------------------------------
main_path = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/MainActivity.java')
s = main_path.read_text()
s = require_replace(s,
'''    private Switch aviationSwitch;''',
'''    private Switch aviationSwitch;\n    private Switch speakerSwitch;''', 'MainActivity speaker switch field')
toggle_anchor = '''        root.addView(aviationCard, matchWrap());\n\n        root.addView(space(10));\n        LinearLayout wavCard = toggleCard("WAV zusätzlich speichern", "16 kHz · Mono · PCM16");'''
toggle_new = '''        root.addView(aviationCard, matchWrap());\n\n        root.addView(space(10));\n        LinearLayout speakerCard = toggleCard("Sprechertrennung",\n                "On-device CAM++ · automatisch · maximal 3 Sprecher");\n        speakerSwitch = new Switch(this);\n        speakerSwitch.setChecked(prefs.getBoolean("speaker_diarization", true));\n        speakerCard.addView(speakerSwitch);\n        root.addView(speakerCard, matchWrap());\n\n        root.addView(space(10));\n        LinearLayout wavCard = toggleCard("WAV zusätzlich speichern", "16 kHz · Mono · PCM16");'''
s = require_replace(s, toggle_anchor, toggle_new, 'MainActivity speaker toggle card')
launch_anchor = '''        boolean useAviation = aviationSwitch.isChecked();\n        boolean save = wavSwitch.isChecked();\n        prefs.edit().putInt("model_v2", idx)\n                .putBoolean("wav", save)\n                .putBoolean("aviation_prompt", useAviation).apply();'''
launch_new = '''        boolean useAviation = aviationSwitch.isChecked();\n        boolean useSpeakers = speakerSwitch != null && speakerSwitch.isChecked();\n        boolean save = wavSwitch.isChecked();\n        prefs.edit().putInt("model_v2", idx)\n                .putBoolean("wav", save)\n                .putBoolean("aviation_prompt", useAviation)\n                .putBoolean("speaker_diarization", useSpeakers).apply();'''
s = require_replace(s, launch_anchor, launch_new, 'MainActivity persist speaker option')
intent_anchor = '''                .putExtra(RecordingService.EXTRA_SAVE_WAV, save)\n                .putExtra(RecordingService.EXTRA_AVIATION_PROMPT, useAviation);'''
intent_new = '''                .putExtra(RecordingService.EXTRA_SAVE_WAV, save)\n                .putExtra(RecordingService.EXTRA_AVIATION_PROMPT, useAviation)\n                .putExtra(RecordingService.EXTRA_DIARIZATION, useSpeakers);'''
s = require_replace(s, intent_anchor, intent_new, 'MainActivity diarization intent extra')
# Keep the new control lifecycle-locked exactly like the other recording options.
s = s.replace('modelSpinner.setEnabled(false); wavSwitch.setEnabled(false); aviationSwitch.setEnabled(false);',
              'modelSpinner.setEnabled(false); wavSwitch.setEnabled(false); aviationSwitch.setEnabled(false); if (speakerSwitch != null) speakerSwitch.setEnabled(false);')
s = s.replace('modelSpinner.setEnabled(true); wavSwitch.setEnabled(true); aviationSwitch.setEnabled(true);',
              'modelSpinner.setEnabled(true); wavSwitch.setEnabled(true); aviationSwitch.setEnabled(true); if (speakerSwitch != null) speakerSwitch.setEnabled(true);')
s = s.replace('wavSwitch.setEnabled(true); aviationSwitch.setEnabled(true); refreshModelState();',
              'wavSwitch.setEnabled(true); aviationSwitch.setEnabled(true); if (speakerSwitch != null) speakerSwitch.setEnabled(true); refreshModelState();')
main_path.write_text(s)

# -----------------------------------------------------------------------------
# Android ONNX Runtime + version bump. AndroidX dependency already exists after
# the v1.8.5 patch; append deterministically instead of replacing the block.
# -----------------------------------------------------------------------------
gradle_path = Path('SpeechNotes/app/build.gradle')
g = gradle_path.read_text()
onnx_dep = "implementation 'com.microsoft.onnxruntime:onnxruntime-android:1.27.0'"
if onnx_dep not in g:
    dep_match = re.search(r'dependencies\s*\{', g)
    if not dep_match:
        raise SystemExit('v2.0.0 dependencies block missing after v1.8.5')
    insert_at = dep_match.end()
    g = g[:insert_at] + "\n    " + onnx_dep + g[insert_at:]
if not re.search(r"versionName\s+['\"]1\.9\.0['\"]", g):
    raise SystemExit('v2.0.0 expected versionName 1.9.0 not found')
g = re.sub(r"versionName\s+['\"]1\.9\.0['\"]", "versionName '2.0.0'", g, count=1)
m = re.search(r'versionCode\s+(\d+)', g)
if not m:
    raise SystemExit('v2.0.0 versionCode missing')
new_code = int(m.group(1)) + 1
g = g[:m.start()] + f'versionCode {new_code}' + g[m.end():]
gradle_path.write_text(g)

print('Applied v2.0.0 max-3 speaker diarization: timestamps + CAM++ embeddings + List<SpeakerCluster> + online tracking + global refinement + transcript fusion')
