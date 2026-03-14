#include <Arduino.h>
#include <WiFi.h>  // For WiFi functionality
// #include <PioDotEnv.h>  // For reading .env constants

#ifndef WIFI_SSID
#error WIFI_SSID is not defined from .env
#endif
#ifndef WIFI_PASS
#error WIFI_PASS is not defined from .env
#endif

const int ledPin = LED_BUILTIN;  // GPIO2 for LED
unsigned long previousMillis = 0;  // Tracks last toggle time
unsigned long lastInputMillis = 0;
// int lastButtonState = HIGH;   // INPUT_PULLUP idle state

const int ButtonPin = 27;      // Change to GPIO13 for button
const unsigned long SLEEP_AFTER = 60000; // 1 minute in ms



void wifi_setup()
{
  // Set WiFi to station mode before connecting
  WiFi.mode(WIFI_STA);

  // Connect to WiFi using .env credentials
//   Serial.printf("SSID from .env: %s\n", WIFI_SSID);
//   Serial.printf("Password from .env: %s\n", WIFI_PASS);
//   esp_deep_sleep_start();     
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");

  // Timeout after 10 seconds (adjust as needed)
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      Serial.print(".");
      delay(500);
  }
  // Check status after attempt
  if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");
      Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  } else {
      Serial.println("\nFailed to connect.");
      // Print detailed status for debugging
      switch (WiFi.status()) {
          case WL_NO_SSID_AVAIL:
              Serial.println("SSID not found.");
              break;
          case WL_CONNECT_FAILED:
              Serial.println("Connection failed (wrong password?).");
              break;
          case WL_CONNECTION_LOST:
              Serial.println("Connection lost.");
              break;
          case WL_DISCONNECTED:
              Serial.println("Disconnected.");
              break;
          default:
              Serial.printf("Unknown status: %d\n", WiFi.status());
              break;
      }
  }
    
}

// Helper function to convert encryption type to string
String getEncryptionType(wifi_auth_mode_t type) {
    switch (type) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        case WIFI_AUTH_WAPI_PSK: return "WAPI";
        default: return "Unknown";
    }
}
void wifi_scan(){
  int numNetworks = WiFi.scanNetworks();  // Perform the scan (blocking call)

    if (numNetworks == 0) {
        Serial.println("No WiFi networks found.");
    } else {
        Serial.printf("%d networks found:\n", numNetworks);
        for (int i = 0; i < numNetworks; ++i) {
            Serial.printf("%d: SSID: %s | RSSI: %d dBm | Encryption: %s\n",
                          i + 1,
                          WiFi.SSID(i).c_str(),
                          WiFi.RSSI(i),
                          getEncryptionType(WiFi.encryptionType(i)).c_str());
            delay(10);  // Small delay to avoid serial overflow
        }
    }
    WiFi.scanDelete();  // Free up memory from scan results
}
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ButtonPin, INPUT_PULLUP);   // Internal pull-up
  Serial.println("Starting WIFI button test...");
  lastInputMillis = millis(); // Initialize start time
  wifi_scan();
  wifi_setup();
  
}

void button_check_led_state_blink(int ledPin, unsigned long &previousMillis)
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 100)
    {  // Check every 0.1 second
      previousMillis = currentMillis;
      int buttonState = digitalRead(ButtonPin);

    //   Serial.print("Current button state: ");
    //   Serial.println(buttonState == LOW ? "PRESSED" : "RELEASED");
      if (buttonState == LOW) {
        lastInputMillis = currentMillis;  // reset inactivity timer
        {
          bool currentState = digitalRead(ledPin);   // read actual pin state from chip
          digitalWrite(ledPin, !currentState);        // toggle it
          Serial.println(!currentState ? "LED ON" : "LED OFF");
        }
    }
  }
}

void loop() {
  button_check_led_state_blink(LED_BUILTIN, previousMillis);
  if (millis() - lastInputMillis >= SLEEP_AFTER) {
    Serial.println("1 minute elapsed — entering deep sleep.");
    Serial.flush();
    digitalWrite(LED_BUILTIN, LOW);      // turn LED off before sleep
    esp_deep_sleep_start();              // wake requires reset or touch/ext wakeup
  }
}
