package com.chatgpt.speechnotes;

public final class WhisperBridge {
    static { System.loadLibrary("whisper_jni"); }
    private WhisperBridge() {}

    public static final String BACKEND_GENERIC = "generic";
    public static final String BACKEND_BEST = "best";

    public static final class Result {
        // Compatibility alias for diagnostics/export code: Result already contains
        // the parsed timing profile fields directly, so profile simply refers to self.
        public final Result profile = this;
        public String text = "";
        public long pcmMs;
        public long whisperMs;
        public long nativeTotalMs;
        public long melMs;

        public long encodeMs;
        public long encodeTotalMs;
        public int encodeRuns;
        public long decodeMs;
        public long decodeTotalMs;
        public int decodeRuns;
        public long sampleMs;
        public long sampleTotalMs;
        public int sampleRuns;
        public long batchMs;
        public long batchTotalMs;
        public int batchRuns;
        public long promptMs;
        public long promptTotalMs;
        public int promptRuns;
    }

    public static native long loadModel(String modelPath, String nativeLibraryDir, String backendMode);
    public static native void unloadModel();
    public static native boolean isModelLoaded(String modelPath, String backendMode);
    public static native long currentModelLoadMs();
    public static native String currentBackendName();

    private static native String transcribeLoadedRaw(
            String pcmPath, String language, String initialPrompt, int threads, boolean noContext);

    public static Result transcribeLoaded(String pcmPath, String language,
                                          String initialPrompt, int threads) {
        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, false));
    }

    public static Result transcribeLoadedBenchmark(String pcmPath, String language,
                                                   String initialPrompt, int threads) {
        return parseResult(transcribeLoadedRaw(pcmPath, language, initialPrompt, threads, true));
    }

    private static Result parseResult(String raw) {
        Result r = new Result();
        if (raw == null) return r;
        final String marker = "\n__SN_TEXT__\n";
        int cut = raw.indexOf(marker);
        if (cut < 0) { r.text = raw; return r; }
        String profile = raw.substring(0, cut);
        r.text = raw.substring(cut + marker.length());
        for (String part : profile.split(";")) {
            String[] kv = part.split("=", 2);
            if (kv.length != 2) continue;
            long v;
            try { v = Long.parseLong(kv[1]); } catch (NumberFormatException e) { continue; }
            switch (kv[0]) {
                case "pcm": r.pcmMs = v; break;
                case "whisper": r.whisperMs = v; break;
                case "native_total": r.nativeTotalMs = v; break;
                case "mel": r.melMs = v; break;
                case "encode": r.encodeMs = v; break;
                case "encode_total": r.encodeTotalMs = v; break;
                case "encode_n": r.encodeRuns = (int) v; break;
                case "decode": r.decodeMs = v; break;
                case "decode_total": r.decodeTotalMs = v; break;
                case "decode_n": r.decodeRuns = (int) v; break;
                case "sample": r.sampleMs = v; break;
                case "sample_total": r.sampleTotalMs = v; break;
                case "sample_n": r.sampleRuns = (int) v; break;
                case "batch": r.batchMs = v; break;
                case "batch_total": r.batchTotalMs = v; break;
                case "batch_n": r.batchRuns = (int) v; break;
                case "prompt": r.promptMs = v; break;
                case "prompt_total": r.promptTotalMs = v; break;
                case "prompt_n": r.promptRuns = (int) v; break;
            }
        }
        return r;
    }
}
