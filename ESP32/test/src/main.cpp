#include <Arduino.h>
#include <WiFi.h>  // For WiFi functionality
#include "wifi_helper.h"  // For WiFi setup and scanning
#include "button_check_led_state_blink_helper.h"  // For button and LED handling
#include <WebSocketsClient.h>  // From links2004/WebSockets
#include <ArduinoJson.h>      // For JSON

#ifndef LED_PIN
#error LED_PIN is not defined from .env
#endif
#ifndef TV_IP
#error TV_IP is not defined from .env
#endif

#ifndef BUTTON_PIN
#error BUTTON_PIN is not defined from .env
#endif

unsigned long previousMillis = 0;  // Tracks last toggle time
unsigned long lastInputMillis = 0;
const unsigned long SLEEP_AFTER = 60000; // 1 minute in ms

String tvIP = TV_IP;
String tvMac = TV_MAC;
String tvClientKey = TV_CLIENT_KEY;

// Pins (from .env or hardcoded)
const int buttonPin = BUTTON_PIN;
const int ledPin = LED_PIN;
// WebSocket client
WebSocketsClient webSocket;
// Pairing flag (true for first run to prompt on TV)
bool needsPairing = false;

// Request ID counter
static int requestId = 0;

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            Serial.println("WebSocket connected to LG TV!");
            // Send registration JSON
            JsonDocument doc;
            doc["type"] = "register";
            doc["id"] = "register_0";
            auto payloadObj = doc["payload"].to<JsonObject>();
            payloadObj["forcePairing"] = false;
            payloadObj["pairingType"] = "PROMPT";
            if (!needsPairing && !tvClientKey.isEmpty()) {
                payloadObj["client-key"] = tvClientKey;
            }
            // Manifest
            auto manifest = payloadObj["manifest"].to<JsonObject>();
            manifest["manifestVersion"] = 1;
            manifest["appVersion"] = "1.0";
            auto permissions = manifest["permissions"].to<JsonArray>();
            permissions.add("LAUNCH");
            permissions.add("CONTROL_AUDIO");
            permissions.add("CONTROL_POWER");
            String json;
            serializeJson(doc, json);
            Serial.println("Sending registration: " + json);
            webSocket.sendTXT(json);
            break;
        }
        case WStype_TEXT: {
            Serial.printf("Response: %s\n", payload);
            // Parse for client-key (only if needed)
            JsonDocument resp;
            DeserializationError error = deserializeJson(resp, payload);
            if (error) {
                Serial.printf("Deserialize failed: %s\n", error.c_str());
                break;
            }
            if (resp["type"] == "registered") {
                String newKey = resp["payload"]["client-key"].as<String>();
                if (!newKey.isEmpty()) {
                    Serial.printf("Client key: %s (already have it)\n", newKey.c_str());
                }
            }
            break;
        }
        case WStype_DISCONNECTED: {
            Serial.println("WebSocket disconnected");
            break;
        }
        case WStype_ERROR: {
            Serial.printf("WebSocket error: %s\n", payload ? (char*)payload : "Unknown");
            break;
        }
        default: {
            break;
        }
    }
}

void sendLgCommand(const char* uri) {
    if (!webSocket.isConnected()) {
        Serial.println("Not connected—can't send command");
        return;
    }
    JsonDocument doc;
    doc["type"] = "request";
    doc["id"] = String("req_") + (++requestId);
    doc["uri"] = uri;
    String json;
    serializeJson(doc, json);
    webSocket.sendTXT(json);
    Serial.printf("Sent command: %s\n", uri);
}

void sendWol() {
    Serial.println("Attempting WoL to turn TV on...");
    WiFiUDP udp;
    udp.beginPacket("255.255.255.255", 9);
    byte mac[6];
    if (sscanf(tvMac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        Serial.println("Error: Invalid TV_MAC format—check .env");
        return;
    }
    // Magic packet: 6x FF followed by 16x MAC
    for (int i = 0; i < 6; ++i) udp.write(0xFF);
    for (int i = 0; i < 16; ++i) udp.write(mac, 6);
    udp.endPacket();
    Serial.println("WoL packet sent (first attempt)");
    
    // Retry once after delay (helps some setups)
    delay(2000);
    udp.beginPacket("255.255.255.255", 9);
    for (int i = 0; i < 6; ++i) udp.write(0xFF);
    for (int i = 0; i < 16; ++i) udp.write(mac, 6);
    udp.endPacket();
    Serial.println("WoL packet sent (second attempt)");
}

void toggleTvPower() {
    if (webSocket.isConnected()) {
        Serial.println("TV connected—turning OFF via API");
        sendLgCommand("ssap://system/turnOff");
    } else {
        Serial.println("TV disconnected—turning ON via WoL");
        sendWol();
    }
}

void setup() {
    Serial.begin(115200);
    initPins();
    pinMode(buttonPin, INPUT_PULLUP);
    Serial.println("Starting WIFI button test...");
    lastInputMillis = millis(); // Initialize start time
    wifi_scan();
    if (wifi_setup()) {  // Assuming your function; replace if different
        Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

        /* Simple TCP test to TV port 3000
        WiFiClient client;
        if (client.connect(tvIP.c_str(), 3000)) {
            Serial.println("Success: Connected to TV on port 3000!");
            client.stop();  // Close immediately
        } else {
            Serial.println("Failure: Could not connect to TV on port 3000.");
        }*/
        // Start secure WebSocket (wss://IP:3001/)
        webSocket.beginSSL(tvIP.c_str(), 3001, "/");
        webSocket.onEvent(webSocketEvent);
        webSocket.setReconnectInterval(5000);  // Auto-retry
    }
    else {
        Serial.println("WiFi failed.");
    }   
}
    
    // initPins();
    // digitalWrite(LED_PIN, LOW);      // turn LED off before starting loop, to avoid it being left on after reset

void loop() {
    // handleButtonLED(lastInputMillis, previousMillis);
    // if (millis() - lastInputMillis >= SLEEP_AFTER) {
    //     Serial.println("1 minute elapsed — entering deep sleep.");
    //     Serial.flush();
    //     digitalWrite(LED_PIN, LOW);      // turn LED off before sleep
    //     esp_deep_sleep_start();              // wake requires reset or touch/ext wakeup
    // }
    webSocket.loop();
    // Button to toggle TV power (debounce simply)
    static unsigned long lastPress = 0;
    if (digitalRead(buttonPin) == LOW && millis() - lastPress > 1000) {  // Longer debounce for testing
        toggleTvPower();
        lastPress = millis();
    }
}
