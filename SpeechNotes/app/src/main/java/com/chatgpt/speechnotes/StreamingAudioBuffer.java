package com.chatgpt.speechnotes;

/** Thread-safe absolute-indexed PCM16 ring buffer used by the streaming scheduler. */
public final class StreamingAudioBuffer {
    private final short[] data;
    private long totalSamples;

    public StreamingAudioBuffer(int capacitySamples) {
        if (capacitySamples <= 0) throw new IllegalArgumentException("capacitySamples");
        data = new short[capacitySamples];
    }

    public synchronized long appendPcm16Le(byte[] bytes, int length) {
        int usable = Math.max(0, length - (length & 1));
        for (int i = 0; i < usable; i += 2) {
            int lo = bytes[i] & 0xff;
            int hi = bytes[i + 1];
            short sample = (short) (lo | (hi << 8));
            data[(int) (totalSamples % data.length)] = sample;
            totalSamples++;
        }
        return totalSamples;
    }

    public synchronized long totalSamples() { return totalSamples; }

    public synchronized short[] snapshot(long startSample, long endSample) {
        long oldest = Math.max(0L, totalSamples - data.length);
        long start = Math.max(oldest, startSample);
        long end = Math.min(totalSamples, endSample);
        if (end <= start) return new short[0];
        int count = Math.toIntExact(end - start);
        short[] out = new short[count];
        for (int i = 0; i < count; i++) {
            out[i] = data[(int) ((start + i) % data.length)];
        }
        return out;
    }
}
