package dev.periscope.lens

import android.annotation.SuppressLint
import android.content.Context
import java.io.File
import java.io.IOException

/**
 * L8 path-1 helper: USB-HID gadget feature detection and report writing
 * (ledger L8). The mechanism is the Android 10+ configfs USB gadget
 * (`/config/usb_gadget`); support varies per device/kernel, so detection is
 * mandatory and failure degrades loudly to targets-only — never silently.
 *
 * Runtime-unverified by house style (no Android test lane in this repo);
 * the pure report-shaping rules live on the PC side in
 * `application/lens/output.py` and are mirrored by [encodeReport] here.
 */
object HidGadgetHelper {

    const val GADGET_CONFIG_DIR = "/config/usb_gadget"
    const val HID_AXIS_MIN = -127
    const val HID_AXIS_MAX = 127

    /**
     * Returns true only when the gadget filesystem is present, writable, and
     * a HID function can be created. Never assumes: a false return sends the
     * caller to the targets-only path with a user-visible message.
     */
    fun probe(): Boolean {
        val root = File(GADGET_CONFIG_DIR)
        if (!root.isDirectory) return false
        if (!root.canWrite()) return false
        // A gadget function directory we can create and remove is the probe.
        return try {
            val probe = File(root, "functions/hid.periscope_probe")
            val created = probe.mkdirs()
            if (created) probe.delete() else probe.isDirectory
            created || probe.isDirectory
        } catch (_: SecurityException) {
            false
        } catch (_: IOException) {
            false
        }
    }

    /**
     * Encodes the standard 3-byte relative mouse report: buttons, dx, dy.
     * Values must already be in range — [dev.periscope.lens.PolicyShaper]
     * guarantees that upstream; this function refuses anything else so a
     * clamp bug cannot become a wrapped axis (a cursor teleport).
     */
    fun encodeReport(dx: Int, dy: Int, buttons: Int = 0): ByteArray {
        require(dx in HID_AXIS_MIN..HID_AXIS_MAX) { "dx out of HID range: $dx" }
        require(dy in HID_AXIS_MIN..HID_AXIS_MAX) { "dy out of HID range: $dy" }
        require(buttons in 0..0xFF) { "buttons must fit a byte" }
        return byteArrayOf(buttons.toByte(), dx.toByte(), dy.toByte())
    }

    /**
     * Writes one report to the gadget's event device. The caller owns the
     * open handle lifecycle; errors surface as exceptions so the runner can
     * degrade to targets-only with a message.
     */
    @SuppressLint("NewApi")
    fun writeReport(handle: FileDescriptorBox, report: ByteArray) {
        handle.write(report)
    }

    /** Minimal write surface so this object stays testable without Android. */
    class FileDescriptorBox(private val writer: (ByteArray) -> Unit) {
        fun write(report: ByteArray) = writer(report)
    }

    /** Convenience for the runner: describe the detected path for the UI. */
    fun describe(available: Boolean, context: Context?): String =
        if (available) {
            "HID gadget available: phone acts as the assist mouse"
        } else {
            "HID gadget unavailable on this device: targets-only mode " +
                "(PC side owns any motion policy)"
        }
}
