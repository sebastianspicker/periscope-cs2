package dev.periscope.lens

/**
 * L6 wire-protocol encoder (ledger L6) — the Kotlin mirror of
 * `vision/src/cs2_vision_access/application/lens/transport.py`.
 *
 * Frame layout (little-endian, packed — `<` structs have no alignment
 * padding): `u8 version, u8 type, u32 payloadLength, payload`.
 *
 * Per-box payload: `u8 classId, f32 conf, f32 cx, f32 cy, f32 w, f32 h`
 * (21 bytes); targets head: `u64 frameId, u64 tCaptureNs, u8 count` (17).
 *
 * Deliberately pure: no Android imports, so a JVM unit test can mirror the
 * Python decoder's fixtures byte-for-byte when CI lands.
 */
object TargetsEncoder {
    const val PROTOCOL_VERSION: Int = 1
    const val TYPE_HELLO: Int = 1
    const val TYPE_TARGETS: Int = 2
    const val TYPE_PING: Int = 3
    const val TYPE_PONG: Int = 4
    const val TYPE_STATE: Int = 6
    const val MAX_FRAME_BYTES: Int = 512 * 1024

    private val BOX_BYTES = 21
    private val HEAD_BYTES = 17

    data class TargetBox(
        val classId: Int,
        val confidence: Float,
        val cx: Float,
        val cy: Float,
        val width: Float,
        val height: Float,
    )

    data class Targets(
        val frameId: Long,
        val tCaptureNs: Long,
        val boxes: List<TargetBox>,
    )

    private fun frame(type: Int, payload: ByteArray): ByteArray {
        require(payload.size <= MAX_FRAME_BYTES) { "payload exceeds frame cap" }
        val out = ByteArray(6 + payload.size)
        out[0] = PROTOCOL_VERSION.toByte()
        out[1] = type.toByte()
        val length = payload.size
        for (index in 0 until 4) {
            out[2 + index] = ((length shr (8 * index)) and 0xFF).toByte()
        }
        payload.copyInto(out, 6)
        return out
    }

    private fun putU32(value: Long, out: ByteArray, offset: Int) {
        var v = value
        for (index in 0 until 4) {
            out[offset + index] = (v and 0xFF).toByte()
            v = v shr 8
        }
    }

    private fun putU64(value: Long, out: ByteArray, offset: Int) {
        var v = value
        for (index in 0 until 8) {
            out[offset + index] = (v and 0xFF).toByte()
            v = v shr 8
        }
    }

    private fun putF32(value: Float, out: ByteArray, offset: Int) =
        putU32(java.lang.Float.floatToIntBits(value).toLong(), out, offset)

    /** Encodes a `Targets` frame. `tCaptureNs` MUST be the Camera2
     * `sensorTimestamp` — L7 compensation depends on it, never on send time. */
    fun encode(targets: Targets): ByteArray {
        val body = ByteArray(HEAD_BYTES + targets.boxes.size * BOX_BYTES)
        putU64(targets.frameId, body, 0)
        putU64(targets.tCaptureNs, body, 8)
        require(targets.boxes.size <= 255) { "too many boxes for u8 count" }
        body[16] = targets.boxes.size.toByte()
        targets.boxes.forEachIndexed { index, box ->
            val offset = HEAD_BYTES + index * BOX_BYTES
            body[offset] = box.classId.toByte()
            putF32(box.confidence, body, offset + 1)
            putF32(box.cx, body, offset + 5)
            putF32(box.cy, body, offset + 9)
            putF32(box.width, body, offset + 13)
            putF32(box.height, body, offset + 17)
        }
        return frame(TYPE_TARGETS, body)
    }

    fun encodeHello(appBuild: Int, flags: Int): ByteArray {
        val payload = ByteArray(8)
        putU32(appBuild.toLong(), payload, 0)
        putU32(flags.toLong(), payload, 4)
        return frame(TYPE_HELLO, payload)
    }

    fun encodePing(t0Ns: Long, isReply: Boolean): ByteArray {
        val payload = ByteArray(8)
        putU64(t0Ns, payload, 0)
        return frame(if (isReply) 4 else TYPE_PING, payload)
    }
}
