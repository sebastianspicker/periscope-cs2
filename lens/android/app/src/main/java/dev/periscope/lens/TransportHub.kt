package dev.periscope.lens

import java.io.IOException
import java.net.Socket
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.ConcurrentHashMap

/**
 * Connection registry for the L6 transport: one attached viewer per socket
 * host, latest-frame semantics (a viewer that cannot keep up skips frames and
 * catches up on the next one, the bounded-latest rule from the relay, R5).
 *
 * Each viewer has a size-1 queue and its own writer thread, so one stalled
 * peer cannot block the camera loop or the other viewers.
 *
 * Pure JVM, no Android imports, so the same semantics can be unit-tested
 * when a JVM test lane lands.
 */
object TransportHub {
    private class Viewer(val socket: Socket) {
        private val pending = ArrayBlockingQueue<ByteArray>(1)
        @Volatile private var running = true
        private val thread = Thread {
            try {
                val output = socket.getOutputStream()
                while (running) {
                    val frame = try {
                        pending.take()
                    } catch (_: InterruptedException) {
                        break
                    }
                    output.write(frame)
                    output.flush()
                }
            } catch (_: IOException) {
                // peer gone; detach(socket) is idempotent.
            } finally {
                runCatching { socket.close() }
            }
        }.also { it.isDaemon = true; it.start() }

        fun offer(frame: ByteArray) {
            // Keep only the newest frame.
            while (pending.isNotEmpty()) pending.poll()
            pending.offer(frame)
        }

        fun close() {
            running = false
            thread.interrupt()
            runCatching { socket.close() }
        }
    }

    private val viewers = ConcurrentHashMap<String, Viewer>()

    private fun key(socket: Socket): String =
        socket.inetAddress?.hostAddress ?: socket.remoteSocketAddress.toString()

    fun attach(socket: Socket) {
        // A reconnect from the same host replaces the stale socket.
        viewers.put(key(socket), Viewer(socket))?.close()
    }

    fun detach(socket: Socket) {
        viewers.remove(key(socket))?.close()
    }

    /** Closes every connection (called when the activity leaves the foreground). */
    fun detachAll() {
        viewers.values.forEach { it.close() }
        viewers.clear()
    }

    fun broadcast(frame: ByteArray) {
        viewers.values.forEach { it.offer(frame) }
    }

    fun viewerCount(): Int = viewers.size
}
