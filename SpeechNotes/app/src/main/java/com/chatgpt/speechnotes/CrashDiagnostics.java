package com.chatgpt.speechnotes;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;

import java.text.DateFormat;
import java.util.Date;
import java.util.List;

public final class CrashDiagnostics {
    private static final String PREF = "speech_crash_diag";
    private static final String KEY_STAGE = "stage";
    private static final String KEY_TIME = "time";

    private CrashDiagnostics() {}

    public static void mark(Context context, String stage) {
        if (context == null) return;
        try {
            context.getSharedPreferences(PREF, Context.MODE_PRIVATE)
                    .edit()
                    .putString(KEY_STAGE, stage == null ? "" : stage)
                    .putLong(KEY_TIME, System.currentTimeMillis())
                    .commit();
        } catch (Throwable ignored) { }
    }

    public static void markError(Context context, String stage, Throwable t) {
        String msg = stage;
        if (t != null) {
            msg += " | " + t.getClass().getSimpleName() + ": " + String.valueOf(t.getMessage());
        }
        mark(context, msg);
    }

    public static String summary(Context context) {
        if (context == null) return "";
        StringBuilder out = new StringBuilder();
        try {
            SharedPreferences p = context.getSharedPreferences(PREF, Context.MODE_PRIVATE);
            String stage = p.getString(KEY_STAGE, "");
            long time = p.getLong(KEY_TIME, 0L);
            if (stage != null && !stage.isEmpty() && !"ui:onCreate".equals(stage)) {
                out.append("last_stage=").append(stage);
                if (time > 0) {
                    out.append("\nlast_stage_time=")
                            .append(DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.MEDIUM)
                                    .format(new Date(time)));
                }
            }
        } catch (Throwable ignored) { }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            appendExitInfo(context, out);
        }
        return out.toString();
    }

    @TargetApi(Build.VERSION_CODES.R)
    private static void appendExitInfo(Context context, StringBuilder out) {
        try {
            ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            List<ApplicationExitInfo> exits = am.getHistoricalProcessExitReasons(context.getPackageName(), 0, 5);
            ApplicationExitInfo e = firstRelevantExit(exits);
            if (e != null) {
                if (out.length() > 0) out.append('\n');
                out.append("last_exit_reason=").append(reasonName(e.getReason()))
                        .append('(').append(e.getReason()).append(')')
                        .append(" status=").append(e.getStatus())
                        .append(" importance=").append(e.getImportance())
                        .append(" pss_kb=").append(e.getPss())
                        .append(" rss_kb=").append(e.getRss());
                CharSequence d = e.getDescription();
                if (d != null && d.length() > 0) out.append("\ndescription=").append(d);
                out.append("\nexit_time=")
                        .append(DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.MEDIUM)
                                .format(new Date(e.getTimestamp())));
            }
        } catch (Throwable ignored) { }
    }

    @TargetApi(Build.VERSION_CODES.R)
    private static ApplicationExitInfo firstRelevantExit(List<ApplicationExitInfo> exits) {
        if (exits == null) return null;
        for (ApplicationExitInfo e : exits) {
            if (e == null) continue;
            CharSequence d = e.getDescription();
            String description = d == null ? "" : d.toString();
            // PackageManager deliberately kills the old process while installing an update.
            // That is expected lifecycle noise, not an application crash.
            if (description.contains("installPackageLI") || description.contains("installPackage")) continue;
            return e;
        }
        return null;
    }

    @TargetApi(Build.VERSION_CODES.R)
    private static String reasonName(int reason) {
        switch (reason) {
            case ApplicationExitInfo.REASON_ANR: return "ANR";
            case ApplicationExitInfo.REASON_CRASH: return "CRASH_JAVA";
            case ApplicationExitInfo.REASON_CRASH_NATIVE: return "CRASH_NATIVE";
            case ApplicationExitInfo.REASON_DEPENDENCY_DIED: return "DEPENDENCY_DIED";
            case ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE: return "EXCESSIVE_RESOURCE";
            case ApplicationExitInfo.REASON_EXIT_SELF: return "EXIT_SELF";
            case ApplicationExitInfo.REASON_INITIALIZATION_FAILURE: return "INIT_FAILURE";
            case ApplicationExitInfo.REASON_LOW_MEMORY: return "LOW_MEMORY";
            case ApplicationExitInfo.REASON_PERMISSION_CHANGE: return "PERMISSION_CHANGE";
            case ApplicationExitInfo.REASON_SIGNALED: return "SIGNALED";
            case ApplicationExitInfo.REASON_USER_REQUESTED: return "USER_REQUESTED";
            default: return "OTHER";
        }
    }
}
