package com.chatgpt.speechnotes;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import java.util.ArrayList;
import java.util.List;

public class TranscriptDb extends SQLiteOpenHelper {
    private static final String DB_NAME = "speech_notes.db";
    private static final int DB_VERSION = 3;

    public static final class Entry {
        public long id;
        public long createdAt;
        public long durationMs;
        public long inferenceMs;
        public long modelLoadMs;
        public long pcmMs;
        public long nativeTotalMs;
        public long melMs;
        public long encodeMs, encodeTotalMs;
        public int encodeRuns;
        public long decodeMs, decodeTotalMs;
        public int decodeRuns;
        public long sampleMs, sampleTotalMs;
        public int sampleRuns;
        public long batchMs, batchTotalMs;
        public int batchRuns;
        public long promptMs, promptTotalMs;
        public int promptRuns;
        public String model;
        public String text;
        public int wordCount;
        public String wavPath;
    }

    public static final class Stats {
        public int count;
        public long words;
        public long durationMs;
    }

    public TranscriptDb(Context context) { super(context, DB_NAME, null, DB_VERSION); }

    @Override public void onCreate(SQLiteDatabase db) {
        db.execSQL("CREATE TABLE transcripts (" +
                "id INTEGER PRIMARY KEY AUTOINCREMENT," +
                "created_at INTEGER NOT NULL," +
                "duration_ms INTEGER NOT NULL," +
                "inference_ms INTEGER NOT NULL," +
                "model_load_ms INTEGER NOT NULL DEFAULT 0," +
                "pcm_ms INTEGER NOT NULL DEFAULT 0," +
                "native_total_ms INTEGER NOT NULL DEFAULT 0," +
                "mel_ms INTEGER NOT NULL DEFAULT 0," +
                "encode_ms INTEGER NOT NULL DEFAULT 0," +
                "encode_total_ms INTEGER NOT NULL DEFAULT 0," +
                "encode_runs INTEGER NOT NULL DEFAULT 0," +
                "decode_ms INTEGER NOT NULL DEFAULT 0," +
                "decode_total_ms INTEGER NOT NULL DEFAULT 0," +
                "decode_runs INTEGER NOT NULL DEFAULT 0," +
                "sample_ms INTEGER NOT NULL DEFAULT 0," +
                "sample_total_ms INTEGER NOT NULL DEFAULT 0," +
                "sample_runs INTEGER NOT NULL DEFAULT 0," +
                "batch_ms INTEGER NOT NULL DEFAULT 0," +
                "batch_total_ms INTEGER NOT NULL DEFAULT 0," +
                "batch_runs INTEGER NOT NULL DEFAULT 0," +
                "prompt_ms INTEGER NOT NULL DEFAULT 0," +
                "prompt_total_ms INTEGER NOT NULL DEFAULT 0," +
                "prompt_runs INTEGER NOT NULL DEFAULT 0," +
                "model TEXT NOT NULL," +
                "text TEXT NOT NULL," +
                "word_count INTEGER NOT NULL," +
                "wav_path TEXT)");
    }

