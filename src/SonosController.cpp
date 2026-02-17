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

    String title, artist, album, albumArtUrl;
    int duration;
    SonosResult res = _sonos.getTrackInfo(ip, title, artist, album, albumArtUrl, duration);

    if (res == SonosResult::SUCCESS) {
        _currentTrack.title = title;
        _currentTrack.artist = artist;
        _currentTrack.album = album;
        _currentTrack.albumArtUrl = albumArtUrl;
        _currentTrack.duration = duration;

        int position;
        int dur2;
        _sonos.getPositionInfo(ip, position, dur2);
        _currentTrack.position = position;

        String state;
        _sonos.getPlaybackState(ip, state);
        _currentTrack.playbackState = state;

        int vol;
        _sonos.getVolume(ip, vol);
        _currentTrack.volume = vol;

        return true;
    }

    return false;
}

bool SonosController::refreshPosition(const String& ip, bool refreshDuration) {
    if (!_sonos.isInitialized()) {
        _sonos.begin();
    }

    int position = 0;
    int duration = 0;
    SonosResult result = _sonos.getPositionInfo(ip, position, duration);
    if (result != SonosResult::SUCCESS) {
        LOG_WARN("control", "Position sync failed: " + _sonos.getErrorString(result));
        return false;
    }

    _currentTrack.position = position;
    if (refreshDuration && duration > 0) {
        _currentTrack.duration = duration;
    }
    _positionRemainderMs = 0;
    _lastTickMs = millis();
    LOG_DEBUG("control", "Synced position via getPositionInfo: " + String(position) + "s");
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
    _sonos.setVolume(ip, volume);
}

void SonosController::volumeUp(const String& ip) {
    _sonos.increaseVolume(ip, 5);
}

void SonosController::volumeDown(const String& ip) {
    _sonos.decreaseVolume(ip, 5);
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

static String multiUnescape(String s) {
    for (int i = 0; i < 3; i++) {
        String old = s;
        s.replace("&amp;", "&"); s.replace("&lt;", "<"); s.replace("&gt;", ">");
        s.replace("&quot;", "\""); s.replace("&apos;", "'");
        if (old == s) break;
    }
    return s;
}

void SonosController::parseEvent(const String& xml) {
    LOG_DEBUG("control", "Event received: " + xml);

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

    // 4. Position and duration
    String relTime = extractValOrTag(lastChange, "RelativeTime");
    if (!relTime.length()) relTime = extractValOrTag(lastChange, "RelTime");
    if (!relTime.length()) relTime = extractValOrTag(lastChange, "RelativeTimePosition");
    if (!relTime.length()) relTime = extractValOrTag(lastChange, "AbsTime");
    if (relTime.length()) {
        int seconds;
        String error;
        if (SonosXmlParser::parseTimeToSeconds(relTime, seconds, error)) {
            _currentTrack.position = seconds;
            _positionRemainderMs = 0;
            LOG_DEBUG("control", "Parsed position: " + relTime);
        } else {
            LOG_WARN("control", "Invalid event position value '" + relTime + "' (" + error + ")");
        }
    } else {
        LOG_DEBUG("control", "Event did not include position; continuing local clock");
    }

    String durationStr = extractValOrTag(lastChange, "CurrentTrackDuration");
    if (!durationStr.length()) durationStr = extractValOrTag(lastChange, "Duration");
    if (!durationStr.length()) durationStr = extractValOrTag(lastChange, "TrackDuration");
    if (!durationStr.length()) durationStr = extractValOrTag(lastChange, "CurrentMediaDuration");
    if (durationStr.length()) {
        int seconds;
        String error;
        if (SonosXmlParser::parseTimeToSeconds(durationStr, seconds, error)) {
            _currentTrack.duration = seconds;
        } else {
            LOG_WARN("control", "Invalid event duration value '" + durationStr + "' (" + error + ")");
        }
    }

    // 5. Metadata (title, artist, album, art)
    String meta = extractVal(lastChange, "CurrentTrackMetaData");
    if (meta.length()) {
        meta = multiUnescape(meta);
        
        auto getTag = [](const String& m, const String& t) {
            SonosXmlParser::XmlLookupResult r = SonosXmlParser::findTagValue(m, t);
            return r.success ? r.value : "";
        };

        String t = getTag(meta, "title");
        if (!t.length()) t = getTag(meta, "dc:title");
        
        String a = getTag(meta, "creator");
        if (!a.length()) a = getTag(meta, "dc:creator");
        
        String alb = getTag(meta, "album");
        if (!alb.length()) alb = getTag(meta, "upnp:album");
        
        String art = getTag(meta, "albumArtURI");
        if (!art.length()) art = getTag(meta, "upnp:albumArtURI");

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
        if (art.length()) _currentTrack.albumArtUrl = art;
    }
}
