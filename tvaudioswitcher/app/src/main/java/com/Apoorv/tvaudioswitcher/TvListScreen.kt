package com.Apoorv.tvaudioswitcher  // Your package

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import android.util.Log
import kotlinx.coroutines.flow.first
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
            ip = "192.168.0.122",
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
                modifier = Modifier.padding(padding).fillMaxSize(),
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