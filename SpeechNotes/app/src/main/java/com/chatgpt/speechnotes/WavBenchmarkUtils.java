package com.chatgpt.speechnotes;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public final class WavBenchmarkUtils {
    private WavBenchmarkUtils() {}

    // Keep benchmark audio safely below Whisper's 30 s chunk boundary so every
    // configuration measures exactly one encoder pass.
    public static final long BENCHMARK_MAX_MS = 25_000L;

    public static final class Info {
        public int channels;
        public int sampleRate;
        public int bitsPerSample;
        public long dataOffset;
        public long dataSize;
        public long durationMs;
    }

    public static Info inspect(File wav) throws IOException {
        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(wav), 128 * 1024)) {
            byte[] riff = readExact(in, 12);
            if (!"RIFF".equals(ascii(riff, 0, 4)) || !"WAVE".equals(ascii(riff, 8, 4))) {
                throw new IOException("Keine gültige RIFF/WAVE-Datei");
            }
            long offset = 12;
            Info info = new Info();
            boolean haveFmt = false;
            while (true) {
                byte[] header = readExact(in, 8);
                offset += 8;
                String id = ascii(header, 0, 4);
                long size = u32le(header, 4);
                if (size > Integer.MAX_VALUE) throw new IOException("WAV-Chunk zu groß");

                if ("fmt ".equals(id)) {
                    byte[] fmt = readExact(in, (int) size);
                    offset += size;
                    if (size < 16) throw new IOException("Ungültiger fmt-Chunk");
                    int audioFormat = u16le(fmt, 0);
                    info.channels = u16le(fmt, 2);
                    info.sampleRate = (int) u32le(fmt, 4);
                    info.bitsPerSample = u16le(fmt, 14);
                    if (audioFormat != 1) throw new IOException("Nur PCM-WAV wird unterstützt");
                    haveFmt = true;
                } else if ("data".equals(id)) {
                    if (!haveFmt) throw new IOException("WAV enthält data vor fmt");
                    info.dataOffset = offset;
                    info.dataSize = size;
                    long bytesPerSecond = (long) info.sampleRate * info.channels * (info.bitsPerSample / 8L);
                    info.durationMs = bytesPerSecond > 0 ? (size * 1000L) / bytesPerSecond : 0;
                    return info;
                } else {
                    skipExact(in, size);
                    offset += size;
                }
                if ((size & 1L) != 0) {
                    skipExact(in, 1);
                    offset += 1;
                }
            }
        }
    }

    public static Info wavToPcm16kMono(File wav, File pcmOut) throws IOException {
        Info info = inspect(wav);
        if (info.channels != 1 || info.sampleRate != 16000 || info.bitsPerSample != 16) {
            throw new IOException("Benchmark erwartet 16 kHz · Mono · PCM16");
        }

        final long bytesPerSecond = 16000L * 2L;
        final long benchmarkBytes = (BENCHMARK_MAX_MS * bytesPerSecond) / 1000L;
        final long bytesToCopy = Math.min(info.dataSize, benchmarkBytes);

        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(wav), 256 * 1024);
             BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(pcmOut), 256 * 1024)) {
            skipExact(in, info.dataOffset);
            byte[] buffer = new byte[256 * 1024];
            long remaining = bytesToCopy;
            while (remaining > 0) {
                int want = (int) Math.min(buffer.length, remaining);
                int n = in.read(buffer, 0, want);
                if (n < 0) throw new EOFException("WAV data unerwartet beendet");
                out.write(buffer, 0, n);
                remaining -= n;
            }
        }

        // Return the effective benchmark duration, not the full source WAV duration.
        info.dataSize = bytesToCopy;
        info.durationMs = (bytesToCopy * 1000L) / bytesPerSecond;
        return info;
    }

    private static byte[] readExact(BufferedInputStream in, int n) throws IOException {
        byte[] b = new byte[n];
        int off = 0;
        while (off < n) {
            int r = in.read(b, off, n - off);
            if (r < 0) throw new EOFException("WAV unerwartet beendet");
            off += r;
        }
        return b;
    }

    private static void skipExact(BufferedInputStream in, long n) throws IOException {
        long left = n;
        while (left > 0) {
            long s = in.skip(left);
            if (s > 0) { left -= s; continue; }
            if (in.read() < 0) throw new EOFException("WAV unerwartet beendet");
            left--;
        }
    }

    private static String ascii(byte[] b, int off, int len) {
        return new String(b, off, len, StandardCharsets.US_ASCII);
    }

    private static int u16le(byte[] b, int off) {
        return (b[off] & 0xff) | ((b[off + 1] & 0xff) << 8);
    }

    private static long u32le(byte[] b, int off) {
        return ((long) b[off] & 0xff) |
                (((long) b[off + 1] & 0xff) << 8) |
                (((long) b[off + 2] & 0xff) << 16) |
                (((long) b[off + 3] & 0xff) << 24);
    }
}
