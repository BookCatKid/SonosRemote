#include "Sonos.h"
#include <ArduinoJson.h>

// Static constants
const char* Sonos::SSDP_MULTICAST_IP = "239.255.255.250";
const char* Sonos::SONOS_DEVICE_TYPE = "urn:schemas-upnp-org:device:ZonePlayer:1";

const char* Sonos::SSDP_SEARCH_REQUEST =
    "M-SEARCH * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900\r\n"
    "MAN: \"ssdp:discover\"\r\n"
    "MX: 1\r\n"
    "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n"
    "USER-AGENT: ESP32/1.0 UPnP/1.0 Sonos/1.0\r\n\r\n";

const char* Sonos::SOAP_ENVELOPE_TEMPLATE =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
    "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
    "<s:Body>%s</s:Body></s:Envelope>";

const char* Sonos::VOLUME_SET_TEMPLATE =
    "<u:SetVolume xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">"
    "<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredVolume>%d</DesiredVolume>"
    "</u:SetVolume>";

const char* Sonos::VOLUME_GET_TEMPLATE =
    "<u:GetVolume xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">"
    "<InstanceID>0</InstanceID><Channel>Master</Channel></u:GetVolume>";

const char* Sonos::MUTE_SET_TEMPLATE =
    "<u:SetMute xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">"
    "<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredMute>%d</DesiredMute>"
    "</u:SetMute>";

const char* Sonos::TRANSPORT_PLAY_TEMPLATE =
    "<u:Play xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID><Speed>1</Speed></u:Play>";

const char* Sonos::TRANSPORT_PAUSE_TEMPLATE =
    "<u:Pause xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID></u:Pause>";

const char* Sonos::TRANSPORT_STOP_TEMPLATE =
    "<u:Stop xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID></u:Stop>";

const char* Sonos::TRANSPORT_NEXT_TEMPLATE =
    "<u:Next xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID></u:Next>";

const char* Sonos::TRANSPORT_PREVIOUS_TEMPLATE =
    "<u:Previous xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID></u:Previous>";

const char* Sonos::GET_POSITION_INFO_TEMPLATE =
    "<u:GetPositionInfo xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID></u:GetPositionInfo>";

const char* Sonos::GET_TRANSPORT_INFO_TEMPLATE =
    "<u:GetTransportInfo xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID></u:GetTransportInfo>";

// Constructor
Sonos::Sonos() {
    // Use default configuration
}

Sonos::Sonos(const SonosConfig& config) : _config(config) {
    // Use provided configuration
}

// Initialization
SonosResult Sonos::begin() {
    if (_initialized) {
        return SonosResult::SUCCESS;
    }

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        logMessage("WiFi not connected");
        return SonosResult::ERROR_NETWORK;
    }

    // Initialize UDP for SSDP discovery
    if (!_udp.begin(_config.discoveryPort)) {
        logMessage("Failed to initialize UDP");
        return SonosResult::ERROR_NETWORK;
    }

    // Configure HTTP client
    _http.setTimeout(_config.soapTimeoutMs);
    _http.setReuse(true);

    _initialized = true;
    logMessage("Sonos library initialized successfully");
    return SonosResult::SUCCESS;
}

void Sonos::end() {
    if (!_initialized) return;

    _udp.stop();
    _http.end();
    _devices.clear();
    _initialized = false;
    logMessage("Sonos library ended");
}

