package com.chatgpt.speechnotes;

import java.util.Arrays;

/** Mutable centroid for one anonymous speaker within a recording session. */
public final class SpeakerCluster {
    private final int id;
    private final float[] centroid;
    private int observations;

    public SpeakerCluster(int id, float[] firstEmbedding) {
        if (firstEmbedding == null || firstEmbedding.length == 0) {
            throw new IllegalArgumentException("Speaker embedding is empty");
        }
        this.id = id;
        this.centroid = Arrays.copyOf(firstEmbedding, firstEmbedding.length);
        normalize(this.centroid);
        this.observations = 1;
    }

    public int id() { return id; }
    public int observations() { return observations; }
    public float[] centroidCopy() { return Arrays.copyOf(centroid, centroid.length); }

    public float similarity(float[] embedding) {
        if (embedding == null || embedding.length != centroid.length) return -1f;
        double dot = 0.0;
        double norm = 0.0;
        for (int i = 0; i < centroid.length; i++) {
            dot += centroid[i] * embedding[i];
            norm += embedding[i] * embedding[i];
        }
        if (norm <= 1e-12) return -1f;
        return (float) (dot / Math.sqrt(norm));
    }

    public void update(float[] embedding) {
        if (embedding == null || embedding.length != centroid.length) return;
        float alpha = 1.0f / Math.min(12, observations + 1);
        for (int i = 0; i < centroid.length; i++) {
            centroid[i] = centroid[i] * (1.0f - alpha) + embedding[i] * alpha;
        }
        normalize(centroid);
        observations++;
    }

    public void resetTo(float[] value, int count) {
        if (value == null || value.length != centroid.length) return;
        System.arraycopy(value, 0, centroid, 0, centroid.length);
        normalize(centroid);
        observations = Math.max(1, count);
    }

    static void normalize(float[] v) {
        double sum = 0.0;
        for (float x : v) sum += x * x;
        double n = Math.sqrt(sum);
        if (n <= 1e-12) return;
        for (int i = 0; i < v.length; i++) v[i] /= (float) n;
    }
}
