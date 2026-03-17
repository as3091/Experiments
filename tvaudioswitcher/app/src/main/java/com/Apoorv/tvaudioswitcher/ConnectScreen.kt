package com.Apoorv.tvaudioswitcher

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.ui.unit.dp
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConnectScreen(ip: String, onConnected: () -> Unit) {
    val coroutineScope = rememberCoroutineScope()
    var isConnecting by remember { mutableStateOf(false) }
    var isPaired by remember { mutableStateOf(false) }  // Mock for now

    Scaffold(topBar = { TopAppBar(title = { Text("Connect to TV") }) }) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Text("TV IP: $ip")
            Spacer(Modifier.height(16.dp))

            if (isConnecting) {
                CircularProgressIndicator()
                Text("Connecting...")
            } else if (!isPaired) {
                Button(onClick = {
                    isConnecting = true
                    coroutineScope.launch {
                        delay(3000)  // Fake delay
                        isConnecting = false
                        isPaired = true
                        onConnected()  // Navigate to next screen
                    }
                }) {
                    Text("Pair Now")
                }
            } else {
                Text("Already Paired!")
                Button(onClick = onConnected) {
                    Text("Continue")
                }
            }
        }
    }
}