#pragma once
#include <Arduino.h>
#include <Sonos.h>

struct TrackData {
    String title;
    String artist;
    String album;
    int position;
    int duration;
    String playbackState;
};

class SonosController {
public:
    SonosController(Sonos& sonos);
    
    bool update(const String& ip);
    const TrackData& getTrackData() const { return _currentTrack; }
    
    void play(const String& ip);
    void pause(const String& ip);
    void next(const String& ip);
    void previous(const String& ip);
    void setVolume(const String& ip, int volume);
    
private:
    Sonos& _sonos;
    TrackData _currentTrack;
};