    @Override public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        if (oldVersion < 2) {
            db.execSQL("ALTER TABLE transcripts ADD COLUMN model_load_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN pcm_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN encode_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN decode_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN sample_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN batch_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN prompt_ms INTEGER NOT NULL DEFAULT 0");
        }
        if (oldVersion < 3) {
            db.execSQL("ALTER TABLE transcripts ADD COLUMN native_total_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN mel_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN encode_total_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN encode_runs INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN decode_total_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN decode_runs INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN sample_total_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN sample_runs INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN batch_total_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN batch_runs INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN prompt_total_ms INTEGER NOT NULL DEFAULT 0");
            db.execSQL("ALTER TABLE transcripts ADD COLUMN prompt_runs INTEGER NOT NULL DEFAULT 0");
        }
    }

    public long insert(long createdAt, long durationMs, WhisperBridge.Result p, long modelLoadMs,
                       String model, String text, int wordCount, String wavPath) {
        ContentValues v = new ContentValues();
        v.put("created_at", createdAt);
        v.put("duration_ms", durationMs);
        v.put("inference_ms", p.whisperMs);
        v.put("model_load_ms", modelLoadMs);
        v.put("pcm_ms", p.pcmMs);
        v.put("native_total_ms", p.nativeTotalMs);
        v.put("mel_ms", p.melMs);
        v.put("encode_ms", p.encodeMs); v.put("encode_total_ms", p.encodeTotalMs); v.put("encode_runs", p.encodeRuns);
        v.put("decode_ms", p.decodeMs); v.put("decode_total_ms", p.decodeTotalMs); v.put("decode_runs", p.decodeRuns);
        v.put("sample_ms", p.sampleMs); v.put("sample_total_ms", p.sampleTotalMs); v.put("sample_runs", p.sampleRuns);
        v.put("batch_ms", p.batchMs); v.put("batch_total_ms", p.batchTotalMs); v.put("batch_runs", p.batchRuns);
        v.put("prompt_ms", p.promptMs); v.put("prompt_total_ms", p.promptTotalMs); v.put("prompt_runs", p.promptRuns);
        v.put("model", model);
        v.put("text", text);
        v.put("word_count", wordCount);
        if (wavPath == null) v.putNull("wav_path"); else v.put("wav_path", wavPath);
        return getWritableDatabase().insert("transcripts", null, v);
    }

    public List<Entry> list() {
        ArrayList<Entry> out = new ArrayList<>();
        try (Cursor c = getReadableDatabase().rawQuery(
                "SELECT id,created_at,duration_ms,inference_ms,model_load_ms,pcm_ms,native_total_ms,mel_ms," +
                        "encode_ms,encode_total_ms,encode_runs,decode_ms,decode_total_ms,decode_runs," +
                        "sample_ms,sample_total_ms,sample_runs,batch_ms,batch_total_ms,batch_runs," +
                        "prompt_ms,prompt_total_ms,prompt_runs,model,text,word_count,wav_path " +
                        "FROM transcripts ORDER BY created_at DESC", null)) {
            while (c.moveToNext()) {
                Entry e = new Entry();
                int i = 0;
                e.id = c.getLong(i++); e.createdAt = c.getLong(i++); e.durationMs = c.getLong(i++);
                e.inferenceMs = c.getLong(i++); e.modelLoadMs = c.getLong(i++); e.pcmMs = c.getLong(i++);
                e.nativeTotalMs = c.getLong(i++); e.melMs = c.getLong(i++);
                e.encodeMs = c.getLong(i++); e.encodeTotalMs = c.getLong(i++); e.encodeRuns = c.getInt(i++);
                e.decodeMs = c.getLong(i++); e.decodeTotalMs = c.getLong(i++); e.decodeRuns = c.getInt(i++);
                e.sampleMs = c.getLong(i++); e.sampleTotalMs = c.getLong(i++); e.sampleRuns = c.getInt(i++);
                e.batchMs = c.getLong(i++); e.batchTotalMs = c.getLong(i++); e.batchRuns = c.getInt(i++);
                e.promptMs = c.getLong(i++); e.promptTotalMs = c.getLong(i++); e.promptRuns = c.getInt(i++);
                e.model = c.getString(i++); e.text = c.getString(i++); e.wordCount = c.getInt(i++);
                e.wavPath = c.isNull(i) ? null : c.getString(i);
                out.add(e);
            }
        }
        return out;
    }

    public Stats stats() {
        Stats s = new Stats();
        try (Cursor c = getReadableDatabase().rawQuery(
                "SELECT COUNT(*),COALESCE(SUM(word_count),0),COALESCE(SUM(duration_ms),0) FROM transcripts", null)) {
            if (c.moveToFirst()) {
                s.count = c.getInt(0); s.words = c.getLong(1); s.durationMs = c.getLong(2);
            }
        }
        return s;
    }
}
