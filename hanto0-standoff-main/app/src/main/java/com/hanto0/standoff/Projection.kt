package com.hanto0.standoff

import android.graphics.PointF
import kotlin.math.*

object Projection {
    data class CamData(
        val pos: MemoryReader.Vec3,
        val yaw: Float, val pitch: Float, val fov: Float,
        val sw: Int, val sh: Int
    )

    fun worldToScreen(w: MemoryReader.Vec3, c: CamData): PointF? {
        val dx = w.x - c.pos.x
        val dy = w.y - c.pos.y
        val dz = w.z - c.pos.z
        val yr = Math.toRadians(c.yaw.toDouble())
        val pr = Math.toRadians(c.pitch.toDouble())
        val cx =  dx * cos(yr) - dz * sin(yr)
        val cz =  dx * sin(yr) + dz * cos(yr)
        val cy =  dy * cos(pr) - cz * sin(pr)
        val cd =  dy * sin(pr) + cz * cos(pr)
        if (cd < 0.1) return null
        val half  = tan(Math.toRadians(c.fov * 0.5))
        val scale = (c.sw * 0.5) / half
        return PointF(
            (c.sw * 0.5 + cx / cd * scale).toFloat(),
            (c.sh * 0.5 - cy / cd * scale).toFloat()
        )
    }

    fun dist(a: MemoryReader.Vec3, b: MemoryReader.Vec3): Float {
        val dx = a.x - b.x
        val dy = a.y - b.y
        val dz = a.z - b.z
        return sqrt(dx * dx + dy * dy + dz * dz)
    }
}
