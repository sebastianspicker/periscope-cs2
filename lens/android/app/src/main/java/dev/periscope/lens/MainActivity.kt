package dev.periscope.lens

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import java.net.ServerSocket

/**
 * Lens topology-B entry point (ledger L5).
 *
 * What is actually wired here: the capability report (honesty rule, the
 * user sees what was detected, never what was configured) and the L6
 * transport listener lifecycle. The Camera2 capture loop, the ORT session
 * and the per-frame IPC into the transport are seams: the pipeline shape is
 * fixed by the surrounding modules, but this activity does not yet drive
 * frames (see `lens/android/README.md`'s gaps list).
 */
class MainActivity: Activity() {

    private lateinit var status: TextView
    private var transportThread: Thread? = null
    private var transportServer: ServerSocket? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        status = TextView(this)
        setContentView(status)
        status.text = describeCapabilities()
        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.CAMERA), CAMERA_REQUEST)
        }
    }

    override fun onResume() {
        super.onResume()
        if (transportThread?.isAlive != true) {
            transportThread = startTransport().also { it.start() }
        }
    }

    override fun onPause() {
        // accept() is not interruptible; closing the server socket ends it.
        runCatching { transportServer?.close() }
        TransportHub.detachAll()
        // Give accept() a bounded moment to unwind before onResume can rebind.
        val thread = transportThread
        runCatching { thread?.join(500) }
        if (thread?.isAlive != true) transportThread = null
        super.onPause()
    }

    /**
     * Capability report shown at startup, the honesty rule from V3/L8:
     * the user sees what was actually detected, never what was configured.
     */
    private fun describeCapabilities(): String {
        val cameraManager = getSystemService(Context.CAMERA_SERVICE) as CameraManager
        val highSpeed = cameraManager.cameraIdList.any { id ->
            val characteristics = cameraManager.getCameraCharacteristics(id)
            val capabilities = characteristics.get(
                CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES
            )
            capabilities?.contains(
                CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO
            ) == true
        }
        val hid = HidGadgetHelper.probe()
        return buildString {
            append("Periscope Lens\n")
            append("camera high-speed 60 fps: ").append(if (highSpeed) "yes" else "no").append('\n')
            append(HidGadgetHelper.describe(hid, this@MainActivity)).append('\n')
            append("transport: adb forward tcp:8742 (fallback RNDIS)")
        }
    }

    /**
     * The assist-path transport listener: one socket, latest-frame semantics
     * per connection (the L6 receiver rule, never queue). Runs only while
     * the activity is foreground; the camera loop feeds [TransportHub].
     */
    private fun startTransport(): Thread {
        return Thread {
            try {
                ServerSocket(8742).also { transportServer = it }.use { server ->
                    while (!server.isClosed) {
                        val client = server.accept()
                        runCatching { client.tcpNoDelay = true }
                        // Per-connection writer is owned by the camera loop;
                        // a slow consumer gets the latest frame and catches
                        // up (bounded-latest, R5 rule).
                        TransportHub.attach(client)
                    }
                }
            } catch (error: Exception) {
                Log.w("PeriscopeLens", "transport stopped: ${error.message}")
            } finally {
                transportServer = null
                transportThread = null
            }
        }
    }

    private companion object {
        const val CAMERA_REQUEST = 1
    }
}
