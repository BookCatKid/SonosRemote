#pragma once
#include <Arduino.h>
#include <Sonos.h>
#include <vector>
#include <functional>
#include "DeviceCache.h"

class DiscoveryManager {
public:
    typedef std::function<void(const std::vector<SonosDevice>&)> DiscoveryCallback;

    DiscoveryManager(Sonos& sonos, DeviceCache& cache);
    
    void begin();
    bool update();
    const std::vector<SonosDevice>& getDevices() const { return _devices; }
    
    void forceRefresh();
    void setDiscoveryCallback(DiscoveryCallback callback) { _discoveryCallback = callback; }

private:
    Sonos& _sonos;
    DeviceCache& _cache;
    std::vector<SonosDevice> _devices;
    DiscoveryCallback _discoveryCallback = nullptr;
    unsigned long _lastDiscoveryTime;
    const unsigned long DISCOVERY_INTERVAL = 300000; // 5 minutes
};
