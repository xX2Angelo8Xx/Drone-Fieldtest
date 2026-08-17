package com.chatgpt.speechnotes;

import android.Manifest;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.text.DateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class MainActivity extends Activity {
    private static final int REQ_AUDIO = 100;
    private static final int REQ_NOTIFY = 101;

    private static final int BG = Color.rgb(11, 15, 20);
    private static final int CARD = Color.rgb(22, 28, 36);
    private static final int CARD_2 = Color.rgb(30, 38, 48);
    private static final int TEXT = Color.rgb(242, 246, 250);
    private static final int MUTED = Color.rgb(151, 163, 178);
    private static final int ACCENT = Color.rgb(100, 220, 190);
    private static final int DANGER = Color.rgb(255, 100, 105);

    private LinearLayout root;
    private TextView timer;
    private TextView status;
    private Button recordButton;
    private Spinner modelSpinner;
    private Switch wavSwitch;
    private Switch aviationSwitch;
    private SharedPreferences prefs;
    private final android.os.Handler ui = new android.os.Handler();

    private final Runnable timerTick = new Runnable() {
        @Override public void run() {
            if (RecordingService.isRecording() && timer != null) {
                long elapsed = SystemClock.elapsedRealtime() - RecordingService.getStartedElapsed();
                timer.setText(formatDuration(elapsed));
                ui.postDelayed(this, 250);
            }
        }
    };

    private final BroadcastReceiver stateReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            applyState(intent.getStringExtra(RecordingService.EXTRA_STATE), intent.getStringExtra(RecordingService.EXTRA_TEXT));
        }
    };

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = getSharedPreferences("settings", MODE_PRIVATE);
        getWindow().setStatusBarColor(BG);
        getWindow().setNavigationBarColor(BG);
        showRecordTab();
        requestNotificationPermissionIfUseful();
    }

    @Override protected void onStart() {
        super.onStart();
        IntentFilter filter = new IntentFilter(RecordingService.ACTION_STATE);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(stateReceiver, filter, RECEIVER_NOT_EXPORTED);
        else registerReceiver(stateReceiver, filter);
        if (RecordingService.isRecording()) applyState("recording", null);
        else if (RecordingService.isTranscribing()) applyState("transcribing", null);
    }

    @Override protected void onStop() {
        try { unregisterReceiver(stateReceiver); } catch (Throwable ignored) { }
        ui.removeCallbacks(timerTick);
        super.onStop();
    }

    private void showRecordTab() {
        ui.removeCallbacks(timerTick);
        root = buildPage(true);
        root.addView(text("Speech Notes", 29, TEXT, true));
        root.addView(space(4));
        root.addView(text("Lokale Whisper-Diktate · vollständig offline", 14, MUTED, false));
        root.addView(space(22));

        LinearLayout recorderCard = column();
        recorderCard.setPadding(dp(22), dp(24), dp(22), dp(24));
        recorderCard.setBackground(roundRect(CARD, 24));
        root.addView(recorderCard, matchWrap());

        status = text("Bereit", 14, ACCENT, true);
        status.setGravity(Gravity.CENTER);
        recorderCard.addView(status, matchWrap());
        recorderCard.addView(space(18));

        timer = text("00:00", 48, TEXT, true);
        timer.setTypeface(Typeface.create("sans-serif-light", Typeface.NORMAL));
        timer.setGravity(Gravity.CENTER);
        recorderCard.addView(timer, matchWrap());
        recorderCard.addView(space(22));

        recordButton = new Button(this);
        recordButton.setAllCaps(false);
        recordButton.setText("Aufnahme starten");
        recordButton.setTextSize(17);
        recordButton.setTextColor(Color.rgb(8, 23, 20));
        recordButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        recordButton.setBackground(roundRect(ACCENT, 18));
        recordButton.setPadding(dp(18), dp(14), dp(18), dp(14));
        recordButton.setOnClickListener(v -> toggleRecording());
        recorderCard.addView(recordButton, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(58)));

        root.addView(space(18));
        root.addView(sectionLabel("MODELL"));
        root.addView(space(8));
        modelSpinner = new Spinner(this);
        String[] labels = {
                "Whisper Base · Q5_1 · ~57 MiB",
                "Whisper Large-v3-Turbo · Q5_0 · ~547 MiB"
        };
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_dropdown_item, labels) {
            @Override public View getView(int position, View convertView, ViewGroup parent) {
                TextView v = (TextView) super.getView(position, convertView, parent);
                v.setTextColor(TEXT); v.setTextSize(15); v.setPadding(dp(16), dp(14), dp(16), dp(14));
                v.setBackground(roundRect(CARD, 16)); return v;
            }
            @Override public View getDropDownView(int position, View convertView, ViewGroup parent) {
                TextView v = (TextView) super.getDropDownView(position, convertView, parent);
                v.setTextColor(TEXT); v.setBackgroundColor(CARD_2); v.setPadding(dp(16), dp(14), dp(16), dp(14)); return v;
            }
        };
        modelSpinner.setAdapter(adapter);
        int savedModel = prefs.getInt("model_v2", 0);
        modelSpinner.setSelection(Math.max(0, Math.min(1, savedModel)));
        root.addView(modelSpinner, matchWrap());

        root.addView(space(14));
        LinearLayout aviationCard = toggleCard(
                "Aviation Vocabulary",
                "Optionaler Fachwort-Kontext für beide Modelle");
        aviationSwitch = new Switch(this);
        aviationSwitch.setChecked(prefs.getBoolean("aviation_prompt", false));
        aviationCard.addView(aviationSwitch);
        root.addView(aviationCard, matchWrap());

        root.addView(space(10));
        LinearLayout wavCard = toggleCard(
                "WAV zusätzlich speichern",
                "16 kHz · Mono · PCM16");
        wavSwitch = new Switch(this);
        wavSwitch.setChecked(prefs.getBoolean("wav", false));
        wavCard.addView(wavSwitch);
        root.addView(wavCard, matchWrap());
        root.addView(space(22));

        if (RecordingService.isRecording()) applyState("recording", null);
        else if (RecordingService.isTranscribing()) applyState("transcribing", null);
    }

    private LinearLayout toggleCard(String title, String subtitle) {
        LinearLayout card = row();
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(dp(16), dp(10), dp(12), dp(10));
        card.setBackground(roundRect(CARD, 16));
        TextView label = text(title + "\n" + subtitle, 14, TEXT, false);
        card.addView(label, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        return card;
    }

    private void showHistoryTab() {
        ui.removeCallbacks(timerTick);
        root = buildPage(false);
        root.addView(text("Verlauf", 29, TEXT, true));
        root.addView(space(4));
        root.addView(text("Text antippen = kopieren · Pfeil = vollständig anzeigen", 14, MUTED, false));
        root.addView(space(18));

        TranscriptDb db = new TranscriptDb(this);
        TranscriptDb.Stats s = db.stats();
        LinearLayout statCard = row();
        statCard.setPadding(dp(16), dp(16), dp(16), dp(16));
        statCard.setBackground(roundRect(CARD, 18));
        statCard.addView(statCell(String.valueOf(s.count), "Diktate"), weight());
        statCard.addView(statCell(String.valueOf(s.words), "Wörter"), weight());
        statCard.addView(statCell(formatDurationCompact(s.durationMs), "Audio"), weight());
        root.addView(statCard, matchWrap());
        root.addView(space(16));

        List<TranscriptDb.Entry> entries = db.list();
        if (entries.isEmpty()) {
            TextView empty = text("Noch keine Transkriptionen.", 16, MUTED, false);
            empty.setGravity(Gravity.CENTER); empty.setPadding(0, dp(50), 0, dp(50));
            root.addView(empty, matchWrap());
            return;
        }

        for (TranscriptDb.Entry e : entries) {
            LinearLayout card = column();
            card.setPadding(dp(16), dp(15), dp(12), dp(15));
            card.setBackground(roundRect(CARD, 18));
            String date = DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT).format(new Date(e.createdAt));
            card.addView(text(date + "  ·  " + modelShort(e.model) + "  ·  " + formatDuration(e.durationMs), 12, MUTED, false));
            card.addView(space(8));

            LinearLayout textRow = row();
            textRow.setGravity(Gravity.TOP);
            TextView body = text(e.text.isEmpty() ? "(Kein Text erkannt)" : e.text, 15, TEXT, false);
            body.setMaxLines(5);
            body.setPadding(0, 0, dp(8), 0);
            body.setOnClickListener(v -> copyText(e.text));
            textRow.addView(body, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

            Button expand = new Button(this);
            expand.setAllCaps(false); expand.setText("⌄"); expand.setTextSize(22); expand.setTextColor(MUTED);
            expand.setGravity(Gravity.CENTER); expand.setPadding(0, 0, 0, 0);
            expand.setMinWidth(dp(42)); expand.setMinimumWidth(dp(42)); expand.setMinHeight(dp(42)); expand.setMinimumHeight(dp(42));
            expand.setBackground(roundRect(CARD_2, 12));
            final boolean[] open = {false};
            expand.setOnClickListener(v -> {
                open[0] = !open[0];
                body.setMaxLines(open[0] ? Integer.MAX_VALUE : 5);
                expand.setText(open[0] ? "⌃" : "⌄");
            });
            textRow.addView(expand, new LinearLayout.LayoutParams(dp(42), dp(42)));
            card.addView(textRow, matchWrap());
            card.addView(space(9));

            String perf = e.wordCount + " Wörter  ·  Inferenz " + formatDurationCompact(e.inferenceMs);
            if (e.wavPath != null) perf += "  ·  WAV gespeichert";
            card.addView(text(perf, 12, MUTED, false));
            LinearLayout.LayoutParams cp = matchWrap(); cp.bottomMargin = dp(10);
            root.addView(card, cp);
        }
        root.addView(space(10));
    }

    private LinearLayout buildPage(boolean recordSelected) {
        LinearLayout screen = column();
        screen.setBackgroundColor(BG);

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true); scroll.setBackgroundColor(BG); scroll.setClipToPadding(false);
        LinearLayout content = column();
        content.setPadding(dp(20), dp(22), dp(20), dp(22));
        scroll.addView(content, new ScrollView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        screen.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        LinearLayout bottom = bottomTabs(recordSelected);
        LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        bp.setMargins(dp(16), dp(6), dp(16), dp(10));
        screen.addView(bottom, bp);
        setContentView(screen);

        screen.setOnApplyWindowInsetsListener((v, insets) -> {
            int top; int bottomInset;
            if (Build.VERSION.SDK_INT >= 30) {
                Insets bars = insets.getInsets(WindowInsets.Type.systemBars());
                top = bars.top; bottomInset = bars.bottom;
            } else {
                top = insets.getSystemWindowInsetTop(); bottomInset = insets.getSystemWindowInsetBottom();
            }
            screen.setPadding(0, top, 0, 0);
            LinearLayout.LayoutParams p = (LinearLayout.LayoutParams) bottom.getLayoutParams();
            p.setMargins(dp(16), dp(6), dp(16), dp(10) + bottomInset);
            bottom.setLayoutParams(p);
            return insets;
        });
        screen.requestApplyInsets();
        return content;
    }

    private LinearLayout bottomTabs(boolean recordSelected) {
        LinearLayout bar = row();
        bar.setPadding(dp(4), dp(4), dp(4), dp(4));
        bar.setBackground(roundRect(CARD, 18));
        Button record = tabButton("Diktat", recordSelected);
        Button history = tabButton("Verlauf", !recordSelected);
        record.setOnClickListener(v -> showRecordTab());
        history.setOnClickListener(v -> showHistoryTab());
        bar.addView(record, weightHeight(dp(48)));
        bar.addView(history, weightHeight(dp(48)));
        return bar;
    }

    private void copyText(String value) {
        ClipboardManager cm = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        cm.setPrimaryClip(ClipData.newPlainText("Speech Notes", value));
        Toast.makeText(this, "Text kopiert", Toast.LENGTH_SHORT).show();
    }

    private void toggleRecording() {
        if (RecordingService.isRecording()) {
            startService(new Intent(this, RecordingService.class).setAction(RecordingService.ACTION_STOP)); return;
        }
        if (RecordingService.isTranscribing()) return;
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, REQ_AUDIO); return;
        }
        launchRecording();
    }

    private void launchRecording() {
        int idx = modelSpinner.getSelectedItemPosition();
        String model = idx == 1 ? "large-v3-turbo-q5_0" : "base-q5_1";
        boolean useAviation = aviationSwitch.isChecked();
        boolean save = wavSwitch.isChecked();
        prefs.edit().putInt("model_v2", idx)
                .putBoolean("wav", save)
                .putBoolean("aviation_prompt", useAviation)
                .apply();
        Intent i = new Intent(this, RecordingService.class).setAction(RecordingService.ACTION_START)
                .putExtra(RecordingService.EXTRA_MODEL, model)
                .putExtra(RecordingService.EXTRA_SAVE_WAV, save)
                .putExtra(RecordingService.EXTRA_AVIATION_PROMPT, useAviation);
        startForegroundService(i);
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_AUDIO && grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) launchRecording();
        else if (requestCode == REQ_AUDIO) Toast.makeText(this, "Mikrofonberechtigung wird für Diktate benötigt.", Toast.LENGTH_LONG).show();
    }

    private void requestNotificationPermissionIfUseful() {
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED)
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_NOTIFY);
    }

    private void applyState(String state, String text) {
        if (status == null || recordButton == null || timer == null || modelSpinner == null || wavSwitch == null || aviationSwitch == null) return;
        if ("recording".equals(state)) {
            status.setText("● AUFNAHME AKTIV"); status.setTextColor(DANGER);
            recordButton.setText("Aufnahme beenden"); recordButton.setTextColor(Color.WHITE); recordButton.setBackground(roundRect(DANGER, 18));
            modelSpinner.setEnabled(false); wavSwitch.setEnabled(false); aviationSwitch.setEnabled(false); ui.removeCallbacks(timerTick); ui.post(timerTick);
        } else if ("stopping".equals(state) || "transcribing".equals(state)) {
            ui.removeCallbacks(timerTick); status.setText("TRANSKRIBIERE…"); status.setTextColor(ACCENT);
            recordButton.setText("Bitte warten…"); recordButton.setEnabled(false);
            modelSpinner.setEnabled(false); wavSwitch.setEnabled(false); aviationSwitch.setEnabled(false);
        } else if ("done".equals(state)) {
            ui.removeCallbacks(timerTick); timer.setText("00:00"); status.setText("Fertig · im Verlauf gespeichert"); status.setTextColor(ACCENT);
            recordButton.setText("Aufnahme starten"); recordButton.setTextColor(Color.rgb(8, 23, 20)); recordButton.setBackground(roundRect(ACCENT, 18));
            recordButton.setEnabled(true); modelSpinner.setEnabled(true); wavSwitch.setEnabled(true); aviationSwitch.setEnabled(true);
            if (text != null && !text.isEmpty()) Toast.makeText(this, "Transkription abgeschlossen", Toast.LENGTH_SHORT).show();
        } else if ("error".equals(state)) {
            ui.removeCallbacks(timerTick); status.setText("Fehler"); status.setTextColor(DANGER); recordButton.setEnabled(true); recordButton.setText("Erneut versuchen");
            modelSpinner.setEnabled(true); wavSwitch.setEnabled(true); aviationSwitch.setEnabled(true);
            if (text != null) Toast.makeText(this, text, Toast.LENGTH_LONG).show();
        }
    }

    private Button tabButton(String label, boolean selected) {
        Button b = new Button(this); b.setAllCaps(false); b.setText(label); b.setTextSize(14);
        b.setTypeface(Typeface.DEFAULT, selected ? Typeface.BOLD : Typeface.NORMAL); b.setTextColor(selected ? TEXT : MUTED);
        b.setBackground(roundRect(selected ? CARD_2 : Color.TRANSPARENT, 14)); return b;
    }

    private LinearLayout statCell(String value, String label) {
        LinearLayout box = column(); box.setGravity(Gravity.CENTER); box.addView(textCentered(value, 21, TEXT, true)); box.addView(textCentered(label, 11, MUTED, false)); return box;
    }
    private TextView sectionLabel(String value) { TextView v = text(value, 11, MUTED, true); v.setLetterSpacing(0.12f); return v; }
    private LinearLayout column() { LinearLayout l = new LinearLayout(this); l.setOrientation(LinearLayout.VERTICAL); return l; }
    private LinearLayout row() { LinearLayout l = new LinearLayout(this); l.setOrientation(LinearLayout.HORIZONTAL); return l; }
    private View space(int d) { View v = new View(this); v.setLayoutParams(new LinearLayout.LayoutParams(1, dp(d))); return v; }
    private TextView text(String value, int sp, int color, boolean bold) { TextView v = new TextView(this); v.setText(value); v.setTextSize(sp); v.setTextColor(color); v.setTypeface(Typeface.DEFAULT, bold ? Typeface.BOLD : Typeface.NORMAL); v.setLineSpacing(0, 1.12f); return v; }
    private TextView textCentered(String value, int sp, int color, boolean bold) { TextView v = text(value, sp, color, bold); v.setGravity(Gravity.CENTER); return v; }
    private GradientDrawable roundRect(int color, int radiusDp) { GradientDrawable g = new GradientDrawable(); g.setColor(color); g.setCornerRadius(dp(radiusDp)); return g; }
    private int dp(int v) { return Math.round(v * getResources().getDisplayMetrics().density); }
    private LinearLayout.LayoutParams matchWrap() { return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT); }
    private LinearLayout.LayoutParams weight() { return new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f); }
    private LinearLayout.LayoutParams weightHeight(int h) { return new LinearLayout.LayoutParams(0, h, 1f); }
    private static String formatDuration(long ms) { long sec = Math.max(0, ms / 1000); return String.format(Locale.getDefault(), "%02d:%02d", sec / 60, sec % 60); }
    private static String formatDurationCompact(long ms) { long sec = Math.max(0, ms / 1000); if (sec < 60) return sec + " s"; return (sec / 60) + "m " + (sec % 60) + "s"; }
    private static String modelShort(String m) {
        if (m.startsWith("large-v3-turbo")) return "Large-v3-Turbo Q5_0";
        if (m.startsWith("tiny")) return "Tiny Q5_1";
        if (m.startsWith("small")) return "Small Q5_1";
        return "Base Q5_1";
    }
}
