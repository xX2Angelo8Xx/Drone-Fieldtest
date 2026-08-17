package com.chatgpt.speechnotes;

import android.content.Context;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public final class ModelManager {
    private ModelManager() {}

    public static File ensureModel(Context context, String modelName) throws IOException {
        String assetName = "models/ggml-" + modelName + ".bin";
        File dir = new File(context.getFilesDir(), "models");
        if (!dir.exists() && !dir.mkdirs()) throw new IOException("Modellordner konnte nicht erstellt werden");
        File dst = new File(dir, "ggml-" + modelName + ".bin");
        if (dst.exists() && dst.length() > 10_000_000) return dst;

        File tmp = new File(dir, dst.getName() + ".tmp");
        if (tmp.exists()) tmp.delete();
        try (InputStream in = new BufferedInputStream(context.getAssets().open(assetName), 1024 * 1024);
             BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(tmp), 1024 * 1024)) {
            byte[] buffer = new byte[1024 * 1024];
            int n;
            while ((n = in.read(buffer)) >= 0) out.write(buffer, 0, n);
        }
        if (dst.exists() && !dst.delete()) throw new IOException("Altes Modell konnte nicht ersetzt werden");
        if (!tmp.renameTo(dst)) throw new IOException("Modell konnte nicht finalisiert werden");
        return dst;
    }
}
