#include "DiscoveryManager.h"
#include "AppLogger.h"

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
    LOG_INFO("discovery", "Loaded " + String(cached.size()) + " cached devices");
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
            LOG_INFO("discovery", "Discovery finished with " + String(_devices.size()) + " devices");
            if (_devices.size() > 0) _cache.saveDevices(_devices);
        }
        return true;
    }

    return false;
}

void DiscoveryManager::forceRefresh() {
    if (WiFi.status() != WL_CONNECTED || _sonos.isDiscovering()) return;

    LOG_INFO("discovery", "Starting manual refresh");
    _lastDiscoveryTime = millis();
    _devices.clear();
    _sonos.discoverDevices();
}
