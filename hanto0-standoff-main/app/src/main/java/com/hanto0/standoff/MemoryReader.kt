package com.hanto0.standoff

import android.util.Log
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder

object MemoryReader {
    private const val TAG = "hanto0_esp"
    private const val PKG = "com.axlebolt.standoff2"

    private const val OFF_PN_PEER      = 0x18L
    private const val OFF_NP_PLIST     = 0x1D0L
    private const val OFF_PP_TAG       = 0x40L
    private const val OFF_PP_NAME      = 0x20L
    private const val OFF_PP_ISLOCAL   = 0x30L
    private const val OFF_CP_PC        = 0x50L
    private const val OFF_PC_TRANSFORM = 0x100L
    private const val OFF_PC_HIT_CTRL  = 0xA8L
    private const val OFF_PC_CAM       = 0xE8L
    private const val OFF_HC_HEALTH    = 0x30L
    private const val OFF_TR_POSITION  = 0x10L
    private const val OFF_CAM_POS      = 0x10L
    private const val OFF_CAM_YAW      = 0x20L
    private const val OFF_CAM_PITCH    = 0x24L
    private const val IL2CPP_CLASS_NAME = 0x198L

    data class MapRange(val start: Long, val end: Long)

    private var pid     = -1
    private var mem: RandomAccessFile? = null
    private var libBase = 0L
    private var pnSF    = 0L
    private var camAddr = 0L

    data class Vec3(val x: Float, val y: Float, val z: Float)
    data class PlayerData(
        val addr: Long, val pos: Vec3, val bones: List<Vec3>,
        val health: Int, val team: Int, val name: String, val isLocal: Boolean
    )

    fun init(): Boolean {
        pid = getPid()
        Log.d(TAG, "pid=$pid")
        if (pid < 0) return false
        return try {
            val su = Runtime.getRuntime().exec(arrayOf("su", "-c", "chmod 777 /proc/$pid/mem"))
            su.waitFor()
            mem     = RandomAccessFile("/proc/$pid/mem", "rw")
            libBase = getLibBase()
            Log.d(TAG, "libBase=0x${libBase.toString(16)}")
            if (libBase == 0L) return false
            pnSF = findPhotonNetworkStatic()
            Log.d(TAG, "pnSF=0x${pnSF.toString(16)}")
            true
        } catch (e: Exception) { Log.e(TAG, "init fail: $e"); false }
    }

    fun close() { try { mem?.close() } catch (_: Exception) {}; mem = null }

    private fun getPid(): Int {
        return try {
            val proc = Runtime.getRuntime().exec(arrayOf("su", "-c",
                "grep -rl axlebolt /proc/[0-9]*/cmdline 2>/dev/null | head -1"))
            val out = proc.inputStream.bufferedReader().readLine() ?: return -1
            val pid = out.trim().split("/")[2].toIntOrNull() ?: return -1
            Log.d(TAG, "found pid=$pid")
            pid
        } catch (e: Exception) { Log.e(TAG, "getPid fail: $e"); -1 }
    }

    private fun getLibBase(): Long {
        try {
            val lines = File("/proc/$pid/maps").readLines()
            for (line in lines) {
                if (line.contains("libunity.so") && line.contains("r-xp")) {
                    val parts = line.trim().split("\\s+".toRegex())
                    val vaddr   = parts[0].split("-")[0].toLong(16)
                    val fileOff = parts[2].toLong(16)
                    return vaddr - fileOff
                }
            }
        } catch (e: Exception) { Log.e(TAG, "getLibBase fail: $e") }
        return 0L
    }

    // Get all readable libunity.so segments for scanning
    private fun getUnityRanges(): List<MapRange> {
        val ranges = mutableListOf<MapRange>()
        try {
            File("/proc/$pid/maps").readLines().forEach { line ->
                if (line.contains("libunity.so") && (line.contains("r--p") || line.contains("r-xp"))) {
                    val parts = line.trim().split("\\s+".toRegex())
                    val range = parts[0].split("-")
                    ranges.add(MapRange(range[0].toLong(16), range[1].toLong(16)))
                }
            }
        } catch (e: Exception) { Log.e(TAG, "getUnityRanges fail: $e") }
        return ranges
    }

