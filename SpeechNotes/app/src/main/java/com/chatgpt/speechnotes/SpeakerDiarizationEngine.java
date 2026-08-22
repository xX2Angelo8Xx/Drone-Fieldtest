package com.chatgpt.speechnotes;

import android.content.Context;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Online diarization for anonymous speakers. Architecture is generic List<SpeakerCluster>,
 * while this product revision intentionally caps the session at three speakers.
 */
public final class SpeakerDiarizationEngine implements AutoCloseable {
    public static final int MAX_SPEAKERS = 3;
    private static final int SAMPLE_RATE = 16000;
    private static final int FRAME = 480; // 30 ms
    private static final int MIN_SPEECH = 10400; // 650 ms
    private static final int MAX_CHUNK = 56000;  // 3.5 s
    private static final int MERGE_GAP = 8000;   // 500 ms
    private static final float CREATE_THRESHOLD = 0.46f;

    public static final class SpeakerTurn {
        public final long startSample;
        public final long endSample;
        public final int speakerId;
        public final float confidence;

        SpeakerTurn(long startSample, long endSample, int speakerId, float confidence) {
            this.startSample = startSample;
            this.endSample = endSample;
            this.speakerId = speakerId;
            this.confidence = confidence;
        }
    }

    private static final class Observation {
        final long start;
        final long end;
        final float[] embedding;
        int speakerId;
        float similarity;

        Observation(long start, long end, float[] embedding, int speakerId, float similarity) {
            this.start = start;
            this.end = end;
            this.embedding = embedding;
            this.speakerId = speakerId;
            this.similarity = similarity;
        }
    }

    private final SpeakerEmbeddingModel encoder;
    private final List<SpeakerCluster> clusters = new ArrayList<>();
    private final List<Observation> observations = new ArrayList<>();
    private final List<SpeakerTurn> turns = new ArrayList<>();
    private long processedUntilSample;
    private long embeddingWallMs;
    private int embeddingRuns;

    public SpeakerDiarizationEngine(Context context) throws Exception {
        encoder = new SpeakerEmbeddingModel(context);
    }

    public synchronized int speakerCount() { return clusters.size(); }
    public synchronized List<SpeakerCluster> clusters() { return Collections.unmodifiableList(new ArrayList<>(clusters)); }
    public synchronized List<SpeakerTurn> turns() { return Collections.unmodifiableList(new ArrayList<>(turns)); }
    public synchronized long embeddingWallMs() { return embeddingWallMs; }
    public synchronized int embeddingRuns() { return embeddingRuns; }

    /** Analyze only audio that has not already been consumed from overlapping Whisper windows. */
    public synchronized void analyzeWindow(short[] pcm, long absoluteStartSample) throws Exception {
        if (pcm == null || pcm.length == 0) return;
        long absoluteEnd = absoluteStartSample + pcm.length;
        int skip = (int) Math.max(0L, Math.min((long) pcm.length, processedUntilSample - absoluteStartSample));
        if (skip >= pcm.length) return;
        short[] fresh = Arrays.copyOfRange(pcm, skip, pcm.length);
        long freshStart = absoluteStartSample + skip;
        for (Range r : detectSpeech(fresh)) {
            int localStart = r.start;
            int localEnd = r.end;
            while (localEnd - localStart > MAX_CHUNK) {
                processChunk(fresh, localStart, localStart + MAX_CHUNK, freshStart);
                localStart += MAX_CHUNK;
            }
            if (localEnd - localStart >= MIN_SPEECH) processChunk(fresh, localStart, localEnd, freshStart);
        }
        processedUntilSample = Math.max(processedUntilSample, absoluteEnd);
        rebuildTurns();
    }

    private void processChunk(short[] pcm, int start, int end, long absoluteBase) throws Exception {
        short[] speech = Arrays.copyOfRange(pcm, start, end);
        long wall = android.os.SystemClock.elapsedRealtime();
        float[] embedding = encoder.embed(speech);
        embeddingWallMs += android.os.SystemClock.elapsedRealtime() - wall;
        embeddingRuns++;

        Assignment a = assign(embedding);
        long absStart = absoluteBase + start;
        long absEnd = absoluteBase + end;
        observations.add(new Observation(absStart, absEnd, embedding, a.speakerId, a.similarity));
    }

    private Assignment assign(float[] embedding) {
        if (clusters.isEmpty()) {
            clusters.add(new SpeakerCluster(1, embedding));
            return new Assignment(1, 1f);
        }
        SpeakerCluster best = null;
        float bestSim = -1f;
        for (SpeakerCluster c : clusters) {
            float sim = c.similarity(embedding);
            if (sim > bestSim) { bestSim = sim; best = c; }
        }
        if (bestSim < CREATE_THRESHOLD && clusters.size() < MAX_SPEAKERS) {
            SpeakerCluster created = new SpeakerCluster(clusters.size() + 1, embedding);
            clusters.add(created);
            return new Assignment(created.id(), 1f);
        }
        if (best == null) throw new IllegalStateException("Speaker assignment failed");
        best.update(embedding);
        return new Assignment(best.id(), bestSim);
    }

