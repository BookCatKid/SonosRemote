#include "SonosController.h"
#include "SonosXmlParser.h"
#include "AppLogger.h"

SonosController::SonosController(Sonos& sonos) : _sonos(sonos) {
    _currentTrack.position = 0;
    _currentTrack.duration = 0;
    _currentTrack.volume = 0;
    _lastTickMs = millis();
    _positionRemainderMs = 0;
}

bool SonosController::update(const String& ip) {
    if (!_sonos.isInitialized()) {
        _sonos.begin();
    }

    Sonos::PlaybackStatus status;
    SonosResult res = _sonos.getPlaybackStatus(ip, status);

    if (res == SonosResult::SUCCESS) {
        _currentTrack.title = status.title;
        _currentTrack.artist = status.artist;
        _currentTrack.album = status.album;
        _currentTrack.albumArtUrl = status.albumArtUrl;
        _currentTrack.position = status.position;
        _currentTrack.duration = status.duration;
        _currentTrack.playbackState = status.state;

        // Only update volume if not in user interaction lockout
        if (millis() - _lastVolumeUserChangeMs > 2000) {
            int vol;
            if (_sonos.getVolume(ip, vol) == SonosResult::SUCCESS) {
                _currentTrack.volume = vol;
            }
        }

        _lastTickMs = millis();
        _positionRemainderMs = 0;
        return true;
    }

    return false;
}

bool SonosController::refreshPosition(const String& ip, bool refreshDuration) {
    if (!_sonos.isInitialized()) {
        _sonos.begin();
    }

    Sonos::PlaybackStatus status;
    SonosResult res = _sonos.getPlaybackStatus(ip, status);
    if (res != SonosResult::SUCCESS) {
        unsigned long nowMs = millis();
        if (_lastStatusSyncWarnMs == 0 || nowMs - _lastStatusSyncWarnMs >= 15000) {
            LOG_WARN("control", "Status sync failed: " + _sonos.getErrorString(res));
            _lastStatusSyncWarnMs = nowMs;
        } else {
            LOG_DEBUG("control", "Status sync failed (suppressed): " + _sonos.getErrorString(res));
        }
        _lastTickMs = millis(); // Still update tick so local interpolation continues
        return false;
    }

    bool hadTrackData = _currentTrack.title.length() > 0 || _currentTrack.artist.length() > 0;
    bool trackMetadataChanged = hadTrackData &&
        (status.title != _currentTrack.title ||
         status.artist != _currentTrack.artist ||
         status.album != _currentTrack.album ||
         status.albumArtUrl != _currentTrack.albumArtUrl);

    // Keep metadata fresh during periodic syncs so missed events don't leave stale tracks on screen.
    _currentTrack.title = status.title;
    _currentTrack.artist = status.artist;
    _currentTrack.album = status.album;
    _currentTrack.albumArtUrl = status.albumArtUrl;

    _currentTrack.position = status.position;
    if (refreshDuration && status.duration > 0) {
        _currentTrack.duration = status.duration;
    }
    _currentTrack.playbackState = status.state;
    _positionRemainderMs = 0;
    _lastTickMs = millis();
    if (trackMetadataChanged) {
        LOG_INFO("control", "Recovered track metadata via polling after a missed/late event");
    }
    LOG_DEBUG("control", "Synced position and state via getPlaybackStatus");
    return true;
}

void SonosController::play(const String& ip) {
    _sonos.play(ip);
}

void SonosController::pause(const String& ip) {
    _sonos.pause(ip);
}

void SonosController::togglePlayPause(const String& ip) {
    if (_currentTrack.playbackState == "PLAYING" || _currentTrack.playbackState == "TRANSITIONING") {
        _sonos.pause(ip);
    } else {
        _sonos.play(ip);
    }
}

void SonosController::next(const String& ip) {
    _sonos.next(ip);
}

void SonosController::previous(const String& ip) {
    _sonos.previous(ip);
}

