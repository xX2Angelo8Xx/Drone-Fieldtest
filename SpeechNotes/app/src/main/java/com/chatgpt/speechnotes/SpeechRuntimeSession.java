package com.chatgpt.speechnotes;

import android.content.Context;

import java.io.File;

/** Single source of truth for the productive Whisper runtime state. */
public final class SpeechRuntimeSession {
    private SpeechRuntimeSession() {}

    public static final class Snapshot {
        public final String modelPath;
        public final String backendMode;
        public final String backendName;
        public final long modelLoadMs;

        Snapshot(String modelPath, String backendMode, String backendName, long modelLoadMs) {
            this.modelPath = modelPath == null ? "" : modelPath;
            this.backendMode = backendMode == null ? "" : backendMode;
            this.backendName = backendName == null ? "" : backendName;
            this.modelLoadMs = modelLoadMs;
        }

        public boolean matches(File modelFile, String model) {
            if (modelFile == null) return false;
            return modelFile.getAbsolutePath().equals(modelPath)
                    && SpeechRuntimeConfig.backendFor(model).equals(backendMode);
        }

        public String describe() {
            return "model=" + new File(modelPath).getName()
                    + " backend_mode=" + backendMode
                    + " backend_name=" + backendName
                    + " load_ms=" + modelLoadMs;
        }
    }

    public static synchronized Snapshot snapshot() {
        return new Snapshot(
                WhisperBridge.currentModelPath(),
                WhisperBridge.currentBackendMode(),
                WhisperBridge.currentBackendName(),
                WhisperBridge.currentModelLoadMs());
    }

    public static synchronized boolean isReady(File modelFile, String model) {
        Snapshot s = snapshot();
        return s.matches(modelFile, model)
                && WhisperBridge.isModelLoaded(modelFile.getAbsolutePath(), SpeechRuntimeConfig.backendFor(model));
    }

    /**
     * Ensure the requested model/backend is the active native context. If a benchmark or
     * previous screen changed the native context, this method transparently restores the
     * productive session instead of exposing stale Java/UI state.
     */
    public static synchronized long ensureLoaded(Context context, String model) throws Exception {
        File modelFile = ModelManager.ensureModel(context, model);
        String backend = SpeechRuntimeConfig.backendFor(model);
        if (WhisperBridge.isModelLoaded(modelFile.getAbsolutePath(), backend)) {
            return 0L;
        }
        long loadMs = WhisperBridge.loadModel(
                modelFile.getAbsolutePath(),
                context.getApplicationInfo().nativeLibraryDir,
                backend);
        if (loadMs < 0) {
            throw new IllegalStateException("Whisper-Context konnte nicht initialisiert werden (" + loadMs + ")");
        }
        Snapshot s = snapshot();
        if (!s.matches(modelFile, model)) {
            throw new IllegalStateException("Runtime-State inkonsistent nach Load: expected="
                    + modelFile.getAbsolutePath() + "/" + backend + " actual=" + s.describe());
        }
        return loadMs;
    }

    public static synchronized void requireReady(File modelFile, String model) {
        if (!isReady(modelFile, model)) {
            Snapshot s = snapshot();
            throw new IllegalStateException("Runtime-State passt nicht: expected="
                    + (modelFile == null ? "null" : modelFile.getAbsolutePath()) + "/"
                    + SpeechRuntimeConfig.backendFor(model) + " actual=" + s.describe());
        }
    }
}