    /**
     * Session-final refinement: re-estimate centroids and assignments over all observations.
     * This keeps online responsiveness but fixes centroid drift after the full conversation is known.
     */
    public synchronized void refineGlobal() {
        if (observations.size() < 2 || clusters.isEmpty()) { rebuildTurns(); return; }
        for (int pass = 0; pass < 3; pass++) {
            List<float[]> sums = new ArrayList<>();
            int[] counts = new int[clusters.size()];
            for (SpeakerCluster c : clusters) sums.add(new float[c.centroidCopy().length]);

            for (Observation o : observations) {
                int bestIndex = 0;
                float best = -1f;
                for (int i = 0; i < clusters.size(); i++) {
                    float sim = clusters.get(i).similarity(o.embedding);
                    if (sim > best) { best = sim; bestIndex = i; }
                }
                o.speakerId = clusters.get(bestIndex).id();
                o.similarity = best;
                float[] sum = sums.get(bestIndex);
                for (int j = 0; j < sum.length; j++) sum[j] += o.embedding[j];
                counts[bestIndex]++;
            }
            for (int i = 0; i < clusters.size(); i++) {
                if (counts[i] == 0) continue;
                float[] mean = sums.get(i);
                for (int j = 0; j < mean.length; j++) mean[j] /= counts[i];
                clusters.get(i).resetTo(mean, counts[i]);
            }
        }
        rebuildTurns();
    }

    public synchronized int speakerForInterval(long startSample, long endSample) {
        long bestOverlap = 0L;
        int bestSpeaker = 0;
        for (SpeakerTurn t : turns) {
            long overlap = Math.max(0L, Math.min(endSample, t.endSample) - Math.max(startSample, t.startSample));
            if (overlap > bestOverlap) { bestOverlap = overlap; bestSpeaker = t.speakerId; }
        }
        return bestSpeaker;
    }

    private void rebuildTurns() {
        turns.clear();
        observations.sort(Comparator.comparingLong(o -> o.start));
        for (Observation o : observations) {
            SpeakerTurn next = new SpeakerTurn(o.start, o.end, o.speakerId, confidence(o.similarity));
            if (!turns.isEmpty()) {
                SpeakerTurn prev = turns.get(turns.size() - 1);
                if (prev.speakerId == next.speakerId && next.startSample - prev.endSample <= MERGE_GAP) {
                    turns.set(turns.size() - 1, new SpeakerTurn(prev.startSample,
                            Math.max(prev.endSample, next.endSample), prev.speakerId,
                            Math.min(prev.confidence, next.confidence)));
                    continue;
                }
            }
            turns.add(next);
        }
    }

    private static float confidence(float sim) {
        if (sim < 0f) return 0f;
        return Math.max(0f, Math.min(1f, (sim - 0.25f) / 0.55f));
    }

    private static final class Assignment {
        final int speakerId; final float similarity;
        Assignment(int speakerId, float similarity) { this.speakerId = speakerId; this.similarity = similarity; }
    }
    private static final class Range {
        final int start, end;
        Range(int start, int end) { this.start = start; this.end = end; }
    }

    /** Adaptive energy VAD: cheap enough to run beside Whisper; neural embeddings do speaker identity. */
    private static List<Range> detectSpeech(short[] pcm) {
        int frames = pcm.length / FRAME;
        if (frames == 0) return Collections.emptyList();
        double[] db = new double[frames];
        double[] sorted = new double[frames];
        for (int f = 0; f < frames; f++) {
            double sum = 0.0;
            int base = f * FRAME;
            for (int i = 0; i < FRAME; i++) {
                double x = pcm[base + i] / 32768.0;
                sum += x * x;
            }
            double rms = Math.sqrt(sum / FRAME);
            db[f] = 20.0 * Math.log10(Math.max(rms, 1e-7));
            sorted[f] = db[f];
        }
        Arrays.sort(sorted);
        double noise = sorted[Math.min(sorted.length - 1, Math.max(0, sorted.length / 5))];
        double threshold = Math.max(-43.0, Math.min(-25.0, noise + 9.0));
        int hangFrames = 7;
        int start = -1, hang = 0;
        List<Range> out = new ArrayList<>();
        for (int f = 0; f < frames; f++) {
            boolean speech = db[f] >= threshold;
            if (speech) {
                if (start < 0) start = Math.max(0, f - 2);
                hang = hangFrames;
            } else if (start >= 0) {
                if (hang > 0) hang--;
                else {
                    int s = start * FRAME;
                    int e = Math.min(pcm.length, f * FRAME);
                    if (e - s >= MIN_SPEECH) out.add(new Range(s, e));
                    start = -1;
                }
            }
        }
        if (start >= 0) {
            int s = start * FRAME;
            if (pcm.length - s >= MIN_SPEECH) out.add(new Range(s, pcm.length));
        }
        return out;
    }

    @Override public synchronized void close() throws Exception { encoder.close(); }
}
