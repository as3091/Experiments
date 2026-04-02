package com.Apoorv.tvaudioswitcher

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.withContext
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import android.util.Log
import kotlinx.coroutines.flow.first

// DataStore extension
val Context.tvDataStore: DataStore<Preferences> by preferencesDataStore(name = "tvs")

class TvRepository(private val context: Context) {

    // Keys
    private val TVS_KEY = stringPreferencesKey("saved_tvs")

    // Load saved TVs
    val savedTvs: Flow<List<SmartTv>> = context.tvDataStore.data.map { prefs ->
        val json = prefs[TVS_KEY] ?: "[]"
        Json.decodeFromString(json)
    }

    // Save TVs
    suspend fun saveTvs(tvs: List<SmartTv>) {
        context.tvDataStore.edit { prefs ->
            val json = Json.encodeToString(tvs)
            prefs[TVS_KEY] = json
            Log.d("DATASTORE", "Saved ${tvs.size} TVs: $json")
        }
    }
    suspend fun updateTvClientKey(ip: String, newKey: String) {
        val currentTvs = savedTvs.first()
        val updatedList = currentTvs.map {
            if (it.ip == ip) it.copy(clientKey = newKey, isPaired = true) else it
        }
        saveTvs(updatedList)
    }
    suspend fun updateTv(updatedTv: SmartTv) {
        val currentTvs = savedTvs.first()  // Get current list (blocking for simplicity; use flow in prod)
        val newList = currentTvs.map { if (it.ip == updatedTv.ip) updatedTv else it }
        saveTvs(newList)
    }
    // Discover TVs via SSDP (async)
    suspend fun discoverTvs(): List<SmartTv> = withContext(Dispatchers.IO) {
        val discovered = mutableListOf<SmartTv>()
        val socket = DatagramSocket()
        socket.broadcast = true
        socket.soTimeout = 5000  // 5 sec timeout

        val searchMessage = """
            M-SEARCH * HTTP/1.1
            HOST: 239.255.255.250:1900
            MAN: "ssdp:discover"
            MX: 3
            ST: upnp:rootdevice  // Broad search; narrow to urn:schemas-upnp-org:device:MediaRenderer:1 for TVs
            USER-AGENT: Android/UPnP/1.0 MyApp/1.0
        """.trimIndent().replace("\n", "\r\n") + "\r\n\r\n"

        val packet = DatagramPacket(
            searchMessage.toByteArray(),
            searchMessage.length,
            InetAddress.getByName("239.255.255.250"),
            1900
        )
        socket.send(packet)
        Log.d("SSDP", "Discovery packet sent!")  // Confirm send

        // Listen for responses
        try {
            while (true) {
                val receivePacket = DatagramPacket(ByteArray(1024), 1024)
                socket.receive(receivePacket)
                val response = String(receivePacket.data, 0, receivePacket.length)
                Log.d("SSDP", "Received response: $response")  // Key debug line

                // Parse response for TV-like devices
                val location = response.line("LOCATION") ?: continue
                val server = response.line("SERVER") ?: continue
                val ip = extractIpFromLocation(location) ?: continue

                // Filter for TVs (customize based on brand; e.g., contains "Samsung" or "LG")
                if (server.contains("TV", ignoreCase = true) || server.contains("MediaRenderer") || server.contains("Samsung") || server.contains("LG") || server.contains("Sony")) {
                    val name = extractNameFromResponse(response) ?: "Unknown TV"
                    discovered.add(SmartTv(name, ip))
                    Log.d("SSDP", "Found TV: $name at $ip")  // Success log
                } else {
                    Log.d("SSDP", "Ignored non-TV device: $server")  // Why ignored
                }
            }
        } catch (e: Exception) {
            Log.e("SSDP", "Discovery error: ${e.message}")  // Catch timeouts/errors
        } finally {
            socket.close()
        }
        discovered.distinctBy { it.ip }  // Dedupe
    }

    // Helpers
    private fun String.line(key: String): String? =
        lines().find { it.startsWith("$key:", ignoreCase = true) }?.let {
            it.substringAfter(":").trim()
        }

    private fun extractIpFromLocation(location: String): String? {
        return Regex("http://([\\d.]+):?\\d*/").find(location)?.groupValues?.get(1)
    }

    private fun extractNameFromResponse(response: String): String? {
        return response.line("USN")?.substringAfter("::")?.trim()  // Or use "friendlyName" if in full XML
    }
}