    private fun findPhotonNetworkStatic(): Long {
        val target = "PhotonNetwork".toByteArray(Charsets.UTF_8)
        val ranges  = getUnityRanges()
        Log.d(TAG, "scanning ${ranges.size} ranges for PhotonNetwork string")

        // Step 1: find all addresses of the "PhotonNetwork" string
        val nameAddrs = mutableListOf<Long>()
        for (r in ranges) {
            val size = (r.end - r.start).coerceAtMost(64 * 1024 * 1024L).toInt()
            val buf  = readBytes(r.start, size)
            var i    = 0
            while (i <= buf.size - target.size) {
                var match = true
                for (j in target.indices) {
                    if (buf[i + j] != target[j]) { match = false; break }
                }
                if (match && (i + target.size >= buf.size || buf[i + target.size] == 0.toByte())) {
                    nameAddrs.add(r.start + i)
                }
                i++
            }
        }
        Log.d(TAG, "found ${nameAddrs.size} 'PhotonNetwork' strings")

        // Step 2: for each name address, scan for Il2CppClass* that has it at offset +0x198
        for (nameAddr in nameAddrs) {
            for (r in ranges) {
                val size = (r.end - r.start).coerceAtMost(64 * 1024 * 1024L).toInt()
                val buf  = ByteBuffer.wrap(readBytes(r.start, size)).order(ByteOrder.LITTLE_ENDIAN)
                var i    = 0
                while (i <= size - 8) {
                    val ptr = buf.getLong(i).toLong()
                    if (ptr == nameAddr) {
                        // potential Il2CppClass at r.start + i - 0x198
                        val klassAddr = r.start + i - IL2CPP_CLASS_NAME
                        if (klassAddr > 0) {
                            // try both known static_fields offsets
                            for (sfOff in listOf(0xB8L, 0x40L, 0xC0L, 0xD0L)) {
                                val sf = readLong(klassAddr + sfOff)
                                if (sf != 0L && sf > 0x700000000000L) {
                                    // verify: peerPtr at sf+0x18 should be non-zero valid ptr
                                    val peer = readLong(sf + OFF_PN_PEER)
                                    if (peer != 0L && peer > 0x700000000000L) {
                                        Log.d(TAG, "PhotonNetwork found! klass=0x${klassAddr.toString(16)} sfOff=0x${sfOff.toString(16)} sf=0x${sf.toString(16)} peer=0x${peer.toString(16)}")
                                        return sf
                                    }
                                }
                            }
                        }
                    }
                    i += 8
                }
            }
        }
        Log.w(TAG, "PhotonNetwork static fields not found via string scan")
        return 0L
    }

    fun readBytes(addr: Long, len: Int): ByteArray {
        val b = ByteArray(len)
        return try { mem!!.seek(addr); mem!!.read(b); b } catch (_: Exception) { b }
    }

    fun readInt(a: Long)   = ByteBuffer.wrap(readBytes(a,4)).order(ByteOrder.LITTLE_ENDIAN).int
    fun readLong(a: Long)  = ByteBuffer.wrap(readBytes(a,8)).order(ByteOrder.LITTLE_ENDIAN).long
    fun readFloat(a: Long) = java.lang.Float.intBitsToFloat(readInt(a))
    fun writeInt(a: Long, v: Int) {
        val b = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()
        try { mem!!.seek(a); mem!!.write(b) } catch (_: Exception) {}
    }
    fun writeFloat(a: Long, v: Float) = writeInt(a, java.lang.Float.floatToIntBits(v))
    fun readVec3(a: Long)  = Vec3(readFloat(a), readFloat(a+4), readFloat(a+8))

    fun readString16(a: Long): String {
        if (a == 0L) return ""
        return try {
            val len = readInt(a + 0x10).coerceIn(0, 64)
            if (len == 0) return ""
            String(readBytes(a + 0x14, len * 2), Charsets.UTF_16LE)
        } catch (_: Exception) { "" }
    }

    private fun readCStr(a: Long): String {
        if (a == 0L) return ""
        val sb = StringBuilder()
        for (i in 0 until 64) {
            val c = readInt(a + i) and 0xFF
            if (c == 0) break
            sb.append(c.toChar())
        }
        return sb.toString()
    }

