package com.hanto0.standoff

import android.content.Context
import android.graphics.*
import android.view.View
import kotlin.math.abs

class EspView(context: Context) : View(context) {

    var players: List<MemoryReader.PlayerData> = emptyList()
    var camData: Projection.CamData? = null
    var showBox      = true
    var showLines    = true
    var showSkeleton = true
    var showHP       = true
    var showInfo     = true

    private val pRed   = Paint().apply { color=Color.RED;    strokeWidth=3f; style=Paint.Style.STROKE; isAntiAlias=true }
    private val pCyan  = Paint().apply { color=Color.CYAN;   strokeWidth=3f; style=Paint.Style.STROKE; isAntiAlias=true }
    private val pGreen = Paint().apply { color=Color.GREEN;  strokeWidth=2f; style=Paint.Style.STROKE; isAntiAlias=true }
    private val pLine  = Paint().apply { color=Color.YELLOW; strokeWidth=2f; isAntiAlias=true }
    private val pText  = Paint().apply { color=Color.WHITE;  textSize=26f;   isAntiAlias=true }
    private val pHP    = Paint().apply { style=Paint.Style.FILL; isAntiAlias=true }

    private val SEGS = listOf(
        0 to 1, 1 to 2, 2 to 3,
        1 to 4, 4 to 5, 5 to 6,
        1 to 7, 7 to 8, 8 to 9,
        3 to 10, 10 to 11, 11 to 12,
        3 to 13, 13 to 14, 14 to 15
    )

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val cam = camData ?: return
        val cx = width / 2f
        val cy = height.toFloat()

        for (p in players) {
            if (p.isLocal) continue
            val head  = MemoryReader.Vec3(p.pos.x, p.pos.y + 1.8f, p.pos.z)
            val chest = MemoryReader.Vec3(p.pos.x, p.pos.y + 1.2f, p.pos.z)
            val feet  = p.pos

            val sHead  = Projection.worldToScreen(head,  cam) ?: continue
            val sFeet  = Projection.worldToScreen(feet,  cam) ?: continue
            val sChest = Projection.worldToScreen(chest, cam) ?: continue

            val h = abs(sFeet.y - sHead.y)
            if (h < 5f) continue
            val w = h / 3f
            val paint = if (p.team == 1) pRed else pCyan

            if (showLines)
                canvas.drawLine(cx, cy, sChest.x, sChest.y, pLine)

            if (showBox)
                canvas.drawRect(sHead.x - w, sHead.y, sHead.x + w, sFeet.y, paint)

            if (showHP) {
                val ratio = (p.health / 100f).coerceIn(0f, 1f)
                pHP.color = when {
                    ratio > 0.6f -> Color.GREEN
                    ratio > 0.3f -> Color.YELLOW
                    else -> Color.RED
                }
                canvas.drawRect(
                    sHead.x - w - 6f, sHead.y + h * (1f - ratio),
                    sHead.x - w - 2f, sFeet.y, pHP
                )
            }

            if (showInfo) {
                canvas.drawText("HP:${p.health}", sHead.x - w, sHead.y - 4f, pText)
                val dist = Projection.dist(p.pos, cam.pos)
                canvas.drawText("${dist.toInt()}m", sHead.x - w, sHead.y - 24f, pText)
                if (p.name.isNotEmpty())
                    canvas.drawText(p.name, sHead.x - w, sHead.y - 44f, pText)
            }

            if (showSkeleton && p.bones.size >= 16) {
                val sp = p.bones.map { Projection.worldToScreen(it, cam) }
                for ((a, b) in SEGS) {
                    val pa = sp.getOrNull(a) ?: continue
                    val pb = sp.getOrNull(b) ?: continue
                    canvas.drawLine(pa.x, pa.y, pb.x, pb.y, pGreen)
                }
            }
        }
    }

    fun update(list: List<MemoryReader.PlayerData>, cam: Projection.CamData?) {
        players = list
        camData = cam
        invalidate()
    }
}
