package com.chatgpt.speechnotes;

import java.text.Normalizer;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Deterministic overlap de-duplication for consecutive Whisper windows. */
public final class StreamingTranscriptAssembler {
    private final List<String> words = new ArrayList<>();

    public synchronized String merge(String text) {
        List<String> incoming = splitWords(text);
        if (incoming.isEmpty()) return text();
        if (words.isEmpty()) {
            words.addAll(incoming);
            return text();
        }

        int max = Math.min(28, Math.min(words.size(), incoming.size()));
        int overlap = 0;
        for (int n = max; n >= 2; n--) {
            boolean match = true;
            int base = words.size() - n;
            for (int i = 0; i < n; i++) {
                if (!norm(words.get(base + i)).equals(norm(incoming.get(i)))) {
                    match = false;
                    break;
                }
            }
            if (match) { overlap = n; break; }
        }
        for (int i = overlap; i < incoming.size(); i++) words.add(incoming.get(i));
        return text();
    }

    public synchronized String text() {
        StringBuilder b = new StringBuilder();
        for (String word : words) {
            if (b.length() > 0) b.append(' ');
            b.append(word);
        }
        return b.toString().trim();
    }

    public synchronized String committedText() {
        int keepLive = Math.min(10, words.size());
        int end = words.size() - keepLive;
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < end; i++) {
            if (b.length() > 0) b.append(' ');
            b.append(words.get(i));
        }
        return b.toString().trim();
    }

    public synchronized String liveTail() {
        int start = Math.max(0, words.size() - 10);
        StringBuilder b = new StringBuilder();
        for (int i = start; i < words.size(); i++) {
            if (b.length() > 0) b.append(' ');
            b.append(words.get(i));
        }
        return b.toString().trim();
    }

    private static List<String> splitWords(String text) {
        ArrayList<String> out = new ArrayList<>();
        if (text == null) return out;
        for (String p : text.trim().split("\\s+")) if (!p.isEmpty()) out.add(p);
        return out;
    }

    private static String norm(String s) {
        String n = Normalizer.normalize(s == null ? "" : s, Normalizer.Form.NFKD)
                .toLowerCase(Locale.ROOT);
        return n.replaceAll("[^\\p{L}\\p{N}]+", "");
    }
}