void SonosController::setVolume(const String& ip, int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    int oldVol = _currentTrack.volume;
    _currentTrack.volume = volume;
    _lastVolumeUserChangeMs = millis();

    if (_sonos.setVolume(ip, volume) != SonosResult::SUCCESS) {
        LOG_WARN("control", "Failed to set volume, rolling back");
        _currentTrack.volume = oldVol;
        // Reset lockout so the next poll/event can correct the UI immediately
        _lastVolumeUserChangeMs = 0;
    }
}

void SonosController::volumeUp(const String& ip) {
    int nextVol = _currentTrack.volume + 5;
    if (nextVol > 100) nextVol = 100;
    setVolume(ip, nextVol);
}

void SonosController::volumeDown(const String& ip) {
    int nextVol = _currentTrack.volume - 5;
    if (nextVol < 0) nextVol = 0;
    setVolume(ip, nextVol);
}

static bool isPlayingState(const String& state) {
    return state == "PLAYING" || state == "TRANSITIONING";
}

void SonosController::tick() {
    unsigned long now = millis();
    if (_lastTickMs == 0) {
        _lastTickMs = now;
        return;
    }

    unsigned long elapsedMs = now - _lastTickMs;
    _lastTickMs = now;

    if (!isPlayingState(_currentTrack.playbackState)) {
        return;
    }

    _positionRemainderMs += elapsedMs;
    if (_positionRemainderMs < 1000) {
        return;
    }

    int deltaSeconds = static_cast<int>(_positionRemainderMs / 1000);
    _positionRemainderMs %= 1000;
    _currentTrack.position += deltaSeconds;

    if (_currentTrack.duration > 0 && _currentTrack.position > _currentTrack.duration) {
        _currentTrack.position = _currentTrack.duration;
    }
}

static String extractVal(const String& xml, const String& tag) {
    SonosXmlParser::XmlLookupResult result = SonosXmlParser::findAttributeValue(xml, tag, "val");
    return result.success ? result.value : "";
}

static String extractValOrTag(const String& xml, const String& tag) {
    String value = extractVal(xml, tag);
    if (value.length() > 0) return value;
    SonosXmlParser::XmlLookupResult result = SonosXmlParser::findTagValue(xml, tag);
    return result.success ? result.value : "";
}

