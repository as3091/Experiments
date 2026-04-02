package com.Apoorv.tvaudioswitcher

import android.util.Log
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.serialization.json.Json
import okhttp3.*
import org.json.JSONObject
import java.security.SecureRandom
import java.security.cert.X509Certificate
import javax.net.ssl.*
import java.util.concurrent.TimeUnit
object WebSocketManager {
    private var webSocket: WebSocket? = null
    private val responseChannels = mutableMapOf<String, Channel<JSONObject>>()
    private val json = Json { ignoreUnknownKeys = true }

    private val trustAllCerts = arrayOf<TrustManager>(object : X509TrustManager {
        override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {}
        override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {}
        override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
    })

    private val sslContext: SSLContext = SSLContext.getInstance("TLS").apply {
        init(null, trustAllCerts, SecureRandom())
    }

    private val client: OkHttpClient = OkHttpClient.Builder()
        .sslSocketFactory(sslContext.socketFactory, trustAllCerts[0] as X509TrustManager)
        .hostnameVerifier { _, _ -> true }
        .build()

    suspend fun connectOrReuse(ip: String, clientKey: String): Boolean = withContext(Dispatchers.IO) {
        if (webSocket != null) return@withContext true  // Already connected

        val request = Request.Builder()
            .url("wss://$ip:3001/")
            .build()

        val latch = java.util.concurrent.CountDownLatch(1)
        var success = false

        val listener = object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                Log.d("WS_MGR", "Connected to $ip")
                this@WebSocketManager.webSocket = webSocket

                // Send registration
                val payload = """
                {
                    "type": "register",
                    "id": "register_mgr",
                    "payload": {
                        "forcePairing": false,
                        "pairingType": "PROMPT",
                        "client-key": "$clientKey",
                        "manifest": { ... }  // paste your full manifest here
                    }
                }
                """.trimIndent()
                webSocket.send(payload)
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                try {
                    val jsonObj = JSONObject(text)
                    val id = jsonObj.optString("id")
                    val type = jsonObj.optString("type")

                    if (type == "registered") {
                        success = true
                        latch.countDown()
                    } else if (id.isNotEmpty() && responseChannels.containsKey(id)) {
                        responseChannels[id]?.trySend(jsonObj)
                        responseChannels.remove(id)
                    }
                } catch (e: Exception) {
                    Log.e("WS_MGR", "Message parse error", e)
                }
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                Log.e("WS_MGR", "Failure: ${t.message}")
                success = false
                latch.countDown()
                cleanup()
            }

            override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                cleanup()
            }
        }

        client.newWebSocket(request, listener)
        latch.await(15, TimeUnit.SECONDS)
        success
    }

    suspend fun sendCommand(
        ip: String,
        clientKey: String,
        uri: String,
        payload: Map<String, Any> = emptyMap()
    ): JSONObject? = withContext(Dispatchers.IO) {
        if (!connectOrReuse(ip, clientKey)) return@withContext null

        val requestId = "req_${System.currentTimeMillis()}"
        val channel = Channel<JSONObject>(1)
        responseChannels[requestId] = channel

        val command = mapOf(
            "type" to "request",
            "id" to requestId,
            "uri" to uri,
            "payload" to payload
        )

        val jsonStr = JSONObject(command).toString()
        webSocket?.send(jsonStr) ?: return@withContext null

        try {
            withTimeout(10000) {
                channel.receive()
            }
        } catch (e: TimeoutCancellationException) {
            Log.e("WS_MGR", "Command timeout: $uri")
            null
        } finally {
            responseChannels.remove(requestId)
        }
    }

    private fun cleanup() {
        webSocket?.close(1000, "Cleanup")
        webSocket = null
        responseChannels.values.forEach { it.cancel() }
        responseChannels.clear()
    }
}