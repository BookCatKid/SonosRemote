#include "DiscoveryManager.h"

DiscoveryManager::DiscoveryManager(Sonos& sonos, DeviceCache& cache) 
    : _sonos(sonos), _cache(cache), _lastDiscoveryTime(0) {}

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
}

bool DiscoveryManager::update() {
    unsigned long now = millis();
    if (_lastDiscoveryTime == 0 || now - _lastDiscoveryTime > DISCOVERY_INTERVAL) {
        forceRefresh();
        return true;
    }
    return false;
}

void DiscoveryManager::forceRefresh() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    _lastDiscoveryTime = millis();
    if (_sonos.discoverDevices() == SonosResult::SUCCESS) {
        _devices = _sonos.getDiscoveredDevices();
        if (_devices.size() > 0) {
            _cache.saveDevices(_devices);
        }
    }
}
