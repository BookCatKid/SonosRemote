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

    unsigned long now = millis();
    if (now - _lastRenewCheckMs < RENEW_CHECK_INTERVAL_MS) {
        return;
    }
    _lastRenewCheckMs = now;

    for (auto& sub : _subscriptions) {
        if (now - sub.lastRenewal > SUB_RENEW_INTERVAL_MS) {
            if (!sendSubscribeRequest(sub, true)) {
                LOG_WARN("events", "Renewal failed for " + sub.service + " on " + sub.ip + ", retrying with fresh subscribe");
                sub.sid = "";
                if (!sendSubscribeRequest(sub, false)) {
                    LOG_ERROR("events", "Resubscribe failed for " + sub.service + " on " + sub.ip);
                }
            }
        }
    }
}

void SonosEventManager::handleClient() {
    WiFiClient client = _server.available();
    if (!client) return;

    client.setTimeout(40);

    String requestLine = client.readStringUntil('\n');
    requestLine.trim();
    if (requestLine.length() == 0) {
        client.stop();
        return;
    }

    LOG_DEBUG("events", "Incoming request: " + requestLine);

    if (requestLine.startsWith("NOTIFY")) {
        int contentLength = 0;
        while (client.connected()) {
            String line = client.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) break;

            String lowerLine = line;
            lowerLine.toLowerCase();
            if (lowerLine.startsWith("content-length:")) {
                int colon = line.indexOf(':');
                String value = colon != -1 ? line.substring(colon + 1) : "0";
                value.trim();
                contentLength = value.toInt();
            }
        }

        if (contentLength < 0) contentLength = 0;
        if (contentLength > static_cast<int>(MAX_NOTIFY_BODY_BYTES)) {
            contentLength = static_cast<int>(MAX_NOTIFY_BODY_BYTES);
        }

        String body;
        body.reserve(contentLength > 0 ? contentLength : 1024);

        unsigned long deadline = millis() + 2000;
        while ((client.connected() || client.available()) && millis() < deadline) {
            while (client.available()) {
                if (contentLength > 0 && static_cast<int>(body.length()) >= contentLength) {
                    break;
                }
                body += static_cast<char>(client.read());
                if (contentLength == 0) {
                    // Keep waiting briefly while bytes keep arriving.
                    deadline = millis() + 25;
                }
            }

            if (contentLength > 0 && static_cast<int>(body.length()) >= contentLength) {
                break;
            }
            if (contentLength == 0 && !client.available()) {
                break;
            }
            delay(1);
        }

        if (contentLength > 0 && static_cast<int>(body.length()) < contentLength) {
            LOG_WARN("events", "Truncated NOTIFY body from " + client.remoteIP().toString() +
                               " expected=" + String(contentLength) + " got=" + String(body.length()));
        }

        if (body.length() > 0 && _eventCallback) {
            _eventCallback(client.remoteIP().toString(), "NOTIFY", body);
        } else if (body.length() == 0) {
            LOG_WARN("events", "Received empty NOTIFY body from " + client.remoteIP().toString());
        }

        client.print("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
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

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    if (statusLine.indexOf("200") == -1) {
        LOG_WARN("events", "Subscribe failed for " + sub.service + " on " + sub.ip + ": " + statusLine);
        client.stop();
        return false;
    }

    String sidFromResponse = "";
    unsigned long timeoutSeconds = 300;
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break;

        String lowerLine = line;
        lowerLine.toLowerCase();
        if (lowerLine.startsWith("sid:")) {
            int colon = line.indexOf(':');
            if (colon != -1) {
                sidFromResponse = line.substring(colon + 1);
                sidFromResponse.trim();
            }
        } else if (lowerLine.startsWith("timeout:")) {
            int secondIdx = lowerLine.indexOf("second-");
            if (secondIdx != -1) {
                String secs = lowerLine.substring(secondIdx + 7);
                secs.trim();
                unsigned long parsed = (unsigned long)secs.toInt();
                if (parsed > 0) timeoutSeconds = parsed;
            }
        }
    }

    if (sidFromResponse.length()) {
        sub.sid = sidFromResponse;
    }

    if (!sub.sid.length()) {
        LOG_WARN("events", "Subscribe response missing SID for " + sub.service + " on " + sub.ip);
        client.stop();
        return false;
    }

    sub.lastRenewal = millis();
    sub.expiry = sub.lastRenewal + (timeoutSeconds * 1000UL);
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
