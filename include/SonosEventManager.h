#pragma once
#include <Arduino.h>
#include <WiFiServer.h>
#include <vector>
#include <functional>

struct SonosEvent {
    String type; // e.g., "TransportState", "Volume", "TrackMetaData"
    String value;
};

class SonosEventManager {
public:
    typedef std::function<void(const String& ip, const String& service, const String& lastChange)> EventCallback;

    SonosEventManager(int port = 8080);
    
    void begin();
    void update();
    
    // Subscribe to a service (e.g., "AVTransport", "RenderingControl")
    bool subscribe(const String& deviceIP, const String& service);
    void unsubscribe(const String& deviceIP, const String& service);
    
    void setEventCallback(EventCallback callback) { _eventCallback = callback; }

private:
    static constexpr unsigned long RENEW_CHECK_INTERVAL_MS = 1000;
    static constexpr unsigned long SUB_RENEW_INTERVAL_MS = 270000;
    static constexpr size_t MAX_NOTIFY_BODY_BYTES = 16384;

    int _port;
    WiFiServer _server;
    unsigned long _lastRenewCheckMs = 0;
    
    struct Subscription {
        String ip;
        String service;
        String sid;
        unsigned long expiry;
        unsigned long lastRenewal;
    };
    std::vector<Subscription> _subscriptions;
    
    EventCallback _eventCallback = nullptr;
    
    void handleClient();
    bool sendSubscribeRequest(Subscription& sub, bool isRenewal = false);
};
