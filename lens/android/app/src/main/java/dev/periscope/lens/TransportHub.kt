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
        viewers[socket.remoteSocketAddress.toString()] = socket
    }

    fun detach(socket: Socket) {
        viewers.remove(socket.remoteSocketAddress.toString())
    }

    fun broadcast(frame: ByteArray) {
        viewers.values.removeIf { socket ->
            try {
                val output = socket.getOutputStream()
                output.write(frame)
                output.flush()
                false
            } catch (_: IOException) {
                true // dead connection: detach
            }
        }
    }

    fun viewerCount(): Int = viewers.size
}
