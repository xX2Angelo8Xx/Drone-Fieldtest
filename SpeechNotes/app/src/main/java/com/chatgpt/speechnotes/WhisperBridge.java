package com.chatgpt.speechnotes;

public final class WhisperBridge {
    static {
        System.loadLibrary("whisper_jni");
    }

    private WhisperBridge() {}

    public static native String transcribePcm16(
            String modelPath,
            String pcmPath,
            String language,
            String initialPrompt,
            int threads);
}
