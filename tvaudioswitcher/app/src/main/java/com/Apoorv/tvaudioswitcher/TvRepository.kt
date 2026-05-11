package com.Apoorv.tvaudioswitcher

import android.content.Context
import android.net.wifi.WifiManager
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.withContext
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.net.DatagramPacket
import java.net.InetAddress
import java.net.MulticastSocket
import java.net.URL

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
            FileLogger.log("DATASTORE", "Saved ${tvs.size} TVs")
        }
    }

    suspend fun updateTvClientKey(ip: String, newKey: String, newName: String? = null) {
        val currentTvs = savedTvs.first()
        val updatedList = currentTvs.map {
            if (it.ip == ip) {
                it.copy(
                    clientKey = newKey,
                    isPaired = true,
                    name = newName ?: it.name
                )
            } else it
        }
        saveTvs(updatedList)
    }

    suspend fun updateTv(updatedTv: SmartTv) {
        val currentTvs = savedTvs.first()
        val newList = currentTvs.map { if (it.ip == updatedTv.ip) updatedTv else it }
        saveTvs(newList)
    }

    // Discover TVs via SSDP (async)
    suspend fun discoverTvs(): List<SmartTv> = withContext(Dispatchers.IO) {
        val discoveredMap = mutableMapOf<String, SmartTv>()

        val wifiManager =
            context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        val lock = wifiManager.createMulticastLock("SSDP_LOCK")

        try {
            lock.acquire()
            val socket = MulticastSocket()
            socket.soTimeout = 4000

            val group = InetAddress.getByName("239.255.255.250")

            val searchMessage = """
                M-SEARCH * HTTP/1.1
                HOST: 239.255.255.250:1900
                MAN: "ssdp:discover"
                MX: 2
                ST: urn:schemas-upnp-org:device:MediaRenderer:1
                USER-AGENT: Android/UPnP/1.0 MyApp/1.0
            """.trimIndent().replace("\n", "\r\n") + "\r\n\r\n"

            val packet = DatagramPacket(
                searchMessage.toByteArray(),
                searchMessage.length,
                group,
                1900
            )

            socket.send(packet)
            FileLogger.log("SSDP", "Discovery packet sent (MediaRenderer)")

            try {
                while (true) {
                    val buf = ByteArray(2048)
                    val receivePacket = DatagramPacket(buf, buf.size)
                    socket.receive(receivePacket)
                    val response = String(receivePacket.data, 0, receivePacket.length)
                    val ip = receivePacket.address.hostAddress ?: continue

                    val location = response.lineValue("LOCATION")

                    if (!discoveredMap.containsKey(ip)) {
                        val detailedName =
                            if (location != null) fetchDetailedName(location) else null
                        val name = detailedName ?: "Smart TV"

                        discoveredMap[ip] = SmartTv(name, ip)
                        FileLogger.log("SSDP", "Found TV: $name at $ip")
                    }
                }
            } catch (e: Exception) {
                FileLogger.log("SSDP", "Listening finished: ${e.message}")
            } finally {
                socket.close()
            }
        } catch (e: Exception) {
            FileLogger.log("SSDP", "Discovery error: ${e.message}")
        } finally {
            if (lock.isHeld) lock.release()
        }
        discoveredMap.values.toList()
    }

    private fun fetchDetailedName(url: String): String? {
        return try {
            val connection = URL(url).openConnection()
            connection.connectTimeout = 1500
            connection.readTimeout = 1500
            val xml = connection.getInputStream().bufferedReader().use { it.readText() }

            FileLogger.log("SSDP", "Raw device.xml from $url:\n$xml")

            val manufacturer = xml.substringBetween("<manufacturer>", "</manufacturer>")
            val friendlyName = xml.substringBetween("<friendlyName>", "</friendlyName>")

            val cleanManufacturer = when {
                manufacturer?.contains("LG", ignoreCase = true) == true -> "LG"
                manufacturer?.contains("Samsung", ignoreCase = true) == true -> "Samsung"
                manufacturer?.contains("Sony", ignoreCase = true) == true -> "Sony"
                else -> manufacturer?.split(" ")?.firstOrNull() ?: "Smart"
            }

            val modelPart = when {
                // For LG, the model is often at the end of the friendlyName
                cleanManufacturer == "LG" && friendlyName != null -> {
                    // Extract after "TV " to get the full model string (e.g., 43NANO83A6A)
                    friendlyName.substringAfter("TV ").trim()
                }

                else -> xml.substringBetween("<modelNumber>", "</modelNumber>")
                    ?: xml.substringBetween("<modelName>", "</modelName>")
            }

            if (modelPart != null && modelPart != "1.0") {
                "${cleanManufacturer}_${modelPart}".replace(" ", "_")
            } else if (friendlyName != null) {
                "${cleanManufacturer}_${friendlyName.replace(" ", "_")}"
            } else {
                null
            }
        } catch (e: Exception) {
            FileLogger.log("SSDP", "Failed to fetch XML from $url: ${e.message}")
            null
        }
    }

    private fun String.substringBetween(start: String, end: String): String? {
        val startIndex = indexOf(start)
        if (startIndex == -1) return null
        val endIndex = indexOf(end, startIndex + start.length)
        if (endIndex == -1) return null
        return substring(startIndex + start.length, endIndex).trim()
    }

    private fun String.lineValue(key: String): String? =
        lines().find { it.startsWith("$key:", ignoreCase = true) }?.let {
            it.substringAfter(":").trim()
        }
}
