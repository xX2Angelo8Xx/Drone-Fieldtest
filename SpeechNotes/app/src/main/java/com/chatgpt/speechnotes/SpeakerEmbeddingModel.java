package com.chatgpt.speechnotes;

import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.FloatBuffer;
import java.util.Collections;

import ai.onnxruntime.OnnxTensor;
import ai.onnxruntime.OnnxValue;
import ai.onnxruntime.OrtEnvironment;
import ai.onnxruntime.OrtSession;

/** Session-scoped CAM++ speaker encoder. Input is 16 kHz PCM16, output is an L2-normalized embedding. */
public final class SpeakerEmbeddingModel implements AutoCloseable {
    private static final String ASSET = "speaker/campplus_cn_en_common_200k.onnx";
    private static final String FILE_NAME = "campplus_cn_en_common_200k.onnx";

    private final OrtEnvironment env;
    private final OrtSession session;
    private final String inputName;

    public SpeakerEmbeddingModel(Context context) throws Exception {
        File model = materializeAsset(context);
        env = OrtEnvironment.getEnvironment();
        OrtSession.SessionOptions options = new OrtSession.SessionOptions();
        options.setIntraOpNumThreads(1);
        options.setInterOpNumThreads(1);
        options.setOptimizationLevel(OrtSession.SessionOptions.OptLevel.ALL_OPT);
        session = env.createSession(model.getAbsolutePath(), options);
        if (session.getInputNames().isEmpty()) throw new IllegalStateException("Speaker ONNX has no input");
        inputName = session.getInputNames().iterator().next();
    }

    public synchronized float[] embed(short[] pcm) throws Exception {
        float[][] feats = FbankExtractor.extract(pcm);
        if (feats.length < 20) throw new IllegalArgumentException("Speaker segment too short for embedding");
        float[] flat = FbankExtractor.flatten(feats);
        long[] shape = new long[]{1L, feats.length, 80L};
        try (OnnxTensor input = OnnxTensor.createTensor(env, FloatBuffer.wrap(flat), shape);
             OrtSession.Result result = session.run(Collections.singletonMap(inputName, input))) {
            if (result.size() == 0) throw new IllegalStateException("Speaker ONNX returned no output");
            OnnxValue output = result.get(0);
            Object value = output.getValue();
            float[] embedding = unpack(value);
            SpeakerCluster.normalize(embedding);
            return embedding;
        }
    }

    private static float[] unpack(Object value) {
        if (value instanceof float[]) return ((float[]) value).clone();
        if (value instanceof float[][]) {
            float[][] a = (float[][]) value;
            if (a.length == 0) throw new IllegalStateException("Empty speaker embedding output");
            return a[0].clone();
        }
        throw new IllegalStateException("Unexpected speaker embedding output: " +
                (value == null ? "null" : value.getClass().getName()));
    }

    private static File materializeAsset(Context context) throws Exception {
        File dir = new File(context.getFilesDir(), "speaker-models");
        if (!dir.exists() && !dir.mkdirs()) throw new IllegalStateException("Cannot create speaker model directory");
        File out = new File(dir, FILE_NAME);
        if (out.exists() && out.length() > 20_000_000L) return out;
        File tmp = new File(dir, FILE_NAME + ".tmp");
        try (InputStream in = context.getAssets().open(ASSET);
             FileOutputStream fos = new FileOutputStream(tmp)) {
            byte[] buffer = new byte[128 * 1024];
            int n;
            while ((n = in.read(buffer)) >= 0) fos.write(buffer, 0, n);
            fos.getFD().sync();
        }
        if (tmp.length() < 20_000_000L) {
            tmp.delete();
            throw new IllegalStateException("Speaker model asset is incomplete");
        }
        if (out.exists() && !out.delete()) throw new IllegalStateException("Cannot replace speaker model");
        if (!tmp.renameTo(out)) throw new IllegalStateException("Cannot install speaker model");
        return out;
    }

    @Override public synchronized void close() throws Exception {
        session.close();
    }
}
