#include "Sonos.h"
#include "SonosXmlParser.h"

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

const char* Sonos::GET_MEDIA_INFO_TEMPLATE =
    "<u:GetMediaInfo xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID></u:GetMediaInfo>";

Sonos::Sonos() {}
Sonos::Sonos(const SonosConfig& config) : _config(config) {}

SonosResult Sonos::begin() {
    if (_initialized) return SonosResult::SUCCESS;

    if (WiFi.status() != WL_CONNECTED) {
        logMessage(LogLevel::ERROR, "core", "WiFi not connected");
        return SonosResult::ERROR_NETWORK;
    }

    if (!_udp.begin(_config.discoveryPort)) {
        logMessage(LogLevel::ERROR, "core", "Failed to initialize UDP");
        return SonosResult::ERROR_NETWORK;
    }

    _http.setTimeout(_config.soapTimeoutMs);
    _http.setReuse(true);
    _initialized = true;
    logMessage(LogLevel::INFO, "core", "Sonos library initialized successfully");
    return SonosResult::SUCCESS;
}

void Sonos::end() {
    if (!_initialized) return;
    _udp.stop();
    _http.end();
    _devices.clear();
    _initialized = false;
    logMessage(LogLevel::INFO, "core", "Sonos library ended");
}

// Device discovery implementation
SonosResult Sonos::discoverDevices() {
    if (!_initialized) {
        return SonosResult::ERROR_INVALID_DEVICE;
    }

    logMessage(LogLevel::INFO, "discovery", "Starting device discovery");

    // Send SSDP multicast request
    IPAddress multicastIP;
    multicastIP.fromString(SSDP_MULTICAST_IP);

    _udp.beginPacket(multicastIP, SSDP_PORT);
    _udp.write((const uint8_t*)SSDP_SEARCH_REQUEST, strlen(SSDP_SEARCH_REQUEST));
    bool sent = _udp.endPacket();

    if (!sent) {
        logMessage(LogLevel::ERROR, "discovery", "Failed to send SSDP request");
        return SonosResult::ERROR_NETWORK;
    }

    _isDiscovering = true;
    _discoveryStartTime = millis();
    _newDevices.clear();

    return SonosResult::SUCCESS;
}

void Sonos::updateDiscovery() {
    if (!_isDiscovering) return;

    if (millis() - _discoveryStartTime > _config.discoveryTimeoutMs) {
        _isDiscovering = false;
        _devices = _newDevices;
        logMessage(LogLevel::INFO, "discovery", "Discovery complete. Found " + String(_devices.size()) + " devices");
        return;
    }

    int packetSize = _udp.parsePacket();
    if (packetSize > 0) {
        String response = _udp.readString();

        if (response.indexOf("ZonePlayer") != -1) {
            int locationStart = response.indexOf("LOCATION: ") + 10;
            int locationEnd = response.indexOf("\r\n", locationStart);
            if (locationStart > 9 && locationEnd > locationStart) {
                String locationUrl = response.substring(locationStart, locationEnd);

                int ipStart = locationUrl.indexOf("//") + 2;
                int ipEnd = locationUrl.indexOf(":", ipStart);
                String deviceIP = locationUrl.substring(ipStart, ipEnd);

                if (isValidIP(deviceIP)) {
                    _http.begin(locationUrl);
                    int httpCode = _http.GET();

                    if (httpCode == HTTP_CODE_OK) {
                        String xmlResponse = _http.getString();
                        SonosDevice device;

                        if (parseDeviceDescription(xmlResponse, device)) {
                            device.ip = deviceIP;

                            bool deviceExists = false;
                            for (auto& existingDevice : _newDevices) {
                                if (existingDevice.ip == device.ip) {
                                    existingDevice = device;
                                    deviceExists = true;
                                    break;
                                }
                            }

                            if (!deviceExists) {
                                _newDevices.push_back(device);
                                logMessage(LogLevel::INFO, "discovery", "Discovered device: " + device.name + " at " + device.ip);
                                if (_deviceFoundCallback) _deviceFoundCallback(device);
                            }
                        }
                    }
                    _http.end();
                }
            }
        }
    }
}

