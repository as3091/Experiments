package com.Apoorv.tvaudioswitcher

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.material3.ExperimentalMaterial3Api

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AudioControlScreen(ip: String) {
    // Mock audio outputs (fetch from TV API later)
    val outputs = listOf(
        "TV Speakers",
        "HDMI eARC",
        "Optical",
        "Bluetooth Headphones",
        "Soundbar"
    )

    Scaffold(topBar = { TopAppBar(title = { Text("Audio Outputs for $ip") }) }) { padding ->
        LazyVerticalGrid(
            columns = GridCells.Fixed(2),
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            items(outputs.size) { index ->
                val output = outputs[index]
                Button(
                    onClick = { switchAudioOutput(output, ip) },  // Use 'output' and 'ip' here
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(output)
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