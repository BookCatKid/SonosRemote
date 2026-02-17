#ifndef APP_LOGGER_H
#define APP_LOGGER_H

#include <Arduino.h>
#include <vector>

enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    OFF = 4
};

class AppLogger {
public:
    static void begin(Stream& serial = Serial, LogLevel minLevel = LogLevel::INFO);
    static void setEnabled(bool enabled);
    static void setMinLevel(LogLevel minLevel);

    static void clearAllowedChannels();
    static void clearBlockedChannels();
    static void allowChannel(const String& channel);
    static void blockChannel(const String& channel);

    static bool isChannelEnabled(const String& channel);
    static void log(LogLevel level, const String& channel, const String& message);
    static const char* levelToString(LogLevel level);

private:
    static String normalizeChannel(const String& channel);
    static bool channelInList(const std::vector<String>& list, const String& normalizedChannel);
};

#define LOG_DEBUG(channel, message) AppLogger::log(LogLevel::DEBUG, channel, message)
#define LOG_INFO(channel, message) AppLogger::log(LogLevel::INFO, channel, message)
#define LOG_WARN(channel, message) AppLogger::log(LogLevel::WARN, channel, message)
#define LOG_ERROR(channel, message) AppLogger::log(LogLevel::ERROR, channel, message)

#endif
