#include "SonosController.h"

SonosController::SonosController(Sonos& sonos) : _sonos(sonos) {
    _currentTrack.position = 0;
    _currentTrack.duration = 0;
    _currentTrack.volume = 0;
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

