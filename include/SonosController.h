#pragma once
#include <Arduino.h>
#include <Sonos.h>

struct TrackData {
    String title;
    String artist;
    String album;
    String albumArtUrl;
    int position;
    int duration;
    int volume;
    String playbackState;
};

class SonosController {
public:
    SonosController(Sonos& sonos);

    bool update(const String& ip);
    bool refreshPosition(const String& ip, bool refreshDuration = true);
    void tick();
    const TrackData& getTrackData() const { return _currentTrack; }

    void play(const String& ip);
    void pause(const String& ip);
    void togglePlayPause(const String& ip);
    void next(const String& ip);
    void previous(const String& ip);
    void setVolume(const String& ip, int volume);
    void volumeUp(const String& ip);
    void volumeDown(const String& ip);

    void parseEvent(const String& xml);

private:
    Sonos& _sonos;
    TrackData _currentTrack;
    unsigned long _lastTickMs = 0;
    unsigned long _positionRemainderMs = 0;
};
