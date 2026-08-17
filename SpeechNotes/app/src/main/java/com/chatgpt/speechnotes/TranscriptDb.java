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
    private static final int DB_VERSION = 1;

    public static final class Entry {
        public long id;
        public long createdAt;
        public long durationMs;
        public long inferenceMs;
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

    public TranscriptDb(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }

    @Override public void onCreate(SQLiteDatabase db) {
        db.execSQL("CREATE TABLE transcripts (" +
                "id INTEGER PRIMARY KEY AUTOINCREMENT," +
                "created_at INTEGER NOT NULL," +
                "duration_ms INTEGER NOT NULL," +
                "inference_ms INTEGER NOT NULL," +
                "model TEXT NOT NULL," +
                "text TEXT NOT NULL," +
                "word_count INTEGER NOT NULL," +
                "wav_path TEXT)");
    }

    @Override public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) { }

    public long insert(long createdAt, long durationMs, long inferenceMs, String model,
                       String text, int wordCount, String wavPath) {
        ContentValues v = new ContentValues();
        v.put("created_at", createdAt);
        v.put("duration_ms", durationMs);
        v.put("inference_ms", inferenceMs);
        v.put("model", model);
        v.put("text", text);
        v.put("word_count", wordCount);
        if (wavPath == null) v.putNull("wav_path"); else v.put("wav_path", wavPath);
        return getWritableDatabase().insert("transcripts", null, v);
    }

    public List<Entry> list() {
        ArrayList<Entry> out = new ArrayList<>();
        try (Cursor c = getReadableDatabase().rawQuery(
                "SELECT id,created_at,duration_ms,inference_ms,model,text,word_count,wav_path " +
                        "FROM transcripts ORDER BY created_at DESC", null)) {
            while (c.moveToNext()) {
                Entry e = new Entry();
                e.id = c.getLong(0);
                e.createdAt = c.getLong(1);
                e.durationMs = c.getLong(2);
                e.inferenceMs = c.getLong(3);
                e.model = c.getString(4);
                e.text = c.getString(5);
                e.wordCount = c.getInt(6);
                e.wavPath = c.isNull(7) ? null : c.getString(7);
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
                s.count = c.getInt(0);
                s.words = c.getLong(1);
                s.durationMs = c.getLong(2);
            }
        }
        return s;
    }
}
