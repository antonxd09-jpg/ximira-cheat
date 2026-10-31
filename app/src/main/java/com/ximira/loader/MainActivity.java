package com.ximira.loader;

import android.app.Activity;
import android.os.Bundle;
import android.view.WindowManager;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("ximira");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // контекст для NativeBridge (URL, path, clipboard, sound)
        NativeBridge.setContext(this);

        // не гасим экран и не блокируем процессор во время игры
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
}