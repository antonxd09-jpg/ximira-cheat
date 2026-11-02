package com.ximira.loader;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.widget.*;

import java.io.*;

public class MainActivity extends Activity {
    static { System.loadLibrary("ximira"); }

    private TextView statusText;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private static final int BG     = Color.rgb(12, 15, 23);
    private static final int PANEL  = Color.rgb(20, 25, 37);
    private static final int ACCENT = Color.rgb(151, 174, 255);
    private static final int TEXT   = Color.rgb(238, 242, 255);
    private static final int MUTED  = Color.rgb(133, 145, 169);
    private static final int GREEN  = Color.rgb(100, 220, 140);
    private static final int RED    = Color.rgb(255, 90, 90);

    private static String findSu() {
        for (String p : new String[]{SU,"/system/xbin/su","/su/bin/su","/sbin/su","/magisk/.core/bin/su"})
            if (new java.io.File(p).exists()) return p;
        return "su";
    }
    private static final String SU = findSu();

    @Override
    protected void onCreate(Bundle s) {
        super.onCreate(s);
        NativeBridge.setContext(this);
        getWindow().getDecorView().setBackgroundColor(BG);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setPadding(dp(24), dp(48), dp(24), dp(24));

        root.addView(label("XIMIRA  /  LOADER", 11, ACCENT, true));
        root.addView(label("Standoff 2 Cheat", 28, TEXT, true));
        root.addView(label("ImGui · GLES3 Hook · root required", 12, MUTED, false));

        LinearLayout card = card();
        statusText = label("○ Ожидание", 14, MUTED, true);
        card.addView(statusText);

        TextView btn = btn("ИНЖЕКТИРОВАТЬ", ACCENT);
        btn.setOnClickListener(v -> inject());
        card.addView(btn, lp(10, 0));

        TextView stopBtn = btn("СТОП", RED);
        stopBtn.setOnClickListener(v -> stop());
        card.addView(stopBtn, lp(6, 0));

        root.addView(card, lp(20, 0));
        root.addView(label("Запусти Standoff 2 и зайди в матч перед инжектом", 12, MUTED, false));
        setContentView(root);
    }

    private void inject() {
        setStatus("⏳ Поиск Standoff 2...", MUTED);
        new Thread(() -> {
            try {
                // find libximira.so in app's native lib dir
                String soSrc = getApplicationInfo().nativeLibraryDir + "/libximira.so";
                String soDst = "/data/local/tmp/libximira.so";
                String injectorSrc = "/sdcard/komarq_full/manualmap_injector";
                String injector = "/data/local/tmp/manualmap_injector";

                // copy so to tmp
                Process cp = Runtime.getRuntime().exec(new String[]{SU, "-c",
                        "cp " + soSrc + " " + soDst + " && cp " + injectorSrc + " " + injector + " && chmod 777 " + soDst + " && chmod +x " + injector});
                cp.waitFor();

                // check standoff running
                Process ps = Runtime.getRuntime().exec(new String[]{SU, "-c",
                        "pidof com.axlebolt.standoff2"});
                String pid = new BufferedReader(new InputStreamReader(ps.getInputStream())).readLine();
                ps.waitFor();

                if (pid == null || pid.trim().isEmpty()) {
                    handler.post(() -> setStatus("❌ Standoff 2 не запущен", RED));
                    return;
                }

                handler.post(() -> setStatus("⏳ Инжектирование...", MUTED));

                // inject
                Process inj = Runtime.getRuntime().exec(new String[]{SU, "-c",
                        injector + " com.axlebolt.standoff2 " + soDst});
                String out = new BufferedReader(new InputStreamReader(inj.getInputStream())).readLine();
                int rc = inj.waitFor();

                handler.post(() -> {
                    if (rc == 0) setStatus("● Меню активно — смотри в игру", GREEN);
                    else setStatus("❌ Ошибка инжекта (код " + rc + ")", RED);
                });
            } catch (Exception e) {
                handler.post(() -> setStatus("❌ " + e.getMessage(), RED));
            }
        }).start();
    }

    private void stop() {
        new Thread(() -> {
            try {
                Runtime.getRuntime().exec(new String[]{SU, "-c",
                        "killall manualmap_injector 2>/dev/null"}).waitFor();
                handler.post(() -> setStatus("○ Остановлено", MUTED));
            } catch (Exception e) {}
        }).start();
    }

    private void setStatus(String msg, int color) {
        statusText.setText(msg); statusText.setTextColor(color);
    }
    private LinearLayout card() {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        GradientDrawable d = new GradientDrawable();
        d.setColor(PANEL); d.setCornerRadius(dp(18));
        l.setBackground(d);
        l.setPadding(dp(16), dp(14), dp(16), dp(14));
        return l;
    }
    private TextView btn(String t, int color) {
        TextView v = new TextView(this);
        v.setText(t); v.setGravity(Gravity.CENTER);
        v.setTextSize(13); v.setTypeface(Typeface.DEFAULT_BOLD);
        v.setTextColor(color == ACCENT ? Color.rgb(15,20,32) : TEXT);
        GradientDrawable d = new GradientDrawable();
        d.setColor(color); d.setCornerRadius(dp(12));
        v.setBackground(d); v.setPadding(0, dp(14), 0, dp(14));
        return v;
    }
    private TextView label(String t, int sz, int c, boolean bold) {
        TextView v = new TextView(this);
        v.setText(t); v.setTextSize(sz); v.setTextColor(c);
        if (bold) v.setTypeface(Typeface.DEFAULT_BOLD);
        return v;
    }
    private LinearLayout.LayoutParams lp(int top, int bot) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, -2);
        p.topMargin = dp(top); p.bottomMargin = dp(bot); return p;
    }
    private int dp(int v) { return Math.round(v * getResources().getDisplayMetrics().density); }
}
