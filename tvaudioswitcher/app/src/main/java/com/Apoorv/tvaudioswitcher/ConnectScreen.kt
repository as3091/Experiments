package com.Apoorv.tvaudioswitcher

import android.util.Log
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.*
import org.json.JSONObject
import java.security.SecureRandom
import java.security.cert.X509Certificate
import javax.net.ssl.*
import java.util.concurrent.TimeUnit

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConnectScreen(ip: String, onConnected: () -> Unit) {
    val context = LocalContext.current
    val repository = remember { TvRepository(context) }
    val coroutineScope = rememberCoroutineScope()

    // We'll load the real TV object from repository to get stored clientKey
    var tv by remember { mutableStateOf<SmartTv?>(null) }
    var isConnecting by remember { mutableStateOf(false) }
    var errorMessage by remember { mutableStateOf("") }

    // Load TV data once
    LaunchedEffect(ip) {
        // Find the TV by IP (you may need to adjust how you query savedTvs)
        repository.savedTvs.collect { tvList ->
            tv = tvList.find { it.ip == ip }
        }
    }

    Scaffold(topBar = { TopAppBar(title = { Text("Connect to TV") }) }) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Text("TV IP: $ip", style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(24.dp))

            if (isConnecting) {
                CircularProgressIndicator()
                Spacer(Modifier.height(16.dp))
                Text("Connecting & Pairing...")
            } else if (errorMessage.isNotEmpty()) {
                Text(errorMessage, color = MaterialTheme.colorScheme.error)
                Spacer(Modifier.height(16.dp))
            } else if (tv?.isPaired == true) {
                Text("Already Paired!")
                Spacer(Modifier.height(24.dp))
                Button(onClick = onConnected) {
                    Text("Continue to Audio Controls")
                }
            } else {
                Text("This TV needs pairing.")
                Spacer(Modifier.height(16.dp))
                Text("On first pair, accept the prompt on your TV screen.", style = MaterialTheme.typography.bodySmall)
                Spacer(Modifier.height(24.dp))
                Button(onClick = {
                    isConnecting = true
                    errorMessage = ""
                    coroutineScope.launch {
                        val result = pairWithTv(ip, tv?.clientKey ?: "", repository)
                        isConnecting = false
                        if (result.success) {
                            // Key is already saved inside pairWithTv
                            onConnected()
                        } else {
                            errorMessage = "Pairing failed. Check Logcat (WS_PAIR) or accept TV prompt."
                        }
                    }
                }) {
                    Text("Pair Now")
                }
            }
        }
    }
}
data class PairingResult(
    val success: Boolean,
    val newClientKey: String? = null
)
// Secure WS pairing matching your LG WebOS Python code
// Secure WS pairing matching your LG WebOS Python code
private suspend fun pairWithTv(
    ip: String,
    existingClientKey: String,
    repository: TvRepository
): PairingResult = withContext(Dispatchers.IO) {
    var success = false
    var receivedClientKey: String? = null

    val trustAllCerts = arrayOf<TrustManager>(object : X509TrustManager {
        override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {}
        override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {}
        override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
    })

    val sslContext = SSLContext.getInstance("TLS").apply {
        init(null, trustAllCerts, SecureRandom())
    }

    val client = OkHttpClient.Builder()
        .sslSocketFactory(sslContext.socketFactory, trustAllCerts[0] as X509TrustManager)
        .hostnameVerifier { _, _ -> true }
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()

    val request = Request.Builder()
        .url("wss://$ip:3001/")
        .build()

    val latch = java.util.concurrent.CountDownLatch(1)

    val listener = object : WebSocketListener() {
        override fun onOpen(webSocket: WebSocket, response: Response) {
            Log.d("WS_PAIR", "Connection opened: ${response.code}")

            val registerPayload = """
            {
                "type": "register",
                "id": "register_0",
                "payload": {
                    "forcePairing": false,
                    "pairingType": "PROMPT",
                    "client-key": "$existingClientKey",
                    "manifest": {
                        "manifestVersion": 1,
                        "permissions": [
                            "LAUNCH", "LAUNCH_WEBAPP", "APP_TO_APP", "CLOSE", "TEST_OPEN", "TEST_PROTECTED",
                            "CONTROL_AUDIO", "CONTROL_DISPLAY", "CONTROL_INPUT_JOYSTICK",
                            "CONTROL_INPUT_MEDIA_RECORDING", "CONTROL_INPUT_MEDIA_PLAYBACK", "CONTROL_INPUT_TV",
                            "CONTROL_POWER", "READ_APP_STATUS", "READ_CURRENT_CHANNEL", "READ_INPUT_DEVICE_LIST",
                            "READ_NETWORK_STATE", "READ_RUNNING_APPS", "READ_TV_CHANNEL_LIST", "WRITE_NOTIFICATION_TOAST",
                            "READ_POWER_STATE", "READ_COUNTRY_INFO", "READ_SETTINGS", "CONTROL_MOUSE_AND_KEYBOARD",
                            "CONTROL_INPUT_TEXT", "CONTROL_USER_INFO", "READ_UPDATE_INFO",
                            "UPDATE_FROM_REMOTE_APP", "READ_LGE_TV_INPUT_EVENTS", "READ_TV_CURRENT_TIME",
                            "CONTROL_AUDIO_OUTPUT", "READ_AUDIO_DEVICES"
                        ],
                        "signatures": [{"signatureVersion": 1}],
                        "appVersion": "1.0"
                    }
                }
            }
            """.trimIndent()

            webSocket.send(registerPayload)
            Log.d("WS_PAIR", "Sent registration payload")
        }

        override fun onMessage(webSocket: WebSocket, text: String) {
            Log.d("WS_PAIR", "Received: $text")
            try {
                val json = JSONObject(text)
                val type = json.optString("type")

                if (type == "registered") {
                    Log.d("WS_PAIR", "Registration successful!")
                    success = true
                    val newKey = json.optJSONObject("payload")?.optString("client-key")
                    if (!newKey.isNullOrEmpty() && newKey != existingClientKey) {
                        receivedClientKey = newKey
                        Log.d("WS_PAIR", "New client key received: $newKey")
                    }
                    webSocket.close(1000, "Registration complete")
                    latch.countDown()
                } else if (type == "error") {
                    Log.e("WS_PAIR", "TV error: ${json.optString("error")}")
                    webSocket.close(1000, "Error")
                    latch.countDown()
                }
            } catch (e: Exception) {
                Log.e("WS_PAIR", "Parse error", e)
            }
        }

        override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
            Log.e("WS_PAIR", "Failure: ${t.message}", t)
            success = false
            latch.countDown()
        }

        override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
            Log.d("WS_PAIR", "Closing: $code - $reason")
            latch.countDown()
        }
    }

    client.newWebSocket(request, listener)

    // Block until latch is released (success, error, close, or timeout)
    latch.await(30, TimeUnit.SECONDS)

    // Save new key if received (on main thread)
    if (success && receivedClientKey != null) {
        withContext(Dispatchers.Main) {
            repository.updateTvClientKey(ip, receivedClientKey)
        }
    }

    // Final return statement — this is allowed here (function scope)
    return@withContext PairingResult(success, receivedClientKey)
}