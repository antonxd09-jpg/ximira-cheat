package com.hanto0.standoff

import kotlin.math.*

object SilentAim {
    var enabled = true
    var fovDeg  = 45f

    data class AimTarget(val yaw: Float, val pitch: Float)

    fun calc(
        camPos: MemoryReader.Vec3,
        camYaw: Float,
        camPitch: Float,
        players: List<MemoryReader.PlayerData>
    ): AimTarget? {
        if (!enabled || players.isEmpty()) return null
        var bestAngle = fovDeg
        var bestTarget: AimTarget? = null
        for (p in players) {
            if (p.isLocal) continue
            val dx = p.pos.x - camPos.x
            val dy = (p.pos.y + 1.4f) - camPos.y
            val dz = p.pos.z - camPos.z
            val horizDist = sqrt(dx * dx + dz * dz)
            val tYaw   = Math.toDegrees(atan2(dx.toDouble(), dz.toDouble())).toFloat()
            val tPitch = Math.toDegrees(atan2(dy.toDouble(), horizDist.toDouble())).toFloat()
            val dYaw   = normAngle(tYaw   - camYaw)
            val dPitch = normAngle(tPitch - camPitch)
            val angle  = sqrt(dYaw * dYaw + dPitch * dPitch)
            if (angle < bestAngle) {
                bestAngle  = angle
                bestTarget = AimTarget(tYaw, tPitch)
            }
        }
        return bestTarget
    }

    private fun normAngle(a: Float): Float {
        var r = a % 360f
        if (r >  180f) r -= 360f
        if (r < -180f) r += 360f
        return r
    }
}
