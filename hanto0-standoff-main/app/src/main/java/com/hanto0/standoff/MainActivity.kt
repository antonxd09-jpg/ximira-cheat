package com.hanto0.standoff

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.widget.*

class MainActivity : Activity() {
    private val OVERLAY_PERM = 1234

    override fun onCreate(s: Bundle?) {
        super.onCreate(s)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(80, 32, 80, 32)
            setBackgroundColor(android.graphics.Color.parseColor("#0D0D0D"))
        }
        fun tv(t: String, sz: Float, col: String) = TextView(this).apply {
            text = t; textSize = sz
            setTextColor(android.graphics.Color.parseColor(col))
            gravity = android.view.Gravity.CENTER
        }
        root.addView(tv("hanto0", 28f, "#2299FF"))
        root.addView(tv("Standoff 2 ESP | v2.1", 11f, "#555555"))
        root.addView(tv("Требуется ROOT + запущенная игра", 11f, "#AA5555"))

        val btn = Button(this).apply {
            text = "СТАРТ"
            setTextColor(android.graphics.Color.WHITE)
            setBackgroundColor(android.graphics.Color.parseColor("#1A4A8A"))
        }
        btn.setOnClickListener {
            if (!Settings.canDrawOverlays(this)) {
                startActivityForResult(
                    Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                        Uri.parse("package:$packageName")), OVERLAY_PERM)
            } else {
                startService(Intent(this, OverlayService::class.java))
                Toast.makeText(this, "ESP запущен", Toast.LENGTH_SHORT).show()
            }
        }
        root.addView(btn)
        setContentView(root)
    }

    override fun onActivityResult(req: Int, res: Int, data: Intent?) {
        if (req == OVERLAY_PERM && Settings.canDrawOverlays(this))
            startService(Intent(this, OverlayService::class.java))
    }
}