bool Sonos::parseDeviceDescription(const String& xml, SonosDevice& device) {
    if (!getXmlValue(xml, "roomName", device.name, "device description", true)) {
        return false;
    }
    getXmlValue(xml, "UDN", device.uuid, "device description", false);

    String speakerSize;
    if (getXmlValue(xml, "internalSpeakerSize", speakerSize, "device description", false)) {
        int parsedSize = 0;
        String parseError;
        bool parsed = SonosXmlParser::parseInt(speakerSize, parsedSize, parseError);
        if (parsed && parsedSize < 0) {
            parseError = "must be non-negative";
        }
        if (!parsed || parsedSize < 0) {
            logMessage(LogLevel::WARN, "xml", "Invalid <internalSpeakerSize> value '" + speakerSize + "' (" + parseError + ")");
            return false;
        }
    }

    return device.name.length() > 0;
}

bool Sonos::getXmlValue(const String& xml, const String& tag, String& value, const char* context, bool required) {
    SonosXmlParser::XmlLookupResult result = SonosXmlParser::findTagValue(xml, tag);
    if (result.success) {
        value = result.value;
        return true;
    }

    value = "";
    LogLevel level = required ? LogLevel::ERROR : LogLevel::WARN;
    String msg = "Lookup failed in " + String(context) + ": " + result.error;
    if (required || _config.enableVerboseLogging) {
        msg += " | payload=" + summarizeXml(xml);
    }
    logMessage(level, "xml", msg);
    return false;
}

bool Sonos::parseTimeToSeconds(const String& value, int& seconds, const char* context) {
    String parseError;
    if (SonosXmlParser::parseTimeToSeconds(value, seconds, parseError)) {
        return true;
    }

    logMessage(LogLevel::WARN, "xml", "Invalid time value in " + String(context) + ": '" + value + "' (" + parseError + ")");
    return false;
}

String Sonos::summarizeXml(const String& xml, int maxLen) {
    String compact = xml;
    compact.replace("\r", " ");
    compact.replace("\n", " ");
    compact.trim();
    if (compact.length() <= maxLen) return compact;
    return compact.substring(0, maxLen) + "...";
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
        logMessage(LogLevel::INFO, "control", "Volume set to " + String(volume) + " on " + deviceIP);
    }

    return result;
}

SonosResult Sonos::getVolume(const String& deviceIP, int& volume) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "RenderingControl", "GetVolume",
                                        VOLUME_GET_TEMPLATE, response);

    if (result == SonosResult::SUCCESS) {
        String volumeStr;
        if (getXmlValue(response, "CurrentVolume", volumeStr, "GetVolume response", true)) {
            String parseError;
            bool parsed = SonosXmlParser::parseInt(volumeStr, volume, parseError);
            if (parsed && (volume < 0 || volume > 100)) {
                parseError = "out of expected range 0..100";
            }
            if (!parsed || volume < 0 || volume > 100) {
                logMessage(LogLevel::ERROR, "xml", "Invalid <CurrentVolume> value '" + volumeStr + "' (" + parseError + ")");
                return SonosResult::ERROR_SOAP_FAULT;
            }
            logMessage(LogLevel::DEBUG, "control", "Current volume: " + String(volume) + " on " + deviceIP);
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

SonosResult Sonos::setMute(const String& deviceIP, bool mute) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    char body[200];
    snprintf(body, sizeof(body), MUTE_SET_TEMPLATE, mute ? 1 : 0);

    String response;
    return sendSoapRequest(deviceIP, "RenderingControl", "SetMute", body, response);
}

SonosResult Sonos::play(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;
    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Play", TRANSPORT_PLAY_TEMPLATE, response);
    if (result == SonosResult::SUCCESS) logMessage(LogLevel::INFO, "control", "Play command sent to " + deviceIP);
    return result;
}

SonosResult Sonos::pause(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;
    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Pause", TRANSPORT_PAUSE_TEMPLATE, response);
    if (result == SonosResult::SUCCESS) logMessage(LogLevel::INFO, "control", "Pause command sent to " + deviceIP);
    return result;
}

SonosResult Sonos::stop(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;
    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Stop", TRANSPORT_STOP_TEMPLATE, response);
    if (result == SonosResult::SUCCESS) logMessage(LogLevel::INFO, "control", "Stop command sent to " + deviceIP);
    return result;
}

SonosResult Sonos::next(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;
    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Next", TRANSPORT_NEXT_TEMPLATE, response);
    if (result == SonosResult::SUCCESS) logMessage(LogLevel::INFO, "control", "Next command sent to " + deviceIP);
    return result;
}

SonosResult Sonos::previous(const String& deviceIP) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;
    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "Previous", TRANSPORT_PREVIOUS_TEMPLATE, response);
    if (result == SonosResult::SUCCESS) logMessage(LogLevel::INFO, "control", "Previous command sent to " + deviceIP);
    return result;
}

