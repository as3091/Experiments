package com.Apoorv.tvaudioswitcher  // Your package

import android.util.Log
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
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
fun TvListScreen(onTvSelected: (SmartTv) -> Unit) {
    val context = LocalContext.current
    val repository = remember { TvRepository(context) }
    val coroutineScope = rememberCoroutineScope()

    // Load saved TVs as state
    val savedTvs = repository.savedTvs.collectAsState(initial = emptyList()).value
    var isDiscovering by remember { mutableStateOf(false) }
    var discoveredTvs by remember { mutableStateOf(emptyList<SmartTv>()) }

    // Manual add dialog states
    var showAddDialog by remember { mutableStateOf(false) }
    var manualName by remember { mutableStateOf("") }
    var manualIp by remember { mutableStateOf("") }

    // Auto-discover on launch + hardcode the IP for emulator testing
    LaunchedEffect(Unit) {
        isDiscovering = true

        // Force add hardcoded TV every time for testing
        val hardcodedTv = SmartTv(
            name = "Hardcoded LG TV",
            ip = "192.168.29.232",
            isPaired = false, // or true if you already paired
            clientKey = null // or your stored key
        )

        // Get current saved TVs
        val currentTvs = repository.savedTvs.first() // blocking get for simplicity

        // Add if not already present
        if (currentTvs.none { it.ip == hardcodedTv.ip }) {
            val updated = (currentTvs + hardcodedTv).distinctBy { it.ip }
            repository.saveTvs(updated)
            Log.d("TV_DEBUG", "Hardcoded TV added/saved: $hardcodedTv")
        } else {
            Log.d("TV_DEBUG", "Hardcoded TV already exists")
        }

        // Optional: still run discovery
        val newTvs = repository.discoverTvs()
        Log.d(
            "TV_DEBUG",
            "Discovered TVs: ${
                newTvs.joinToString(
                    prefix = "[",
                    postfix = "]"
                ) { "${it.name} (${it.ip})" }
            }"
        )
        discoveredTvs = newTvs
        val allTvs = (currentTvs + newTvs + listOf(hardcodedTv)).distinctBy { it.ip }
        repository.saveTvs(allTvs)

        isDiscovering = false
    }

    Scaffold(
        topBar = { TopAppBar(title = { Text("Available TVs") }) },
        floatingActionButton = {
            FloatingActionButton(onClick = { showAddDialog = true }) {  // Show manual add dialog
                Icon(Icons.Filled.Add, contentDescription = "Add TV")
            }
        }
    ) { padding ->
        if (isDiscovering) {
            Box(Modifier.fillMaxSize(), Alignment.Center) {
                CircularProgressIndicator()
            }
        } else if (savedTvs.isEmpty()) {
            Box(Modifier.fillMaxSize(), Alignment.Center) {
                Text("No TVs found. Tap + to add manually.")
            }
        } else {
            LazyColumn(
                modifier = Modifier
                    .padding(padding)
                    .fillMaxSize(),
                contentPadding = PaddingValues(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(savedTvs) { tv ->
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        onClick = { onTvSelected(tv) }
                    ) {
                        Column(modifier = Modifier.padding(16.dp)) {
                            Text(tv.name, style = MaterialTheme.typography.titleMedium)
                            Text("IP: ${tv.ip}")
                            Text(if (tv.isPaired) "Paired" else "Not Paired")
                        }
                    }
                }
            }
        }

        // Manual Add Dialog
        if (showAddDialog) {
            AlertDialog(
                onDismissRequest = { showAddDialog = false },
                title = { Text("Add TV Manually") },
                text = {
                    Column {
                        TextField(
                            value = manualName,
                            onValueChange = { manualName = it },
                            label = { Text("TV Name") }
                        )
                        Spacer(Modifier.height(8.dp))
                        TextField(
                            value = manualIp,
                            onValueChange = { manualIp = it },
                            label = { Text("IP Address (e.g., 192.168.1.100)") }
                        )
                    }
                },
                confirmButton = {
                    Button(onClick = {
                        if (manualName.isNotBlank() && manualIp.isNotBlank()) {
                            coroutineScope.launch {
                                val newTv = SmartTv(manualName, manualIp)
                                val allTvs = (savedTvs + newTv).distinctBy { it.ip }
                                repository.saveTvs(allTvs)
                            }
                        }
                        showAddDialog = false
                        manualName = ""
                        manualIp = ""
                    }) {
                        Text("Add")
                    }
                },
                dismissButton = {
                    Button(onClick = { showAddDialog = false }) {
                        Text("Cancel")
                    }
                }
            )
        }
    }
}