// Device discovery implementation
SonosResult Sonos::discoverDevices() {
    if (!_initialized) {
        return SonosResult::ERROR_INVALID_DEVICE;
    }

    logMessage("Starting device discovery...");

    // Create a temporary list for newly discovered devices
    std::vector<SonosDevice> newDevices;

    // Send SSDP multicast request
    IPAddress multicastIP;
    multicastIP.fromString(SSDP_MULTICAST_IP);

    _udp.beginPacket(multicastIP, SSDP_PORT);
    _udp.write((const uint8_t*)SSDP_SEARCH_REQUEST, strlen(SSDP_SEARCH_REQUEST));
    bool sent = _udp.endPacket();

    if (!sent) {
        logMessage("Failed to send SSDP request");
        return SonosResult::ERROR_NETWORK;
    }

    // Listen for responses
    unsigned long startTime = millis();
    int discoveredCount = 0;

    while (millis() - startTime < _config.discoveryTimeoutMs) {
        int packetSize = _udp.parsePacket();
        if (packetSize > 0) {
            String response = _udp.readString();

            // Parse SSDP response
            if (response.indexOf("ZonePlayer") != -1) {
                // Extract location URL
                int locationStart = response.indexOf("LOCATION: ") + 10;
                int locationEnd = response.indexOf("\r\n", locationStart);
                if (locationStart > 9 && locationEnd > locationStart) {
                    String locationUrl = response.substring(locationStart, locationEnd);

                    // Extract IP from URL
                    int ipStart = locationUrl.indexOf("//") + 2;
                    int ipEnd = locationUrl.indexOf(":", ipStart);
                    String deviceIP = locationUrl.substring(ipStart, ipEnd);

                    if (isValidIP(deviceIP)) {
                        // Fetch device description
                        _http.begin(locationUrl);
                        int httpCode = _http.GET();

                        if (httpCode == HTTP_CODE_OK) {
                            String xmlResponse = _http.getString();
                            SonosDevice device;

                            if (parseDeviceDescription(xmlResponse, device)) {
                                device.ip = deviceIP;

                                // Check if device already exists in new list
                                bool deviceExists = false;
                                for (auto& existingDevice : newDevices) {
                                    if (existingDevice.ip == device.ip) {
                                        existingDevice = device;  // Update existing
                                        deviceExists = true;
                                        break;
                                    }
                                }

                                if (!deviceExists) {
                                    newDevices.push_back(device);
                                    discoveredCount++;
                                    logMessage("Discovered device: " + device.name + " at " + device.ip);

                                    // Call callback if registered
                                    if (_deviceFoundCallback) {
                                        _deviceFoundCallback(device);
                                    }
                                }
                            }
                        }
                        _http.end();
                    }
                }
            }
        }
        delay(10);  // Small delay to prevent busy waiting
    }

    // Replace the device list with newly discovered devices
    _devices = newDevices;

    logMessage("Discovery complete. Found " + String(_devices.size()) + " devices");
    return SonosResult::SUCCESS;
}

bool Sonos::parseDeviceDescription(const String& xml, SonosDevice& device) {
    device.name = extractXmlValue(xml, "roomName");

    // Check if it's a valid speaker (has internal speakers)
    String speakerSize = extractXmlValue(xml, "internalSpeakerSize");
    if (speakerSize.length() > 0) {
        int size = speakerSize.toInt();
        if (size < 0) {
            return false;  // Not a speaker device
        }
    }

    return device.name.length() > 0;
}

String Sonos::extractXmlValue(const String& xml, const String& tag) {
    String startTag = "<" + tag + ">";
    String endTag = "</" + tag + ">";

    int startPos = xml.indexOf(startTag);
    if (startPos == -1) return "";

    startPos += startTag.length();
    int endPos = xml.indexOf(endTag, startPos);
    if (endPos == -1) return "";

    return xml.substring(startPos, endPos);
}

// Volume control implementation
SonosResult Sonos::setVolume(const String& deviceIP, int volume) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;
    if (volume < 0 || volume > 100) return SonosResult::ERROR_INVALID_PARAM;

    char body[200];
    snprintf(body, sizeof(body), VOLUME_SET_TEMPLATE, volume);

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "RenderingControl", "SetVolume", body, response);

    if (result == SonosResult::SUCCESS) {
        logMessage("Volume set to " + String(volume) + " on " + deviceIP);
    }

    return result;
}

SonosResult Sonos::getVolume(const String& deviceIP, int& volume) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "RenderingControl", "GetVolume",
                                        VOLUME_GET_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        String volumeStr = extractXmlValue(response, "CurrentVolume");
        if (volumeStr.length() > 0) {
            volume = volumeStr.toInt();
            logMessage("Current volume: " + String(volume) + " on " + deviceIP);
        } else {
            return SonosResult::ERROR_SOAP_FAULT;
        }
    }

    return result;
}

SonosResult Sonos::increaseVolume(const String& deviceIP, int increment) {
    int currentVolume;
    SonosResult result = getVolume(deviceIP, currentVolume);
    if (result != SonosResult::SUCCESS) return result;

    int newVolume = min(100, currentVolume + increment);
    return setVolume(deviceIP, newVolume);
}

SonosResult Sonos::decreaseVolume(const String& deviceIP, int decrement) {
    int currentVolume;
    SonosResult result = getVolume(deviceIP, currentVolume);
    if (result != SonosResult::SUCCESS) return result;

    int newVolume = max(0, currentVolume - decrement);
    return setVolume(deviceIP, newVolume);
}

