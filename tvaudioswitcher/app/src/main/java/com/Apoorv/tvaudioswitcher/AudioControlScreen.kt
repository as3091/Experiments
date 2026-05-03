package com.Apoorv.tvaudioswitcher

import android.util.Log
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AudioControlScreen(ip: String) {
    val context = LocalContext.current
    val repository = remember { TvRepository(context) }
    val coroutineScope = rememberCoroutineScope()

    var currentOutputId by remember { mutableStateOf("Loading...") }
    var availableModes by remember { mutableStateOf<List<Pair<String, String>>>(emptyList()) }
    var isLoading by remember { mutableStateOf(true) }
    var error by remember { mutableStateOf("") }

    val savedTvs by repository.savedTvs.collectAsState(initial = emptyList())
    val tv = savedTvs.find { it.ip == ip }

    LaunchedEffect(ip) {
        val saved = repository.savedTvs.first()
        val tvFound = saved.find { it.ip == ip }
        if (tvFound == null) {
            error = "TV not found in saved list"
            isLoading = false
            return@LaunchedEffect
        }

        isLoading = true
        error = ""

        try {
            // 1. Get Current Output
            val currentResp = WebSocketManager.sendCommand(
                ip,
                tvFound.clientKey ?: "",
                "ssap://com.webos.service.apiadapter/audio/getSoundOutput"
            )
            currentOutputId = currentResp
                ?.optJSONObject("payload")
                ?.optString("soundOutput")
                ?: "Unknown"

            // 2. Get Available Output List (Dynamic API)
            val listResp = WebSocketManager.sendCommand(
                ip,
                tvFound.clientKey ?: "",
                "ssap://com.webos.service.apiadapter/audio/getSoundOutputList"
            )
            Log.d("TV_DEBUG", "getSoundOutputList raw response: $listResp")

            val soundList = listResp?.optJSONObject("payload")?.optJSONArray("soundOutputList")
            val modes = mutableListOf<Pair<String, String>>()

            if (soundList != null && soundList.length() > 0) {
                for (i in 0 until soundList.length()) {
                    val modeId = soundList.optString(i)
                    modes.add(modeId to mapModeIdToLabel(modeId))
                }
            } else {
                Log.w(
                    "TV_DEBUG",
                    "Dynamic list empty or failed (Response: $listResp), using robust fallback list"
                )
                // Fallback to standard LG modes if the dynamic API fails
                listOf(
                    "tv_speaker",
                    "external_arc",
                    "external_optical",
                    "tv_speaker_arc",
                    "bt_soundbar",
                    "tv_speaker_bt",
                    "tv_external_speaker",
                    "headphone"
                ).forEach { modes.add(it to mapModeIdToLabel(it)) }
            }
            availableModes = modes
        } catch (e: Exception) {
            error = "Error: ${e.message}"
        }

        isLoading = false
    }

    Scaffold(topBar = { TopAppBar(title = { Text(tv?.name ?: "Audio Control") }) }) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .padding(24.dp)
                .fillMaxSize(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            // Helper to get label for current output
            val currentLabel = availableModes.find { it.first == currentOutputId }?.second
                ?: if (isLoading) "Detecting..." else "Unknown ($currentOutputId)"

            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 32.dp),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant
                )
            ) {
                Column(
                    modifier = Modifier.padding(24.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        text = "Current Audio Output",
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        text = currentLabel,
                        style = MaterialTheme.typography.headlineMedium,
                        color = MaterialTheme.colorScheme.primary
                    )
                }
            }

            if (error.isNotEmpty()) {
                Text(
                    text = error,
                    color = MaterialTheme.colorScheme.error,
                    modifier = Modifier.padding(bottom = 16.dp),
                    style = MaterialTheme.typography.bodyMedium
                )
            } else if (!isLoading && currentOutputId != "Loading...") {
                Text(
                    text = "Switch successful",
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.padding(bottom = 16.dp),
                    style = MaterialTheme.typography.bodyMedium
                )
            }

            Text(
                text = "Available Modes",
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.padding(bottom = 16.dp)
            )

            availableModes.forEach { (modeId, label) ->
                Button(
                    onClick = {
                        coroutineScope.launch {
                            error = ""
                            var success = false

                            // 1. Try audio/setSystemSettings (Specific LG audio endpoint)
                            Log.d("TV_DEBUG", "Trying audio/setSystemSettings for $label")
                            val response1 = WebSocketManager.sendCommand(
                                ip, tv?.clientKey ?: "",
                                "ssap://com.webos.service.apiadapter/audio/setSystemSettings",
                                mapOf(
                                    "category" to "sound",
                                    "settings" to mapOf("soundOutput" to modeId)
                                )
                            )
                            success = response1?.optJSONObject("payload")?.optBoolean("returnValue")
                                ?: false

                            // 2. Fallback to system/setSystemSettings
                            if (!success) {
                                Log.d("TV_DEBUG", "Fallback: Trying system/setSystemSettings")
                                val response2 = WebSocketManager.sendCommand(
                                    ip, tv?.clientKey ?: "",
                                    "ssap://com.webos.service.apiadapter/system/setSystemSettings",
                                    mapOf(
                                        "category" to "sound",
                                        "settings" to mapOf("soundOutput" to modeId)
                                    )
                                )
                                success =
                                    response2?.optJSONObject("payload")?.optBoolean("returnValue")
                                        ?: false
                            }

                            // 3. Final fallback to legacy changeSoundOutput
                            if (!success) {
                                Log.d("TV_DEBUG", "Fallback: Trying legacy changeSoundOutput")
                                val response3 = WebSocketManager.sendCommand(
                                    ip, tv?.clientKey ?: "",
                                    "ssap://com.webos.service.apiadapter/audio/changeSoundOutput",
                                    mapOf("output" to modeId)
                                )
                                success =
                                    response3?.optJSONObject("payload")?.optBoolean("returnValue")
                                        ?: false
                            }

                            if (success) {
                                currentOutputId = modeId
                                WebSocketManager.showToast(
                                    ip,
                                    tv?.clientKey ?: "",
                                    "Audio: $label Enabled"
                                )
                            } else {
                                error = "Failed to switch to $label"
                            }
                        }
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    enabled = !isLoading && tv != null,
                    colors = if (currentOutputId == modeId)
                        ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary)
                    else
                        ButtonDefaults.buttonColors()
                ) {
                    Text(label)
                }
            }
        }
    }
}

private fun mapModeIdToLabel(modeId: String): String {
    return when (modeId) {
        "tv_speaker" -> "Internal Speakers"
        "external_arc" -> "HDMI ARC"
        "tv_speaker_arc" -> "WOW Orchestra"
        "bt_soundbar" -> "Bluetooth Soundbar"
        "tv_speaker_bt" -> "TV + Bluetooth"
        "external_optical" -> "Optical"
        "tv_external_speaker" -> "TV + Optical"
        "headphone" -> "Wired Headphones"
        else -> modeId.replace("_", " ").replaceFirstChar { it.uppercase() }
    }
}