void SonosController::parseEvent(const String& xml) {
    LOG_DEBUG("control", "Event received (bytes=" + String(xml.length()) + ")");

    // 1. Extract LastChange payload. Parser decoding handles one pass safely.
    String lastChange;
    SonosXmlParser::XmlLookupResult lastChangeResult = SonosXmlParser::findTagValue(xml, "LastChange");
    if (lastChangeResult.success) {
        lastChange = lastChangeResult.value;
    } else if (xml.indexOf("<Event") != -1) {
        lastChange = xml;
    } else {
        LOG_WARN("control", "Event missing LastChange payload");
        return;
    }

    // 2. Playback state
    String state = extractValOrTag(lastChange, "TransportState");
    if (state.length()) _currentTrack.playbackState = state;

    // 3. Volume (specific handling for Master channel)
    if (millis() - _lastVolumeUserChangeMs > 2000) {
        int volIdx = lastChange.indexOf("Volume channel=\"Master\"");
        if (volIdx != -1) {
            int valPos = lastChange.indexOf("val=\"", volIdx);
            if (valPos != -1) {
                valPos += 5;
                int endQuote = lastChange.indexOf("\"", valPos);
                if (endQuote != -1) {
                    _currentTrack.volume = lastChange.substring(valPos, endQuote).toInt();
                }
            }
        } else {
            String vol = extractVal(lastChange, "Volume");
            if (vol.length()) _currentTrack.volume = vol.toInt();
        }
    }

    // 4. Position and duration
    String relTime = extractValOrTag(lastChange, "RelativeTime");
    if (!relTime.length()) relTime = extractValOrTag(lastChange, "RelTime");
    if (!relTime.length()) relTime = extractValOrTag(lastChange, "RelativeTimePosition");
    if (!relTime.length()) relTime = extractValOrTag(lastChange, "AbsTime");
    if (relTime.length() && relTime != "NOT_IMPLEMENTED") {
        int seconds;
        String error;
        if (SonosXmlParser::parseTimeToSeconds(relTime, seconds, error)) {
            _currentTrack.position = seconds;
            _positionRemainderMs = 0;
            LOG_DEBUG("control", "Parsed position: " + relTime);
        } else {
            LOG_WARN("control", "Invalid event position value '" + relTime + "' (" + error + ")");
        }
    }

    String durationStr = extractValOrTag(lastChange, "CurrentTrackDuration");
    if (!durationStr.length()) durationStr = extractValOrTag(lastChange, "Duration");
    if (!durationStr.length()) durationStr = extractValOrTag(lastChange, "TrackDuration");
    if (!durationStr.length()) durationStr = extractValOrTag(lastChange, "CurrentMediaDuration");
    if (durationStr.length() && durationStr != "NOT_IMPLEMENTED") {
        int seconds;
        String error;
        if (SonosXmlParser::parseTimeToSeconds(durationStr, seconds, error)) {
            _currentTrack.duration = seconds;
        } else {
            LOG_WARN("control", "Invalid event duration value '" + durationStr + "' (" + error + ")");
        }
    }

    // 5. Metadata (title, artist, album, art)
    auto getTag = [](const String& m, const String& t) {
        SonosXmlParser::XmlLookupResult r = SonosXmlParser::findTagValue(m, t);
        return r.success ? r.value : "";
    };

    String meta = extractVal(lastChange, "CurrentTrackMetaData");
    String stationMeta = extractVal(lastChange, "AVTransportURIMetaData");

    if (meta.length()) {
        String t = getTag(meta, "title");
        if (!t.length()) t = getTag(meta, "dc:title");

        String a = getTag(meta, "creator");
        if (!a.length()) a = getTag(meta, "dc:creator");

        String alb = getTag(meta, "album");
        if (!alb.length()) alb = getTag(meta, "upnp:album");

        String art = getTag(meta, "albumArtURI");
        if (!art.length()) art = getTag(meta, "upnp:albumArtURI");

        // Use station metadata if track metadata is poor (radio streams)
        bool isPoorTitle = (!t.length() || t.startsWith("http") ||
                           t.indexOf(".mp3") != -1 || t.indexOf(".m4a") != -1 ||
                           t.indexOf("multi_bump") != -1);

        if (isPoorTitle && stationMeta.length()) {
            String st = getTag(stationMeta, "title");
            if (!st.length()) st = getTag(stationMeta, "dc:title");
            if (st.length()) t = st;

            String sart = getTag(stationMeta, "albumArtURI");
            if (!sart.length()) sart = getTag(stationMeta, "upnp:albumArtURI");
            if (sart.length()) art = sart;
        }

        if (t.length()) {
            if (t != _currentTrack.title) {
                _currentTrack.position = 0;
                _positionRemainderMs = 0;
            }
            _currentTrack.title = t;
            LOG_DEBUG("control", "Parsed title: " + t);
        }
        if (a.length()) _currentTrack.artist = a;
        if (alb.length()) _currentTrack.album = alb;
        if (art.length()) {
            art.replace("&amp;", "&");
            _currentTrack.albumArtUrl = art;
        }
    } else if (stationMeta.length()) {
        // Fallback for radio streams with no CurrentTrackMetaData
        String t = getTag(stationMeta, "title");
        if (!t.length()) t = getTag(stationMeta, "dc:title");
        if (t.length()) _currentTrack.title = t;

        String art = getTag(stationMeta, "albumArtURI");
        if (!art.length()) art = getTag(stationMeta, "upnp:albumArtURI");
        if (art.length()) {
            art.replace("&amp;", "&");
            _currentTrack.albumArtUrl = art;
        }
    }
}
