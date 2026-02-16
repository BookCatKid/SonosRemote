#include "DeviceCache.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

const char* DeviceCache::CACHE_FILE = "/sonos_devices.json";

bool DeviceCache::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return false;
    }
    filesystemReady = true;
    return true;
}

std::vector<CachedDevice> DeviceCache::loadDevices() {
    std::vector<CachedDevice> devices;
    
    if (!filesystemReady || !LittleFS.exists(CACHE_FILE)) {
        return devices;
    }
    
    File file = LittleFS.open(CACHE_FILE, "r");
    if (!file) {
        return devices;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("Failed to parse device cache");
        return devices;
    }
    
    JsonArray array = doc.as<JsonArray>();
    for (JsonObject obj : array) {
        CachedDevice dev;
        dev.name = obj["name"].as<String>();
        dev.ip = obj["ip"].as<String>();
        devices.push_back(dev);
    }
    
    return devices;
}

bool DeviceCache::saveDevices(const std::vector<SonosDevice>& devices) {
    if (!filesystemReady) {
        return false;
    }
    
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& device : devices) {
        JsonObject obj = array.add<JsonObject>();
        obj["name"] = device.name;
        obj["ip"] = device.ip;
    }
    
    File file = LittleFS.open(CACHE_FILE, "w");
    if (!file) {
        return false;
    }
    
    bool success = serializeJson(doc, file) > 0;
    file.close();
    
    if (success) {
        Serial.println("Saved " + String(devices.size()) + " devices to cache");
    }
    
    return success;
}

bool DeviceCache::hasCachedDevices() {
    return filesystemReady && LittleFS.exists(CACHE_FILE);
}

void DeviceCache::clear() {
    if (filesystemReady && LittleFS.exists(CACHE_FILE)) {
        LittleFS.remove(CACHE_FILE);
    }
}