// Mute control
SonosResult Sonos::setMute(const String& deviceIP, bool mute) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    char body[200];
    snprintf(body, sizeof(body), MUTE_SET_TEMPLATE, mute ? 1 : 0);

    String response;
    return sendSoapRequest(deviceIP, "RenderingControl", "SetMute", body, response);
}

// Playback control implementation
SonosResult Sonos::play(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Play",
                                        TRANSPORT_PLAY_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        logMessage("Play command sent to " + deviceIP);
    }

    return result;
}

SonosResult Sonos::pause(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Pause",
                                        TRANSPORT_PAUSE_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        logMessage("Pause command sent to " + deviceIP);
    }

    return result;
}

SonosResult Sonos::stop(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Stop",
                                        TRANSPORT_STOP_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        logMessage("Stop command sent to " + deviceIP);
    }

    return result;
}

SonosResult Sonos::next(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Next",
                                        TRANSPORT_NEXT_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        logMessage("Next command sent to " + deviceIP);
    }

    return result;
}

SonosResult Sonos::previous(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Previous",
                                        TRANSPORT_PREVIOUS_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        logMessage("Previous command sent to " + deviceIP);
    }

    return result;
}

// SOAP request implementation
SonosResult Sonos::sendSoapRequest(const String& deviceIP, const String& service,
                                  const String& action, const String& body, String& response) {
    if (!isValidIP(deviceIP)) {
        return SonosResult::ERROR_INVALID_PARAM;
    }

    // Format the complete SOAP request
    String soapBody = formatSoapRequest(service, action, body);
    String url = "http://" + deviceIP + ":1400/MediaRenderer/" + service + "/Control";

    // Set up HTTP request
    _http.begin(url);
    _http.addHeader("Content-Type", "text/xml; charset=utf-8");
    _http.addHeader("SOAPAction", "\"urn:schemas-upnp-org:service:" + service + ":1#" + action + "\"");

    // Send request with retries
    int httpCode = -1;
    for (int retry = 0; retry < _config.maxRetries && httpCode != HTTP_CODE_OK; retry++) {
        httpCode = _http.POST(soapBody);
        if (httpCode != HTTP_CODE_OK) {
            delay(100 * (retry + 1));  // Exponential backoff
        }
    }

    if (httpCode == HTTP_CODE_OK) {
        response = _http.getString();
        _http.end();
        return SonosResult::SUCCESS;
    } else if (httpCode == HTTP_CODE_INTERNAL_SERVER_ERROR) {
        response = _http.getString();
        _http.end();
        return SonosResult::ERROR_SOAP_FAULT;
    } else {
        _http.end();
        logMessage("HTTP error: " + String(httpCode));
        return SonosResult::ERROR_NETWORK;
    }
}

String Sonos::formatSoapRequest(const String& service, const String& action, const String& body) {
    char envelope[2048];
    snprintf(envelope, sizeof(envelope), SOAP_ENVELOPE_TEMPLATE, body.c_str());
    return String(envelope);
}

// Utility methods
bool Sonos::isValidIP(const String& ip) {
    IPAddress addr;
    return addr.fromString(ip);
}

void Sonos::logMessage(const String& message) {
    if (_config.enableLogging) {
        if (_logCallback) {
            _logCallback(message);
        } else {
            Serial.println("[Sonos] " + message);
        }
    }
}

std::vector<SonosDevice> Sonos::getDiscoveredDevices() const {
    return _devices;
}

SonosDevice* Sonos::getDeviceByName(const String& name) {
    for (auto& device : _devices) {
        if (device.name.equalsIgnoreCase(name)) {
            return &device;
        }
    }
    return nullptr;
}

SonosDevice* Sonos::getDeviceByIP(const String& ip) {
    for (auto& device : _devices) {
        if (device.ip == ip) {
            return &device;
        }
    }
    return nullptr;
}

String Sonos::getErrorString(SonosResult result) {
    switch (result) {
        case SonosResult::SUCCESS: return "Success";
        case SonosResult::ERROR_NETWORK: return "Network error";
        case SonosResult::ERROR_TIMEOUT: return "Timeout";
        case SonosResult::ERROR_INVALID_DEVICE: return "Invalid device";
        case SonosResult::ERROR_SOAP_FAULT: return "SOAP fault";
        case SonosResult::ERROR_NO_MEMORY: return "No memory";
        case SonosResult::ERROR_INVALID_PARAM: return "Invalid parameter";
        default: return "Unknown error";
    }
}

