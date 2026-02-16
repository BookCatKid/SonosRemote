#ifndef SONOS_H
#define SONOS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <vector>
#include <functional>

enum class SonosResult {
    SUCCESS = 0,
    ERROR_NETWORK = -1,
    ERROR_TIMEOUT = -2,
    ERROR_INVALID_DEVICE = -3,
    ERROR_SOAP_FAULT = -4,
    ERROR_NO_MEMORY = -5,
    ERROR_INVALID_PARAM = -6
};

struct SonosDevice {
    String name;
    String ip;
    String uuid;
};

struct SonosConfig {
    uint16_t discoveryTimeoutMs = 10000;
    uint16_t soapTimeoutMs = 10000;
    uint8_t maxRetries = 3;
    uint16_t discoveryPort = 1901;
    bool enableLogging = false;
    bool enableVerboseLogging = false;
};

class Sonos {
private:
    WiFiUDP _udp;
    HTTPClient _http;
    std::vector<SonosDevice> _devices;
    SonosConfig _config;
    bool _initialized = false;
    bool _isDiscovering = false;
    unsigned long _discoveryStartTime = 0;
    std::vector<SonosDevice> _newDevices;
    
    // SSDP/UPnP constants
    static const char* SSDP_MULTICAST_IP;
    static const int SSDP_PORT = 1900;
    static const char* SONOS_DEVICE_TYPE;
    static const char* SSDP_SEARCH_REQUEST;
    
    // SOAP templates
    static const char* SOAP_ENVELOPE_TEMPLATE;
    static const char* VOLUME_SET_TEMPLATE;
    static const char* VOLUME_GET_TEMPLATE;
    static const char* MUTE_SET_TEMPLATE;
    static const char* TRANSPORT_PLAY_TEMPLATE;
    static const char* TRANSPORT_PAUSE_TEMPLATE;
    static const char* TRANSPORT_STOP_TEMPLATE;
    static const char* TRANSPORT_NEXT_TEMPLATE;
    static const char* TRANSPORT_PREVIOUS_TEMPLATE;
    static const char* GET_POSITION_INFO_TEMPLATE;
    static const char* GET_TRANSPORT_INFO_TEMPLATE;
    
    bool parseDeviceDescription(const String& xml, SonosDevice& device);
    bool getXmlValue(const String& xml, const String& tag, String& value, const char* context, bool required = true);
    bool parseTimeToSeconds(const String& value, int& seconds, const char* context);
    String summarizeXml(const String& xml, int maxLen = 200);
    SonosResult sendSoapRequest(const String& deviceIP, const String& service, 
                               const String& action, const String& body, String& response);
    String formatSoapRequest(const String& service, const String& action, const String& body);
    bool isValidIP(const String& ip);
    void logMessage(const String& message);
    
public:
    Sonos();
    Sonos(const SonosConfig& config);
    
    SonosResult begin();
    void end();
    bool isInitialized() const { return _initialized; }
    
    // Discovery
    SonosResult discoverDevices();
    void updateDiscovery();
    bool isDiscovering() const { return _isDiscovering; }
    std::vector<SonosDevice> getDiscoveredDevices() const;
    void setDevices(const std::vector<SonosDevice>& devices) { _devices = devices; }
    SonosDevice* getDeviceByName(const String& name);
    SonosDevice* getDeviceByIP(const String& ip);
    int getDeviceCount() const { return _devices.size(); }
    
    // Control
    SonosResult setVolume(const String& deviceIP, int volume);
    SonosResult getVolume(const String& deviceIP, int& volume);
    SonosResult increaseVolume(const String& deviceIP, int increment = 5);
    SonosResult decreaseVolume(const String& deviceIP, int decrement = 5);
    SonosResult setMute(const String& deviceIP, bool mute);
    
    SonosResult play(const String& deviceIP);
    SonosResult pause(const String& deviceIP);
    SonosResult stop(const String& deviceIP);
    SonosResult next(const String& deviceIP);
    SonosResult previous(const String& deviceIP);
    
    // Info
    SonosResult getTrackInfo(const String& deviceIP, String& title, String& artist, String& album, String& albumArtUrl, int& duration);
    SonosResult getPlaybackState(const String& deviceIP, String& state);
    SonosResult getPositionInfo(const String& deviceIP, int& position, int& duration);
    
    void setConfig(const SonosConfig& config) { _config = config; }
    SonosConfig getConfig() const { return _config; }
    String getErrorString(SonosResult result);
    
    // Callbacks
    typedef std::function<void(const SonosDevice&)> DeviceFoundCallback;
    typedef std::function<void(const String&)> LogCallback;
    
    void setDeviceFoundCallback(DeviceFoundCallback callback) { _deviceFoundCallback = callback; }
    void setLogCallback(LogCallback callback) { _logCallback = callback; }
    
private:
    DeviceFoundCallback _deviceFoundCallback = nullptr;
    LogCallback _logCallback = nullptr;
};

#endif
