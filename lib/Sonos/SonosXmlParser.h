#ifndef SONOS_XML_PARSER_H
#define SONOS_XML_PARSER_H

#include <Arduino.h>

namespace SonosXmlParser {

struct XmlLookupResult {
    bool success = false;
    String value;
    String error;
};

XmlLookupResult findTagValue(const String& xml, const String& tag);
bool parseTimeToSeconds(const String& value, int& seconds, String& error);
bool parseInt(const String& value, int& parsed, String& error);

}  // namespace SonosXmlParser

#endif
