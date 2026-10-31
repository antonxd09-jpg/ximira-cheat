package com.hanto0.standoff

import android.app.Service
import android.content.Intent
import android.graphics.Color
import android.graphics.PixelFormat
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import android.view.*
import android.widget.*

class OverlayService : Service() {
    private lateinit var wm: WindowManager
    private lateinit var btn: Button
    private lateinit var menu: LinearLayout
    private lateinit var esp: EspView
    private var menuVisible = false
    private val handler = Handler(Looper.getMainLooper())

    var showBox=true; var showLines=true; var showSkeleton=true
    var showHP=true; var showInfo=true; var silentAim=true

    private val loop = object : Runnable {
        override fun run() {
            try {
                val players = MemoryReader.getPlayers()
                val cam = Projection.CamData(
                    MemoryReader.getCamPos(),
                    MemoryReader.getCamYaw(),
                    MemoryReader.getCamPitch(),
                    90f,
                    resources.displayMetrics.widthPixels,
                    resources.displayMetrics.heightPixels
                )
                if (silentAim) {
                    SilentAim.calc(cam.pos, cam.yaw, cam.pitch, players)?.let {
                        MemoryReader.writeCamAngles(it.yaw, it.pitch)
                    }
                }
                esp.showBox=showBox; esp.showLines=showLines
                esp.showSkeleton=showSkeleton; esp.showHP=showHP; esp.showInfo=showInfo
                esp.update(players, cam)
            } catch (e: Exception) {
                Log.e("hanto0_esp", "loop error: $e")
            }
            handler.postDelayed(this, 50)
        }
    }

    override fun onBind(i: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        Log.d("hanto0_esp", "OverlayService onCreate")
        try {
            val ok = MemoryReader.init()
            Log.d("hanto0_esp", "MemoryReader.init=$ok")
        } catch (e: Exception) {
            Log.e("hanto0_esp", "MemoryReader init error: $e")
        }
        wm = getSystemService(WINDOW_SERVICE) as WindowManager
        setupEsp(); setupBtn(); setupMenu()
        handler.post(loop)
    }

    private fun setupEsp() {
        esp = EspView(this)
        wm.addView(esp, WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE or
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT))
    }

    private fun setupBtn() {
        btn = Button(this).apply {
            text="ESP"; setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#AA1A4A8A")); textSize=12f
        }
        val p = WindowManager.LayoutParams(120,80,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
            PixelFormat.TRANSLUCENT).apply{
                gravity=Gravity.TOP or Gravity.START; x=0; y=200
            }
        var ix=0; var iy=0; var itx=0f; var ity=0f
        btn.setOnTouchListener{_,e->
            when(e.action){
                MotionEvent.ACTION_DOWN->{ix=p.x;iy=p.y;itx=e.rawX;ity=e.rawY;true}
                MotionEvent.ACTION_MOVE->{p.x=ix+(e.rawX-itx).toInt();p.y=iy+(e.rawY-ity).toInt();wm.updateViewLayout(btn,p);true}
                MotionEvent.ACTION_UP->{if(Math.abs(e.rawX-itx)<5&&Math.abs(e.rawY-ity)<5)toggleMenu();true}
                else->false
            }
        }
        wm.addView(btn, p)
    }

    private fun setupMenu() {
        menu = LinearLayout(this).apply {
            orientation=LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#EE0D0D0D"))
            setPadding(24,24,24,24); visibility=View.GONE
        }
        fun tog(label: String, init: Boolean, fn:(Boolean)->Unit) {
            val row=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;setPadding(0,8,0,8)}
            val tv=TextView(this).apply{text=label;setTextColor(Color.WHITE);textSize=14f
                layoutParams=LinearLayout.LayoutParams(0,LinearLayout.LayoutParams.WRAP_CONTENT,1f)}
            val sw=Switch(this).apply{isChecked=init;setOnCheckedChangeListener{_,b->fn(b)}}
            row.addView(tv);row.addView(sw);menu.addView(row)
        }
        menu.addView(TextView(this).apply{
            text="── hanto0 menu ──"
            setTextColor(Color.parseColor("#2299FF"))
            textSize=15f; gravity=android.view.Gravity.CENTER
        })
        tog("Silent Aim",  true){silentAim=it; SilentAim.enabled=it}
        tog("ESP Box",     true){showBox=it}
        tog("ESP Lines",   true){showLines=it}
        tog("ESP Skeleton",true){showSkeleton=it}
        tog("ESP HP",      true){showHP=it}
        tog("ESP Info",    true){showInfo=it}

        wm.addView(menu, WindowManager.LayoutParams(550,
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
            PixelFormat.TRANSLUCENT).apply{
                gravity=Gravity.TOP or Gravity.START; x=130; y=200
            })
    }

    private fun toggleMenu(){
        menuVisible=!menuVisible
        menu.visibility=if(menuVisible)View.VISIBLE else View.GONE
    }

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacks(loop)
        MemoryReader.close()
        listOf(esp,btn,menu).forEach{try{wm.removeView(it)}catch(_:Exception){}}
    }
}
