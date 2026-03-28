package com.Apoorv.tvaudioswitcher

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.material3.ExperimentalMaterial3Api
import android.util.Log
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.platform.LocalContext
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.first

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AudioControlScreen(ip: String) {
    val context = LocalContext.current
    val repository = remember { TvRepository(context) }
    val coroutineScope = rememberCoroutineScope()

    var currentOutput by remember { mutableStateOf("Loading...") }
    var outputs by remember { mutableStateOf(listOf<String>()) }
    var isLoading by remember { mutableStateOf(true) }
    var error by remember { mutableStateOf("") }

    // Load saved TVs and find the one matching IP
    val savedTvs by repository.savedTvs.collectAsState(initial = emptyList())
    val tv = savedTvs.find { it.ip == ip }

    LaunchedEffect(ip) {
        val saved = repository.savedTvs.first()
        Log.d("AUDIO_DEBUG", "Saved TVs count: ${saved.size}")
        saved.forEach { Log.d("AUDIO_DEBUG", "Saved TV: ${it.name} - ${it.ip} - Paired: ${it.isPaired} - Key: ${it.clientKey}") }

        val tv = saved.find { it.ip == ip }
        if (tv == null) {
            error = "TV not found in saved list (IP: $ip)"
            isLoading = false
            return@LaunchedEffect
        }

        isLoading = true
        error = ""

        // Get current output
        val currentResp = WebSocketManager.sendCommand(
            ip, tv.clientKey ?: "", "ssap://com.webos.service.apiadapter/audio/getSoundOutput"
        )
        currentOutput = currentResp
            ?.optJSONObject("payload")
            ?.optString("soundOutput")
            ?: "Unknown"

        // Get available audio devices
        val devicesResp = WebSocketManager.sendCommand(
            ip, tv.clientKey ?: "", "ssap://com.webos.service.audiooutput/getDeviceList"
        )
        if (devicesResp != null) {
            val deviceList = devicesResp.optJSONObject("payload")?.optJSONArray("devices")
            outputs = (0 until (deviceList?.length() ?: 0)).mapNotNull { index ->
                deviceList?.getJSONObject(index)?.optString("id")
            }.filterNotNull()
        } else {
            error = "Failed to load audio devices"
        }

        isLoading = false
    }

    Scaffold(topBar = { TopAppBar(title = { Text("Audio Outputs - $ip") }) }) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .padding(16.dp)
                .fillMaxSize()
        ) {
            Text(
                text = "Current: $currentOutput",
                style = MaterialTheme.typography.titleMedium
            )
            Spacer(modifier = Modifier.height(16.dp))

            when {
                isLoading -> {
                    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        CircularProgressIndicator()
                    }
                }
                error.isNotEmpty() -> {
                    Text(error, color = MaterialTheme.colorScheme.error)
                }
                outputs.isEmpty() -> {
                    Text("No audio outputs found")
                }
                else -> {
                    LazyVerticalGrid(
                        columns = GridCells.Fixed(2),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        items(outputs) { outputId ->
                            Button(
                                onClick = {
                                    coroutineScope.launch {
                                        val payload = mapOf("output" to outputId)
                                        val response = WebSocketManager.sendCommand(
                                            ip, tv?.clientKey ?: "",
                                            "ssap://com.webos.service.apiadapter/audio/changeSoundOutput",
                                            payload
                                        )
                                        // Optional: refresh current output after switch
                                        delay(1500)
                                        // You can re-fetch getSoundOutput here if needed
                                        Log.d("AudioSwitch", "Switched to $outputId on $ip")
                                    }
                                },
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Text(
                                    text = outputId.replace("_", " ").replaceFirstChar { it.uppercase() }
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}
private fun switchAudioOutput(output: String, tvIp: String) {
    // TODO: real HTTP call here
//    Log.d("TV", "Switched to $output on $tvIp")
    android.util.Log.d("AudioSwitch", "Switched to $output on TV $tvIp")
    // show Snackbar "Switched to $output"
}
