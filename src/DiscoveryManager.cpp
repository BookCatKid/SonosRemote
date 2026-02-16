#include "DiscoveryManager.h"

DiscoveryManager::DiscoveryManager(Sonos& sonos, DeviceCache& cache)
    : _sonos(sonos), _cache(cache), _lastDiscoveryTime(0) {
    _sonos.setDeviceFoundCallback([this](const SonosDevice& device) {
        bool exists = false;
        for (const auto& d : _devices) {
            if (d.ip == device.ip) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            _devices.push_back(device);
            if (_discoveryCallback) _discoveryCallback(_devices);
        }
    });
}

void DiscoveryManager::begin() {
    auto cached = _cache.loadDevices();
    _devices.clear();
    for (const auto& c : cached) {
        SonosDevice dev;
        dev.name = c.name;
        dev.ip = c.ip;
        dev.uuid = c.uuid;
        _devices.push_back(dev);
    }
    _sonos.setDevices(_devices);
    _lastDiscoveryTime = millis();
}

bool DiscoveryManager::update() {
    if (_sonos.isDiscovering()) {
        _sonos.updateDiscovery();
        if (!_sonos.isDiscovering()) {
            if (_devices.size() > 0) _cache.saveDevices(_devices);
        }
        return true;
    }

    if (_lastDiscoveryTime != 0 && millis() - _lastDiscoveryTime > DISCOVERY_INTERVAL) {
        forceRefresh();
        return true;
    }
    return false;
}

void DiscoveryManager::forceRefresh() {
    if (WiFi.status() != WL_CONNECTED || _sonos.isDiscovering()) return;

    Serial.println("[Discovery] Starting manual refresh...");
    _lastDiscoveryTime = millis();
    _devices.clear();
    _sonos.discoverDevices();
}
