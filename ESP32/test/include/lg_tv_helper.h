#ifndef LG_TV_HELPER_H
#define LG_TV_HELPER_H

#include <WebSocketsClient.h>  // For WebSocket connection

extern String tvIP;
extern String tvMac;
extern String tvClientKey;

void initLgTv();                  // Setup WebSocket
void handleLgTvWebSocket();       // Call in loop() to handle events
void sendLgCommand(const char* uri, const char* payload = nullptr);  // Send API request
void toggleTvPower();             // Turn on (WoL) or off (API)
void sendWol();                   // Wake-on-LAN helper

#endif