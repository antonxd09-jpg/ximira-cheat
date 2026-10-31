package com.hanto0.standoff

import android.util.Log

object ZeninLoader {
    private const val TAG = "hanto0_esp"
    var loaded = false

    fun load(): Boolean {
        if (loaded) return true
        return try {
            System.loadLibrary("hantoInternal")
            loaded = true
            Log.d(TAG, "libhantoInternal loaded OK")
            true
        } catch (e: Throwable) {
            Log.e(TAG, "load fail: $e")
            false
        }
    }
}