SonosResult Sonos::sendSoapRequest(const String& deviceIP, const String& service,
                                  const String& action, const String& body, String& response) {
    if (!isValidIP(deviceIP)) return SonosResult::ERROR_INVALID_PARAM;

    String soapBody = formatSoapRequest(service, action, body);
    String url = "http://" + deviceIP + ":1400/MediaRenderer/" + service + "/Control";

    if (_config.enableVerboseLogging) {
        logMessage(LogLevel::DEBUG, "soap", "REQUEST url=" + url + " action=" + action);
        logMessage(LogLevel::DEBUG, "soap", "REQUEST body=" + summarizeXml(soapBody, 600));
    }

    _http.begin(url);
    _http.addHeader("Content-Type", "text/xml; charset=utf-8");
    _http.addHeader("SOAPAction", "\"urn:schemas-upnp-org:service:" + service + ":1#" + action + "\"");

    int httpCode = -1;
    for (int retry = 0; retry < _config.maxRetries && httpCode != HTTP_CODE_OK; retry++) {
        httpCode = _http.POST(soapBody);
        if (httpCode != HTTP_CODE_OK) delay(100 * (retry + 1));
    }

    if (httpCode == HTTP_CODE_OK) {
        response = _http.getString();
        if (_config.enableVerboseLogging) {
            logMessage(LogLevel::DEBUG, "soap", "RESPONSE body=" + summarizeXml(response, 600));
        }
        _http.end();
        return SonosResult::SUCCESS;
    } else if (httpCode == HTTP_CODE_INTERNAL_SERVER_ERROR) {
        response = _http.getString();
        if (_config.enableVerboseLogging) {
            logMessage(LogLevel::WARN, "soap", "ERROR RESPONSE body=" + summarizeXml(response, 600));
        }
        _http.end();
        return SonosResult::ERROR_SOAP_FAULT;
    } else {
        _http.end();
        logMessage(LogLevel::ERROR, "soap", "HTTP error: " + String(httpCode) + " for " + url);
        return SonosResult::ERROR_NETWORK;
    }
}

String Sonos::formatSoapRequest(const String& service, const String& action, const String& body) {
    char envelope[2048];
    snprintf(envelope, sizeof(envelope), SOAP_ENVELOPE_TEMPLATE, body.c_str());
    return String(envelope);
}

bool Sonos::isValidIP(const String& ip) {
    IPAddress addr;
    return addr.fromString(ip);
}

