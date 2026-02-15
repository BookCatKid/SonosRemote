#ifndef DEVICE_CACHE_H
#define DEVICE_CACHE_H

#include <Arduino.h>
#include <vector>
#include <Sonos.h>

struct CachedDevice {
    String name;
    String ip;
    
    CachedDevice() = default;
    CachedDevice(const String& n, const String& i) : name(n), ip(i) {}
    CachedDevice(const SonosDevice& dev) : name(dev.name), ip(dev.ip) {}
};

class DeviceCache {
public:
    bool begin();
    std::vector<CachedDevice> loadDevices();
    bool saveDevices(const std::vector<SonosDevice>& devices);
    bool hasCachedDevices();
    void clear();
    
private:
    static const char* CACHE_FILE;
    bool filesystemReady = false;
};

#endif
