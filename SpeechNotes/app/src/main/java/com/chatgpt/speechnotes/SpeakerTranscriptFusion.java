package com.chatgpt.speechnotes;

import java.util.ArrayList;
import java.util.List;

/** Fuses Whisper segment timestamps with the diarization timeline and de-duplicates overlap windows. */
public final class SpeakerTranscriptFusion {
    private static final long SAMPLES_PER_WHISPER_TICK = 160L; // whisper.cpp t0/t1 are 10 ms units at 16 kHz

    private static final class RawSegment {
        final long startSample;
        final long endSample;
        final String text;
        RawSegment(long startSample, long endSample, String text) {
            this.startSample = startSample;
            this.endSample = endSample;
            this.text = text;
        }
    }

    private static final class Block {
        final int speakerId;
        final StringBuilder text = new StringBuilder();
        Block(int speakerId) { this.speakerId = speakerId; }
    }

    private final List<RawSegment> raw = new ArrayList<>();
    private long lastAcceptedMidSample = -1L;

    public synchronized void acceptWindow(WhisperBridge.Result result, long windowStartSample,
                                          SpeakerDiarizationEngine diarizer) {
        if (result == null || result.segments == null) return;
        for (WhisperBridge.Segment s : result.segments) {
            String text = s.text == null ? "" : s.text.trim();
            if (text.isEmpty()) continue;
            long start = windowStartSample + Math.max(0L, s.t0) * SAMPLES_PER_WHISPER_TICK;
            long end = windowStartSample + Math.max(s.t0, s.t1) * SAMPLES_PER_WHISPER_TICK;
            if (end <= start) end = start + 1;
            long mid = start + (end - start) / 2L;
            // Windows overlap by three seconds. Midpoint monotonicity is deliberately used as
            // the stable commit rule so the overlap can improve decoding without duplicating text.
            if (mid <= lastAcceptedMidSample) continue;
            raw.add(new RawSegment(start, end, text));
            lastAcceptedMidSample = mid;
        }
    }

    public synchronized String text(SpeakerDiarizationEngine diarizer) {
        List<Block> blocks = buildBlocks(diarizer);
        StringBuilder out = new StringBuilder();
        for (Block b : blocks) {
            if (out.length() > 0) out.append('\n').append('\n');
            out.append(b.speakerId > 0 ? "Sprecher " + b.speakerId : "Sprecher ?").append(": ");
            out.append(b.text.toString().trim());
        }
        return out.toString().trim();
    }

    public synchronized String plainText() {
        StringBuilder out = new StringBuilder();
        for (RawSegment s : raw) {
            if (out.length() > 0) out.append(' ');
            out.append(s.text);
        }
        return out.toString().replaceAll("\\s+", " ").trim();
    }

    private List<Block> buildBlocks(SpeakerDiarizationEngine diarizer) {
        ArrayList<Block> blocks = new ArrayList<>();
        for (RawSegment s : raw) {
            int speaker = diarizer == null ? 0 : diarizer.speakerForInterval(s.startSample, s.endSample);
            Block block = blocks.isEmpty() ? null : blocks.get(blocks.size() - 1);
            if (block == null || block.speakerId != speaker) {
                block = new Block(speaker);
                blocks.add(block);
            }
            if (block.text.length() > 0) block.text.append(' ');
            block.text.append(s.text);
        }
        return blocks;
    }
}