void Sonos::logMessage(LogLevel level, const char* channel, const String& message) {
    if (_config.enableLogging) {
        AppLogger::log(level, channel, message);
        if (_logCallback) {
            _logCallback(String("[") + AppLogger::levelToString(level) + "] [" + channel + "] " + message);
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

SonosResult Sonos::getPlaybackStatus(const String& deviceIP, PlaybackStatus& status) {
    if (!_initialized) return SonosResult::ERROR_INVALID_DEVICE;

    String response;
    SonosResult result = sendSoapRequest(deviceIP, "AVTransport", "GetPositionInfo",
                                        GET_POSITION_INFO_TEMPLATE, response);
    if (result != SonosResult::SUCCESS) return result;

    String trackUri;
    getXmlValue(response, "TrackURI", trackUri, "GetPositionInfo response", false);
    
    // Check for redirection (slave speaker in a group)
    if (trackUri.startsWith("x-rincon:")) {
        String masterUuid = trackUri.substring(9);
        String masterIP = "";
        for (const auto& dev : _devices) {
            if (dev.uuid.indexOf(masterUuid) != -1) {
                masterIP = dev.ip;
                break;
            }
        }
        
        if (masterIP.length() > 0 && masterIP != deviceIP) {
            logMessage(LogLevel::DEBUG, "playback", "Redirecting AVTransport requests to master: " + masterIP);
            // Fetch everything from the master
            SonosResult res = getPlaybackStatus(masterIP, status);
            return res;
        }
    }

    // If we're here, we're at the coordinator or a standalone speaker
    // 1. Parse metadata from the GetPositionInfo response we already have
    parsePositionInfo(deviceIP, response, status);

    // 2. Fetch Playback State (separate action but same service)
    String transportResponse;
    if (sendSoapRequest(deviceIP, "AVTransport", "GetTransportInfo",
                        GET_TRANSPORT_INFO_TEMPLATE, transportResponse) == SonosResult::SUCCESS) {
        getXmlValue(transportResponse, "CurrentTransportState", status.state, "GetTransportInfo response", true);
    }

    return SonosResult::SUCCESS;
}

void Sonos::parsePositionInfo(const String& deviceIP, const String& response, PlaybackStatus& status) {
    String metadata;
    getXmlValue(response, "TrackMetaData", metadata, "GetPositionInfo response", false);
    
    status.title = "";
    status.artist = "";
    status.album = "";
    status.albumArtUrl = "";

    if (metadata.length() > 0 && metadata != "NOT_IMPLEMENTED") {
        getXmlValue(metadata, "dc:title", status.title, "TrackMetaData", false);
        getXmlValue(metadata, "dc:creator", status.artist, "TrackMetaData", false);
        getXmlValue(metadata, "upnp:album", status.album, "TrackMetaData", false);
        getXmlValue(metadata, "upnp:albumArtURI", status.albumArtUrl, "TrackMetaData", false);

        // Handle poor metadata (radio streams)
        bool isPoorTitle = (status.title.length() == 0 || status.title.startsWith("http") || 
                           status.title.indexOf(".mp3") != -1 || status.title.indexOf(".m4a") != -1 ||
                           status.title.indexOf("multi_bump") != -1);

        if (isPoorTitle) {
            String streamContent;
            getXmlValue(metadata, "r:streamContent", streamContent, "TrackMetaData", false);
            if (streamContent.length() > 0) {
                int dashPos = streamContent.indexOf(" - ");
                if (dashPos != -1) {
                    status.artist = streamContent.substring(0, dashPos);
                    status.title = streamContent.substring(dashPos + 3);
                } else {
                    status.title = streamContent;
                }
            }
        }

        // Fallback to Station Name from GetMediaInfo if needed
        isPoorTitle = (status.title.length() == 0 || status.title.startsWith("http") || 
                       status.title.indexOf(".mp3") != -1 || status.title.indexOf(".m4a") != -1 ||
                       status.title.indexOf("multi_bump") != -1);

        if (isPoorTitle) {
            String mediaResponse;
            if (sendSoapRequest(deviceIP, "AVTransport", "GetMediaInfo", 
                                GET_MEDIA_INFO_TEMPLATE, mediaResponse) == SonosResult::SUCCESS) {
                String stationMeta;
                if (getXmlValue(mediaResponse, "CurrentURIMetaData", stationMeta, "GetMediaInfo response", false)) {
                    String stationTitle;
                    if (getXmlValue(stationMeta, "dc:title", stationTitle, "StationMetaData", false)) {
                        status.title = stationTitle;
                    }
                    if (status.albumArtUrl.length() == 0) {
                        getXmlValue(stationMeta, "upnp:albumArtURI", status.albumArtUrl, "StationMetaData", false);
                    }
                }
            }
        }

        if (status.albumArtUrl.length() > 0) {
            status.albumArtUrl.replace("&amp;", "&");
            if (status.albumArtUrl.startsWith("/")) {
                status.albumArtUrl = "http://" + deviceIP + ":1400" + status.albumArtUrl;
            }
        }
    }

    if (status.title.length() == 0) status.title = "Unknown Title";
    if (status.artist.length() == 0) status.artist = "Unknown Artist";

    // Position & Duration
    String relTime, durationStr;
    getXmlValue(response, "RelTime", relTime, "GetPositionInfo response", false);
    getXmlValue(response, "TrackDuration", durationStr, "GetPositionInfo response", false);
    
    status.position = 0;
    status.duration = 0;
    if (relTime.length() > 0 && relTime != "NOT_IMPLEMENTED") parseTimeToSeconds(relTime, status.position, "RelTime");
    if (durationStr.length() > 0 && durationStr != "NOT_IMPLEMENTED") parseTimeToSeconds(durationStr, status.duration, "TrackDuration");
}
