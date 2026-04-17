package com.Apoorv.tvaudioswitcher

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

        val currentResp = WebSocketManager.sendCommand(
            ip, tvFound.clientKey ?: "", "ssap://com.webos.service.apiadapter/audio/getSoundOutput"
        )
        currentOutputId = currentResp
            ?.optJSONObject("payload")
            ?.optString("soundOutput")
            ?: "Unknown"

        isLoading = false
    }

    Scaffold(topBar = { TopAppBar(title = { Text("Audio Control") }) }) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .padding(24.dp)
                .fillMaxSize(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            val currentMode = SoundMode.fromId(currentOutputId)

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
                        text = if (isLoading) "Detecting..." else currentMode.label,
                        style = MaterialTheme.typography.headlineMedium,
                        color = MaterialTheme.colorScheme.primary
                    )
                }
            }

            if (error.isNotEmpty()) {
                Text(
                    text = error,
                    color = MaterialTheme.colorScheme.error,
                    modifier = Modifier.padding(bottom = 16.dp)
                )
            }

            Text(
                text = "Available Modes",
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.padding(bottom = 16.dp)
            )

            SoundMode.entries.forEach { mode ->
                Button(
                    onClick = {
                        coroutineScope.launch {
                            val response = WebSocketManager.sendCommand(
                                ip, tv?.clientKey ?: "",
                                "ssap://com.webos.service.apiadapter/audio/changeSoundOutput",
                                mapOf("output" to mode.id)
                            )
                            if (response != null) {
                                currentOutputId = mode.id
                                WebSocketManager.showToast(
                                    ip,
                                    tv?.clientKey ?: "",
                                    "Audio: ${mode.label} Enabled"
                                )
                            } else {
                                error = "Failed to switch to ${mode.label}"
                            }
                        }
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    enabled = !isLoading && tv != null,
                    colors = if (currentOutputId == mode.id)
                        ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary)
                    else
                        ButtonDefaults.buttonColors()
                ) {
                    Text(mode.label)
                }
            }
        }
    }
}
