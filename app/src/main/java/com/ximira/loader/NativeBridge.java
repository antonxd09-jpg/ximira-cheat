package com.ximira.loader;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.media.MediaPlayer;
import android.net.Uri;

import java.util.ArrayList;
import java.util.List;

public final class NativeBridge {

    private static Context sAppContext;

    // пул MediaPlayer для hitsound — 8 штук, чтобы хиты не резались
    private static final int MAX_PLAYERS = 8;
    private static final List<MediaPlayer> sPool = new ArrayList<>();

    public static void setContext(Context ctx) {
        sAppContext = ctx.getApplicationContext();
    }

    // ---- URL / path ----
    public static void openUrl(String url) {
        if (sAppContext == null) return;
        try {
            Intent i = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            sAppContext.startActivity(i);
        } catch (Exception ignored) {}
    }

    public static void openPath(String path) {
        if (sAppContext == null) return;
        try {
            Intent i = new Intent(Intent.ACTION_VIEW);
            i.setDataAndType(Uri.parse("file://" + path), "resource/folder");
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            sAppContext.startActivity(i);
        } catch (Exception e) {
            try {
                Intent i = new Intent(Intent.ACTION_GET_CONTENT);
                i.setType("*/*");
                i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                sAppContext.startActivity(i);
            } catch (Exception ignored) {}
        }
    }

    // ---- sound ----
    public static synchronized void playSound(String path, float vol) {
        if (sAppContext == null || path == null) return;

        MediaPlayer player = null;
        for (MediaPlayer p : sPool) {
            try {
                if (!p.isPlaying()) { player = p; break; }
            } catch (IllegalStateException ignored) {}
        }
        if (player == null) {
            if (sPool.size() < MAX_PLAYERS) {
                player = new MediaPlayer();
                sPool.add(player);
            } else {
                player = sPool.remove(0);
                try { player.release(); } catch (Exception ignored) {}
                player = new MediaPlayer();
                sPool.add(player);
            }
        }

        try {
            player.reset();
            player.setDataSource(path);
            player.setVolume(vol, vol);
            player.prepare();
            player.start();
        } catch (Exception e) {
            try { player.reset(); } catch (Exception ignored) {}
        }
    }

    public static synchronized void stopSound() {
        for (MediaPlayer p : sPool) {
            try { p.stop(); p.release(); } catch (Exception ignored) {}
        }
        sPool.clear();
    }

    // ---- clipboard (для config export/import) ----
    public static void setClipboard(String text) {
        if (sAppContext == null || text == null) return;
        try {
            ClipboardManager cm = (ClipboardManager)
                    sAppContext.getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm != null) {
                cm.setPrimaryClip(ClipData.newPlainText("ximira_config", text));
            }
        } catch (Exception ignored) {}
    }

    public static String getClipboard() {
        if (sAppContext == null) return "";
        try {
            ClipboardManager cm = (ClipboardManager)
                    sAppContext.getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm == null || !cm.hasPrimaryClip()) return "";
            ClipData clip = cm.getPrimaryClip();
            if (clip == null || clip.getItemCount() == 0) return "";
            CharSequence txt = clip.getItemAt(0).coerceToText(sAppContext);
            return txt != null ? txt.toString() : "";
        } catch (Exception e) {
            return "";
        }
    }
}