    fun getCamPos():   Vec3  = if (camAddr==0L) Vec3(0f,0f,0f) else readVec3(camAddr+OFF_CAM_POS)
    fun getCamYaw():   Float = if (camAddr==0L) 0f else readFloat(camAddr+OFF_CAM_YAW)
    fun getCamPitch(): Float = if (camAddr==0L) 0f else readFloat(camAddr+OFF_CAM_PITCH)
    fun writeCamAngles(yaw: Float, pitch: Float) {
        if (camAddr==0L) return
        writeFloat(camAddr+OFF_CAM_YAW, yaw)
        writeFloat(camAddr+OFF_CAM_PITCH, pitch)
    }

    fun getPlayers(): List<PlayerData> {
        if (pnSF == 0L) { pnSF = findPhotonNetworkStatic() }
        if (pnSF == 0L) { Log.w(TAG, "pnSF still 0"); return emptyList() }
        return try {
            val peerPtr = readLong(pnSF + OFF_PN_PEER)
            if (peerPtr == 0L) { Log.w(TAG, "peerPtr=0"); return emptyList() }
            val listArr = readLong(peerPtr + OFF_NP_PLIST)
            if (listArr == 0L) { Log.w(TAG, "listArr=0"); return emptyList() }
            val count = readInt(listArr + 0x18)
            val items = readLong(listArr + 0x20)
            Log.d(TAG, "playerCount=$count")
            if (count <= 0 || count > 32 || items == 0L) return emptyList()
            val result = mutableListOf<PlayerData>()
            for (i in 0 until count) {
                val ppPtr = readLong(items + i * 8L); if (ppPtr == 0L) continue
                val isLocal = readInt(ppPtr + OFF_PP_ISLOCAL) != 0
                val name    = readString16(readLong(ppPtr + OFF_PP_NAME))
                val tagPtr  = readLong(ppPtr + OFF_PP_TAG); if (tagPtr == 0L) continue
                val cpPtr   = readLong(tagPtr + 0x10L);     if (cpPtr == 0L) continue
                val pcPtr   = readLong(cpPtr + OFF_CP_PC);  if (pcPtr == 0L) continue
                val trPtr   = readLong(pcPtr + OFF_PC_TRANSFORM); if (trPtr == 0L) continue
                val pos     = readVec3(trPtr + OFF_TR_POSITION)
                val hcPtr   = readLong(pcPtr + OFF_PC_HIT_CTRL); if (hcPtr == 0L) continue
                val hp      = readInt(hcPtr + OFF_HC_HEALTH)
                if (hp <= 0 || hp > 200) continue
                val team    = readInt(pcPtr + 0x38L)
                if (isLocal && camAddr == 0L) {
                    val cp = readLong(pcPtr + OFF_PC_CAM)
                    if (cp != 0L) { camAddr = cp; Log.d(TAG, "camAddr=0x${camAddr.toString(16)}") }
                }
                result.add(PlayerData(pcPtr, pos, buildBones(pos), hp, team, name, isLocal))
            }
            result
        } catch (e: Exception) { Log.e(TAG, "getPlayers fail: $e"); emptyList() }
    }

    private fun buildBones(o: Vec3) = listOf(
        Vec3(o.x,o.y+1.8f,o.z), Vec3(o.x,o.y+1.5f,o.z),
        Vec3(o.x,o.y+1.2f,o.z), Vec3(o.x,o.y+0.9f,o.z),
        Vec3(o.x-.4f,o.y+1.4f,o.z), Vec3(o.x-.4f,o.y+1.1f,o.z), Vec3(o.x-.4f,o.y+0.8f,o.z),
        Vec3(o.x+.4f,o.y+1.4f,o.z), Vec3(o.x+.4f,o.y+1.1f,o.z), Vec3(o.x+.4f,o.y+0.8f,o.z),
        Vec3(o.x-.2f,o.y+0.8f,o.z), Vec3(o.x-.2f,o.y+0.45f,o.z), Vec3(o.x-.2f,o.y,o.z),
        Vec3(o.x+.2f,o.y+0.8f,o.z), Vec3(o.x+.2f,o.y+0.45f,o.z), Vec3(o.x+.2f,o.y,o.z)
    )
}
