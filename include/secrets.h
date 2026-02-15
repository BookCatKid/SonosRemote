#ifndef SECRETS_H
#define SECRETS_H

// WiFi credentials
extern const char* ssid;
extern const char* password;

// Optional: Static IP configuration
#define USE_STATIC_IP 1
#define STATIC_IP IPAddress(192, 168, 1, 100)
#define GATEWAY IPAddress(192, 168, 1, 1)
#define SUBNET IPAddress(255, 255, 255, 0)

#endif
