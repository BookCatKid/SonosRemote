#include "SonosController.h"

SonosController::SonosController(Sonos& sonos) : _sonos(sonos) {
    _currentTrack.position = 0;
    _currentTrack.duration = 0;
}

bool SonosController::update(const String& ip) {
    if (!_sonos.isInitialized()) {
        _sonos.begin();
    }
    
    String title, artist, album;
    int duration;
    SonosResult res = _sonos.getTrackInfo(ip, title, artist, album, duration);
    
    if (res == SonosResult::SUCCESS) {
        _currentTrack.title = title;
        _currentTrack.artist = artist;
        _currentTrack.album = album;
        _currentTrack.duration = duration;
        
        int position;
        int dur2;
        _sonos.getPositionInfo(ip, position, dur2);
        _currentTrack.position = position;
        
        String state;
        _sonos.getPlaybackState(ip, state);
        _currentTrack.playbackState = state;
        
        return true;
    }
    
    return false;
}

void SonosController::play(const String& ip) {
    _sonos.play(ip);
}

void SonosController::pause(const String& ip) {
    _sonos.pause(ip);
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
