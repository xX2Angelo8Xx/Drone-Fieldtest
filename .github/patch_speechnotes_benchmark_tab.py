from pathlib import Path

p = Path('SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/MainActivity.java')
s = p.read_text()

old = '''        showRecordTab();
        requestNotificationPermissionIfUseful();'''
new = '''        String openTab = getIntent().getStringExtra("open_tab");
        if ("history".equals(openTab)) showHistoryTab();
        else showRecordTab();
        requestNotificationPermissionIfUseful();'''
if old not in s:
    raise SystemExit('onCreate tab hook not found')
s = s.replace(old, new, 1)

anchor = '''    @Override protected void onStart() {'''
insert = '''    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        String openTab = intent.getStringExtra("open_tab");
        if ("history".equals(openTab)) showHistoryTab();
        else showRecordTab();
    }

'''
if anchor not in s:
    raise SystemExit('onStart anchor not found')
s = s.replace(anchor, insert + anchor, 1)

old = '''        Button record = tabButton("Diktat", recordSelected);
        Button history = tabButton("Verlauf", !recordSelected);
        record.setOnClickListener(v -> showRecordTab());
        history.setOnClickListener(v -> showHistoryTab());
        bar.addView(record, weightHeight(dp(48)));
        bar.addView(history, weightHeight(dp(48)));
        return bar;'''
new = '''        Button record = tabButton("Diktat", recordSelected);
        Button history = tabButton("Verlauf", !recordSelected);
        Button benchmark = tabButton("Benchmark", false);
        record.setOnClickListener(v -> showRecordTab());
        history.setOnClickListener(v -> showHistoryTab());
        benchmark.setOnClickListener(v -> startActivity(new Intent(this, BenchmarkActivity.class)));
        bar.addView(record, weightHeight(dp(48)));
        bar.addView(history, weightHeight(dp(48)));
        bar.addView(benchmark, weightHeight(dp(48)));
        return bar;'''
if old not in s:
    raise SystemExit('bottom tab block not found')
s = s.replace(old, new, 1)

p.write_text(s)
print('Benchmark navigation patched into MainActivity.java')
