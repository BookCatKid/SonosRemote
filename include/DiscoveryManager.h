#pragma once
#include <Arduino.h>
#include <Sonos.h>
#include <vector>
#include "DeviceCache.h"

class DiscoveryManager {
public:
    DiscoveryManager(Sonos& sonos, DeviceCache& cache);
    
    void begin();
    bool update();
    const std::vector<SonosDevice>& getDevices() const { return _devices; }
    
    void forceRefresh();

private:
    Sonos& _sonos;
    DeviceCache& _cache;
    std::vector<SonosDevice> _devices;
    unsigned long _lastDiscoveryTime;
    const unsigned long DISCOVERY_INTERVAL = 300000; // 5 minutes
};
