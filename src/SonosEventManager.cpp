#include "SonosEventManager.h"
#include "AppLogger.h"
#include <WiFi.h>

SonosEventManager::SonosEventManager(int port) : _port(port), _server(port) {}

void SonosEventManager::begin() {
    _server.begin();
    LOG_INFO("events", "Server started on port " + String(_port));
}

void SonosEventManager::update() {
    handleClient();
    
    // Check for renewals (every 4.5 minutes for a 5 min subscription)
    unsigned long now = millis();
    for (auto& sub : _subscriptions) {
        if (now - sub.lastRenewal > 270000) { // 4.5 minutes
            sendSubscribeRequest(sub, true);
        }
    }
}

void SonosEventManager::handleClient() {
    WiFiClient client = _server.available();
    if (!client) return;

    String requestLine = client.readStringUntil('\r');
    client.readStringUntil('\n');
    
    LOG_DEBUG("events", "Incoming request: " + requestLine);

    if (requestLine.startsWith("NOTIFY")) {
        // Skip headers
        while (client.connected()) {
            String line = client.readStringUntil('\r');
            client.readStringUntil('\n');
            if (line.length() <= 2) break;
        }

        // FAST READ LOGIC
        String body = "";
        while (client.connected() && client.available()) {
            body += (char)client.read();
        }

        if (body.length() > 0) {
            String remoteIP = client.remoteIP().toString();
            if (_eventCallback) {
                _eventCallback(remoteIP, "NOTIFY", body);
            }
        }

        // Send minimal response
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Length: 0");
        client.println();
    }
    
    client.stop();
}

bool SonosEventManager::subscribe(const String& deviceIP, const String& service) {
    Subscription sub;
    sub.ip = deviceIP;
    sub.service = service;
    sub.sid = "";
    sub.expiry = 0;
    sub.lastRenewal = millis();
    
    if (sendSubscribeRequest(sub)) {
        _subscriptions.push_back(sub);
        LOG_INFO("events", "Subscribed to " + service + " on " + deviceIP);
        return true;
    }
    return false;
}

bool SonosEventManager::sendSubscribeRequest(Subscription& sub, bool isRenewal) {
    WiFiClient client;
    if (!client.connect(sub.ip.c_str(), 1400)) {
        LOG_ERROR("events", "Connection failed to " + sub.ip);
        return false;
    }

    String path = "/MediaRenderer/" + sub.service + "/Event";
    String callback = "<http://" + WiFi.localIP().toString() + ":" + String(_port) + "/>";

    client.println("SUBSCRIBE " + path + " HTTP/1.1");
    client.println("HOST: " + sub.ip + ":1400");
    
    if (isRenewal) {
        client.println("SID: " + sub.sid);
    } else {
        client.println("CALLBACK: " + callback);
        client.println("NT: upnp:event");
    }
    client.println("TIMEOUT: Second-300");
    client.println();

    // Parse SID from response if not a renewal
    if (!isRenewal) {
        while (client.connected()) {
            String line = client.readStringUntil('\n');
            line.trim();
            if (line.startsWith("SID: ")) {
                sub.sid = line.substring(5);
                sub.sid.trim();
            }
            if (line.length() == 0) break;
        }
    }

    sub.lastRenewal = millis();
    client.stop();
    return true;
}

void SonosEventManager::unsubscribe(const String& deviceIP, const String& service) {
    for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
        if (it->ip == deviceIP && it->service == service) {
            WiFiClient client;
            if (client.connect(deviceIP.c_str(), 1400)) {
                String path = "/MediaRenderer/" + service + "/Event";
                client.println("UNSUBSCRIBE " + path + " HTTP/1.1");
                client.println("HOST: " + deviceIP + ":1400");
                client.println("SID: " + it->sid);
                client.println();
                client.stop();
            }
            _subscriptions.erase(it);
            break;
        }
    }
}

