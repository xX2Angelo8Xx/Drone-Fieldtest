package com.chatgpt.speechnotes;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class BenchmarkActivity extends Activity {
    private static final int BG = Color.rgb(11, 15, 20);
    private static final int CARD = Color.rgb(22, 28, 36);
    private static final int CARD_2 = Color.rgb(30, 38, 48);
    private static final int TEXT = Color.rgb(242, 246, 250);
    private static final int MUTED = Color.rgb(151, 163, 178);
    private static final int ACCENT = Color.rgb(100, 220, 190);
    private static final int DANGER = Color.rgb(255, 100, 105);

    private final android.os.Handler ui = new android.os.Handler();
    private final int[] threadSuite = {2, 4, 6, 8};

    private Spinner wavSpinner;
    private Spinner modelSpinner;
    private Button startButton;
    private ProgressBar progress;
    private TextView progressLabel;
    private LinearLayout stepList;
    private LinearLayout results;
    private final ArrayList<File> wavFiles = new ArrayList<>();
    private boolean running = false;
    private PowerManager.WakeLock wakeLock;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setStatusBarColor(BG);
        getWindow().setNavigationBarColor(BG);
        buildUi();
    }

    @Override protected void onDestroy() {
        releaseWakeLock();
        super.onDestroy();
    }

    private void buildUi() {
        LinearLayout screen = column();
        screen.setBackgroundColor(BG);

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setClipToPadding(false);
        LinearLayout content = column();
        content.setPadding(dp(20), dp(22), dp(20), dp(22));
        scroll.addView(content, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        screen.addView(scroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        LinearLayout bottom = bottomTabs();
        LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        bp.setMargins(dp(16), dp(6), dp(16), dp(10));
        screen.addView(bottom, bp);
        setContentView(screen);

        screen.setOnApplyWindowInsetsListener((v, insets) -> {
            int top; int bottomInset;
            if (Build.VERSION.SDK_INT >= 30) {
                Insets bars = insets.getInsets(WindowInsets.Type.systemBars());
                top = bars.top; bottomInset = bars.bottom;
            } else {
                top = insets.getSystemWindowInsetTop();
                bottomInset = insets.getSystemWindowInsetBottom();
            }
            screen.setPadding(0, top, 0, 0);
            LinearLayout.LayoutParams p = (LinearLayout.LayoutParams) bottom.getLayoutParams();
            p.setMargins(dp(16), dp(6), dp(16), dp(10) + bottomInset);
            bottom.setLayoutParams(p);
            return insets;
        });
        screen.requestApplyInsets();

        content.addView(text("Benchmark", 29, TEXT, true));
        content.addView(space(4));
        content.addView(text("Reproduzierbare Performance-Tests mit derselben gespeicherten WAV-Datei", 14, MUTED, false));
        content.addView(space(18));

        LinearLayout info = card();
        info.addView(text("Threading Suite", 17, TEXT, true));
        info.addView(space(5));
        info.addView(text("2 → 4 → 6 → 8 Threads · Sprache Deutsch · Aviation aus · no-context", 13, MUTED, false));
        info.addView(space(6));
        info.addView(text("Hinweis: spätere Runs können durch Erwärmung leicht benachteiligt sein. Der gleiche Audio-Input bleibt jedoch exakt identisch.", 12, MUTED, false));
        content.addView(info, matchWrap());
        content.addView(space(16));

        content.addView(sectionLabel("WAV-DATEI"));
        content.addView(space(8));
        wavSpinner = darkSpinner();
        content.addView(wavSpinner, matchWrap());
        content.addView(space(7));
        TextView wavHint = text("Gespeicherte WAVs stammen aus Diktaten mit aktiviertem WAV-Toggle.", 12, MUTED, false);
        content.addView(wavHint, matchWrap());
        loadWavs();

        content.addView(space(18));
        content.addView(sectionLabel("MODELL"));
        content.addView(space(8));
        modelSpinner = darkSpinner();
        modelSpinner.setAdapter(new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_dropdown_item,
                new String[]{"Whisper Large-v3-Turbo · Q5_0", "Whisper Base · Q5_1"}) {
            @Override public View getView(int position, View convertView, ViewGroup parent) {
                return styleSpinner(super.getView(position, convertView, parent), false);
            }
            @Override public View getDropDownView(int position, View convertView, ViewGroup parent) {
                return styleSpinner(super.getDropDownView(position, convertView, parent), true);
            }
        });
        content.addView(modelSpinner, matchWrap());

        content.addView(space(18));
        startButton = new Button(this);
        startButton.setAllCaps(false);
        startButton.setText("Thread-Benchmark starten");
        startButton.setTextSize(16);
        startButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        startButton.setTextColor(Color.rgb(8, 23, 20));
        startButton.setBackground(roundRect(ACCENT, 18));
        startButton.setOnClickListener(v -> startBenchmark());
        content.addView(startButton, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(56)));

        content.addView(space(18));
        LinearLayout progressCard = card();
        progressLabel = text("Bereit", 14, TEXT, true);
        progressCard.addView(progressLabel, matchWrap());
        progressCard.addView(space(10));
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(threadSuite.length);
        progress.setProgress(0);
        progressCard.addView(progress, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(8)));
        progressCard.addView(space(12));
        stepList = column();
        progressCard.addView(stepList, matchWrap());
        rebuildStepList(-1, -1);
        content.addView(progressCard, matchWrap());

        content.addView(space(18));
        content.addView(sectionLabel("ERGEBNISSE"));
        content.addView(space(8));
        results = column();
        content.addView(results, matchWrap());
        results.addView(text("Noch kein Benchmark durchgeführt.", 13, MUTED, false));
        content.addView(space(14));
    }

    private void loadWavs() {
        wavFiles.clear();
        File dir = new File(getFilesDir(), "recordings");
        File[] files = dir.listFiles((d, name) -> name.toLowerCase(Locale.ROOT).endsWith(".wav"));
        if (files != null) {
            Arrays.sort(files, Comparator.comparingLong(File::lastModified).reversed());
            wavFiles.addAll(Arrays.asList(files));
        }

        ArrayList<String> labels = new ArrayList<>();
        for (File f : wavFiles) {
            try {
                WavBenchmarkUtils.Info info = WavBenchmarkUtils.inspect(f);
                labels.add(DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT)
                        .format(new Date(f.lastModified())) + " · " + formatDuration(info.durationMs));
            } catch (Throwable t) {
                labels.add(f.getName());
            }
        }
        if (labels.isEmpty()) labels.add("Keine gespeicherten WAV-Dateien");

        wavSpinner.setAdapter(new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_dropdown_item, labels) {
            @Override public View getView(int position, View convertView, ViewGroup parent) {
                return styleSpinner(super.getView(position, convertView, parent), false);
            }
            @Override public View getDropDownView(int position, View convertView, ViewGroup parent) {
                return styleSpinner(super.getDropDownView(position, convertView, parent), true);
            }
        });
        startButtonState();
    }

    private void startButtonState() {
        if (startButton == null) return;
        startButton.setEnabled(!running && !wavFiles.isEmpty());
        startButton.setAlpha(startButton.isEnabled() ? 1f : 0.5f);
    }

    private void startBenchmark() {
        if (running) return;
        if (wavFiles.isEmpty()) {
            Toast.makeText(this, "Bitte zuerst ein Diktat mit WAV-Speicherung aufnehmen.", Toast.LENGTH_LONG).show();
            return;
        }
        int pos = wavSpinner.getSelectedItemPosition();
        if (pos < 0 || pos >= wavFiles.size()) return;
        File wav = wavFiles.get(pos);
        final String model = modelSpinner.getSelectedItemPosition() == 1
                ? "base-q5_1" : "large-v3-turbo-q5_0";

        running = true;
        startButtonState();
        wavSpinner.setEnabled(false);
        modelSpinner.setEnabled(false);
        progress.setProgress(0);
        progressLabel.setText("Benchmark wird vorbereitet …");
        results.removeAllViews();
        rebuildStepList(0, -1);
        acquireWakeLock();

        new Thread(() -> runSuite(wav, model), "speech-benchmark").start();
    }

    private void runSuite(File wav, String model) {
        File pcm = new File(getCacheDir(), "benchmark-input.pcm");
        try {
            WavBenchmarkUtils.Info info = WavBenchmarkUtils.wavToPcm16kMono(wav, pcm);
            File modelFile = ModelManager.ensureModel(this, model);
            long loadMs = WhisperBridge.loadModel(modelFile.getAbsolutePath());
            if (loadMs < 0) throw new IllegalStateException("Modell konnte nicht geladen werden");

            ArrayList<RunResult> runResults = new ArrayList<>();
            for (int i = 0; i < threadSuite.length; i++) {
                final int step = i;
                final int threads = threadSuite[i];
                ui.post(() -> {
                    progressLabel.setText("Schritt " + (step + 1) + "/" + threadSuite.length + " · " + threads + " Threads");
                    rebuildStepList(step, -1);
                });

                long wallStart = SystemClock.elapsedRealtime();
                WhisperBridge.Result r = WhisperBridge.transcribeLoadedBenchmark(
                        pcm.getAbsolutePath(), "de", "", threads);
                long wallMs = SystemClock.elapsedRealtime() - wallStart;
                RunResult rr = new RunResult(threads, wallMs, info.durationMs, r);
                runResults.add(rr);

                ui.post(() -> {
                    progress.setProgress(step + 1);
                    rebuildStepList(step + 1, step);
                    addResultCard(rr);
                });
            }

            RunResult best = runResults.stream()
                    .min(Comparator.comparingLong(a -> a.wallMs)).orElse(null);
            ui.post(() -> finishBenchmark(best));
        } catch (Throwable t) {
            ui.post(() -> failBenchmark(t));
        } finally {
            pcm.delete();
        }
    }

    private void finishBenchmark(RunResult best) {
        releaseWakeLock();
        running = false;
        wavSpinner.setEnabled(true);
        modelSpinner.setEnabled(true);
        startButtonState();
        progress.setProgress(threadSuite.length);
        rebuildStepList(threadSuite.length, threadSuite.length - 1);
        if (best != null) {
            progressLabel.setText("Fertig · schnellster Run: " + best.threads + " Threads · " + formatMs(best.wallMs));
            TextView winner = text("Schnellste Konfiguration: " + best.threads + " Threads", 14, ACCENT, true);
            LinearLayout.LayoutParams p = matchWrap(); p.topMargin = dp(8);
            results.addView(winner, p);
        } else {
            progressLabel.setText("Benchmark abgeschlossen");
        }
    }

    private void failBenchmark(Throwable t) {
        releaseWakeLock();
        running = false;
        wavSpinner.setEnabled(true);
        modelSpinner.setEnabled(true);
        startButtonState();
        progressLabel.setText("Benchmark fehlgeschlagen");
        progressLabel.setTextColor(DANGER);
        results.removeAllViews();
        results.addView(text(t.getMessage() == null ? t.toString() : t.getMessage(), 13, DANGER, false));
    }

    private void addResultCard(RunResult rr) {
        LinearLayout c = card();
        c.addView(text(rr.threads + " Threads", 17, TEXT, true));
        c.addView(space(5));
        c.addView(text("Wall " + formatMs(rr.wallMs) + " · RTF " +
                String.format(Locale.getDefault(), "%.2f", rr.rtf()), 13, ACCENT, true));
        c.addView(space(4));
        c.addView(text("Native " + formatMs(rr.profile.nativeTotalMs) +
                " · Encode Σ " + formatMs(rr.profile.encodeTotalMs) +
                " · " + rr.profile.encodeRuns + "×", 12, MUTED, false));
        c.addView(space(3));
        c.addView(text("Decode Σ " + formatMs(rr.profile.decodeTotalMs) +
                " · Mel " + formatMs(rr.profile.melMs) +
                " · PCM " + formatMs(rr.profile.pcmMs), 12, MUTED, false));
        LinearLayout.LayoutParams p = matchWrap(); p.bottomMargin = dp(9);
        results.addView(c, p);
    }

    private void rebuildStepList(int activeStep, int justFinished) {
        if (stepList == null) return;
        stepList.removeAllViews();
        for (int i = 0; i < threadSuite.length; i++) {
            String prefix;
            int color;
            if (i < activeStep || i == justFinished) { prefix = "✓"; color = ACCENT; }
            else if (i == activeStep && activeStep < threadSuite.length) { prefix = "●"; color = TEXT; }
            else { prefix = "○"; color = MUTED; }
            TextView line = text(prefix + "  " + threadSuite[i] + " Threads", 13, color, i == activeStep);
            LinearLayout.LayoutParams p = matchWrap(); p.bottomMargin = dp(4);
            stepList.addView(line, p);
        }
    }

    private void acquireWakeLock() {
        if (wakeLock != null && wakeLock.isHeld()) return;
        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SpeechNotes:Benchmark");
        wakeLock.setReferenceCounted(false);
        wakeLock.acquire(60L * 60L * 1000L);
    }

    private void releaseWakeLock() {
        if (wakeLock != null && wakeLock.isHeld()) {
            try { wakeLock.release(); } catch (Throwable ignored) { }
        }
        wakeLock = null;
    }

    private LinearLayout bottomTabs() {
        LinearLayout bar = row();
        bar.setPadding(dp(4), dp(4), dp(4), dp(4));
        bar.setBackground(roundRect(CARD, 18));
        Button record = tabButton("Diktat", false);
        Button history = tabButton("Verlauf", false);
        Button benchmark = tabButton("Benchmark", true);
        record.setOnClickListener(v -> navigateMain("record"));
        history.setOnClickListener(v -> navigateMain("history"));
        bar.addView(record, weightHeight(dp(48)));
        bar.addView(history, weightHeight(dp(48)));
        bar.addView(benchmark, weightHeight(dp(48)));
        return bar;
    }

    private void navigateMain(String tab) {
        if (running) {
            Toast.makeText(this, "Benchmark läuft noch.", Toast.LENGTH_SHORT).show();
            return;
        }
        Intent i = new Intent(this, MainActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP)
                .putExtra("open_tab", tab);
        startActivity(i);
        finish();
    }

    private Spinner darkSpinner() { return new Spinner(this); }

    private View styleSpinner(View view, boolean dropdown) {
        TextView v = (TextView) view;
        v.setTextColor(TEXT);
        v.setTextSize(14);
        v.setPadding(dp(14), dp(13), dp(14), dp(13));
        if (dropdown) v.setBackgroundColor(CARD_2);
        else v.setBackground(roundRect(CARD, 15));
        return v;
    }

    private LinearLayout card() {
        LinearLayout c = column();
        c.setPadding(dp(16), dp(15), dp(16), dp(15));
        c.setBackground(roundRect(CARD, 18));
        return c;
    }

    private Button tabButton(String label, boolean selected) {
        Button b = new Button(this);
        b.setAllCaps(false); b.setText(label); b.setTextSize(13);
        b.setTypeface(Typeface.DEFAULT, selected ? Typeface.BOLD : Typeface.NORMAL);
        b.setTextColor(selected ? TEXT : MUTED);
        b.setBackground(roundRect(selected ? CARD_2 : Color.TRANSPARENT, 14));
        return b;
    }

    private TextView sectionLabel(String value) {
        TextView v = text(value, 11, MUTED, true); v.setLetterSpacing(0.12f); return v;
    }
    private LinearLayout column() { LinearLayout l = new LinearLayout(this); l.setOrientation(LinearLayout.VERTICAL); return l; }
    private LinearLayout row() { LinearLayout l = new LinearLayout(this); l.setOrientation(LinearLayout.HORIZONTAL); return l; }
    private View space(int d) { View v = new View(this); v.setLayoutParams(new LinearLayout.LayoutParams(1, dp(d))); return v; }
    private TextView text(String value, int sp, int color, boolean bold) {
        TextView v = new TextView(this); v.setText(value); v.setTextSize(sp); v.setTextColor(color);
        v.setTypeface(Typeface.DEFAULT, bold ? Typeface.BOLD : Typeface.NORMAL); v.setLineSpacing(0, 1.12f); return v;
    }
    private GradientDrawable roundRect(int color, int radiusDp) {
        GradientDrawable g = new GradientDrawable(); g.setColor(color); g.setCornerRadius(dp(radiusDp)); return g;
    }
    private int dp(int v) { return Math.round(v * getResources().getDisplayMetrics().density); }
    private LinearLayout.LayoutParams matchWrap() { return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT); }
    private LinearLayout.LayoutParams weightHeight(int h) { return new LinearLayout.LayoutParams(0, h, 1f); }
    private static String formatMs(long ms) {
        if (ms < 1000) return ms + " ms";
        if (ms < 60_000) return String.format(Locale.getDefault(), "%.1f s", ms / 1000.0);
        return String.format(Locale.getDefault(), "%dm %.1fs", ms / 60_000, (ms % 60_000) / 1000.0);
    }
    private static String formatDuration(long ms) {
        long s = Math.max(0, ms / 1000);
        return String.format(Locale.getDefault(), "%02d:%02d", s / 60, s % 60);
    }

    private static final class RunResult {
        final int threads;
        final long wallMs;
        final long audioMs;
        final WhisperBridge.Result profile;
        RunResult(int threads, long wallMs, long audioMs, WhisperBridge.Result profile) {
            this.threads = threads; this.wallMs = wallMs; this.audioMs = audioMs; this.profile = profile;
        }
        double rtf() { return audioMs <= 0 ? 0.0 : (double) wallMs / (double) audioMs; }
    }
}
