package dev.periscope.lens

import java.io.IOException
import java.net.Socket
import java.util.concurrent.ConcurrentHashMap

/**
 * Connection registry for the L6 transport: one attached viewer per socket,
 * latest-frame semantics (a viewer that cannot keep up skips frames and
 * catches up on the next one — the bounded-latest rule from the relay, R5).
 *
 * Pure JVM, no Android imports, so the same semantics can be unit-tested
 * when a JVM test lane lands.
 */
object TransportHub {
    private val viewers = ConcurrentHashMap<String, Socket>()

    fun attach(socket: Socket) {
        val key = socket.remoteSocketAddress.toString()
        // A reconnecting viewer replaces its stale socket; close the old one.
        viewers.put(key, socket)?.let { previous -> runCatching { previous.close() } }
    }

    fun detach(socket: Socket) {
        viewers.remove(socket.remoteSocketAddress.toString())
    }

    /** Closes every connection (called when the activity leaves the foreground). */
    fun detachAll() {
        viewers.values.forEach { socket -> runCatching { socket.close() } }
        viewers.clear()
    }

    fun broadcast(frame: ByteArray) {
        viewers.values.removeIf { socket ->
            try {
                val output = socket.getOutputStream()
                output.write(frame)
                output.flush()
                false
            } catch (_: IOException) {
                runCatching { socket.close() }
                true // dead connection: detach
            }
        }
    }

    fun viewerCount(): Int = viewers.size
}
