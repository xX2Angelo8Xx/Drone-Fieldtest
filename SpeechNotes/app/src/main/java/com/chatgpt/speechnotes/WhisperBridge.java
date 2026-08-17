package com.chatgpt.speechnotes;

public final class WhisperBridge {
    static { System.loadLibrary("whisper_jni"); }
    private WhisperBridge() {}

    public static final class Result {
        public String text = "";
        public long pcmMs;
        public long whisperMs;
        public long encodeMs;
        public long decodeMs;
        public long sampleMs;
        public long batchMs;
        public long promptMs;
    }

    public static native long loadModel(String modelPath);
    public static native void unloadModel();
    public static native boolean isModelLoaded(String modelPath);
    public static native long currentModelLoadMs();

    private static native String transcribeLoadedRaw(
            String pcmPath, String language, String initialPrompt, int threads);

    public static Result transcribeLoaded(String pcmPath, String language,
                                          String initialPrompt, int threads) {
        String raw = transcribeLoadedRaw(pcmPath, language, initialPrompt, threads);
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
                case "encode": r.encodeMs = v; break;
                case "decode": r.decodeMs = v; break;
                case "sample": r.sampleMs = v; break;
                case "batch": r.batchMs = v; break;
                case "prompt": r.promptMs = v; break;
            }
        }
        return r;
    }
}
