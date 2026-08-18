package com.chatgpt.speechnotes;

public final class SpeechRuntimeConfig {
    private SpeechRuntimeConfig() {}

    public static final String DEFAULT_MODEL = "large-v3-turbo-q4_0";
    public static final String BACKEND = WhisperBridge.BACKEND_V90;
    public static final String LANGUAGE = "de";
    public static final int TURBO_THREADS = 4;
    public static final int TURBO_AUDIO_CTX = 1280;
    public static final int TURBO_FAST_CORES = 0;

    public static boolean isTurbo(String model) {
        return model != null && model.startsWith("large-v3-turbo");
    }

    public static String backendFor(String model) {
        return BACKEND;
    }

    public static int threadsFor(String model) {
        if (isTurbo(model)) return TURBO_THREADS;
        int cores = Math.max(1, Runtime.getRuntime().availableProcessors());
        return Math.max(2, Math.min(6, Math.max(2, cores - 2)));
    }

    public static int audioCtxFor(String model) {
        return isTurbo(model) ? TURBO_AUDIO_CTX : 0;
    }

    public static int fastCoresFor(String model) {
        return 0;
    }

    public static WhisperBridge.Result transcribe(String pcmPath, String model, String prompt) {
        return WhisperBridge.transcribeLoadedProfile(
                pcmPath,
                LANGUAGE,
                prompt == null ? "" : prompt,
                threadsFor(model),
                audioCtxFor(model),
                fastCoresFor(model));
    }

    public static String runtimeLabel(String model) {
        return "backend=" + backendFor(model)
                + " · threads=" + threadsFor(model)
                + " · ctx=" + audioCtxFor(model)
                + " · affinity=none";
    }
}