// Track information
SonosResult Sonos::getTrackInfo(const String& deviceIP, String& title, String& artist, String& album, String& albumArtUrl, int& duration) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "GetPositionInfo",
                                        GET_POSITION_INFO_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        String metadata = extractXmlValue(response, "TrackMetaData");

        // Metadata is escaped XML inside the SOAP response.
        // We unescape it once here so we can use standard extractXmlValue on it.
        metadata.replace("&amp;", "&");
        metadata.replace("&lt;", "<");
        metadata.replace("&gt;", ">");
        metadata.replace("&quot;", "\"");
        metadata.replace("&apos;", "'");

        if (metadata.length() > 0) {
            title = extractXmlValue(metadata, "dc:title");
            artist = extractXmlValue(metadata, "dc:creator");
            album = extractXmlValue(metadata, "upnp:album");
            albumArtUrl = extractXmlValue(metadata, "upnp:albumArtURI");
            albumArtUrl.replace("&amp;", "&");
            if (albumArtUrl.length() > 0 && albumArtUrl.startsWith("/")) {
                albumArtUrl = "http://" + deviceIP + ":1400" + albumArtUrl;
            }
        }

        if (title.length() == 0) title = "Unknown Title";
        if (artist.length() == 0) artist = "Unknown Artist";

        // Parse duration from "H:MM:SS" or "M:SS" format
        String durationStr = extractXmlValue(response, "TrackDuration");
        duration = 0;
        if (durationStr.length() > 0 && durationStr != "NOT_IMPLEMENTED") {
            int firstColon = durationStr.indexOf(':');
            int lastColon = durationStr.lastIndexOf(':');
            if (firstColon != -1 && lastColon != -1 && firstColon != lastColon) {
                // H:MM:SS format
                int hours = durationStr.substring(0, firstColon).toInt();
                int minutes = durationStr.substring(firstColon + 1, lastColon).toInt();
                int seconds = durationStr.substring(lastColon + 1).toInt();
                duration = hours * 3600 + minutes * 60 + seconds;
            } else if (firstColon != -1) {
                // M:SS format
                int minutes = durationStr.substring(0, firstColon).toInt();
                int seconds = durationStr.substring(firstColon + 1).toInt();
                duration = minutes * 60 + seconds;
            }
        }

        logMessage("Track info: " + title + " by " + artist + " (Art: " + albumArtUrl + ")");
    }

    return result;
}

SonosResult Sonos::getPlaybackState(const String& deviceIP, String& state) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "GetTransportInfo",
                                        GET_TRANSPORT_INFO_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        state = extractXmlValue(response, "CurrentTransportState");
        logMessage("Playback state: " + state);
    }

    return result;
}

SonosResult Sonos::getPositionInfo(const String& deviceIP, int& position, int& duration) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "GetPositionInfo",
                                        GET_POSITION_INFO_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        // Parse current position
        String relTime = extractXmlValue(response, "RelTime");
        position = 0;
        if (relTime.length() > 0 && relTime != "NOT_IMPLEMENTED") {
            int firstColon = relTime.indexOf(':');
            int lastColon = relTime.lastIndexOf(':');
            if (firstColon != -1 && lastColon != -1 && firstColon != lastColon) {
                int hours = relTime.substring(0, firstColon).toInt();
                int minutes = relTime.substring(firstColon + 1, lastColon).toInt();
                int seconds = relTime.substring(lastColon + 1).toInt();
                position = hours * 3600 + minutes * 60 + seconds;
            } else if (firstColon != -1) {
                int minutes = relTime.substring(0, firstColon).toInt();
                int seconds = relTime.substring(firstColon + 1).toInt();
                position = minutes * 60 + seconds;
            }
        }

        // Parse track duration
        String durationStr = extractXmlValue(response, "TrackDuration");
        duration = 0;
        if (durationStr.length() > 0 && durationStr != "NOT_IMPLEMENTED") {
            int firstColon = durationStr.indexOf(':');
            int lastColon = durationStr.lastIndexOf(':');
            if (firstColon != -1 && lastColon != -1 && firstColon != lastColon) {
                int hours = durationStr.substring(0, firstColon).toInt();
                int minutes = durationStr.substring(firstColon + 1, lastColon).toInt();
                int seconds = durationStr.substring(lastColon + 1).toInt();
                duration = hours * 3600 + minutes * 60 + seconds;
            } else if (firstColon != -1) {
                int minutes = durationStr.substring(0, firstColon).toInt();
                int seconds = durationStr.substring(firstColon + 1).toInt();
                duration = minutes * 60 + seconds;
            }
        }
    }

    return result;
}
