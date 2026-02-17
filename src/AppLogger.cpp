#include "AppLogger.h"

namespace {
Stream* gSerial = &Serial;
bool gEnabled = true;
LogLevel gMinLevel = LogLevel::INFO;
std::vector<String> gAllowedChannels;
std::vector<String> gBlockedChannels;
}

void AppLogger::begin(Stream& serial, LogLevel minLevel) {
    gSerial = &serial;
    gEnabled = true;
    gMinLevel = minLevel;
}

void AppLogger::setEnabled(bool enabled) {
    gEnabled = enabled;
}

void AppLogger::setMinLevel(LogLevel minLevel) {
    gMinLevel = minLevel;
}

void AppLogger::clearAllowedChannels() {
    gAllowedChannels.clear();
}

void AppLogger::clearBlockedChannels() {
    gBlockedChannels.clear();
}

void AppLogger::allowChannel(const String& channel) {
    String normalized = normalizeChannel(channel);
    if (normalized.length() == 0) return;
    if (!channelInList(gAllowedChannels, normalized)) {
        gAllowedChannels.push_back(normalized);
    }
}

void AppLogger::blockChannel(const String& channel) {
    String normalized = normalizeChannel(channel);
    if (normalized.length() == 0) return;
    if (!channelInList(gBlockedChannels, normalized)) {
        gBlockedChannels.push_back(normalized);
    }
}

bool AppLogger::isChannelEnabled(const String& channel) {
    String normalized = normalizeChannel(channel);
    if (normalized.length() == 0) return false;

    if (channelInList(gBlockedChannels, normalized)) {
        return false;
    }

    if (gAllowedChannels.empty()) {
        return true;
    }

    return channelInList(gAllowedChannels, normalized);
}

void AppLogger::log(LogLevel level, const String& channel, const String& message) {
    if (!gEnabled || level < gMinLevel || level == LogLevel::OFF) return;
    if (!isChannelEnabled(channel)) return;
    if (gSerial == nullptr) return;

    String normalized = normalizeChannel(channel);
    gSerial->print('[');
    gSerial->print(millis());
    gSerial->print("] [");
    gSerial->print(levelToString(level));
    gSerial->print("] [");
    gSerial->print(normalized);
    gSerial->print("] ");
    gSerial->println(message);
}

const char* AppLogger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::OFF: return "OFF";
        default: return "UNKNOWN";
    }
}

String AppLogger::normalizeChannel(const String& channel) {
    String normalized = channel;
    normalized.trim();
    normalized.toLowerCase();
    return normalized;
}

bool AppLogger::channelInList(const std::vector<String>& list, const String& normalizedChannel) {
    for (const auto& item : list) {
        if (item == normalizedChannel) {
            return true;
        }
    }
    return false;
}
