#include "SonosXmlParser.h"
#include <ctype.h>

namespace SonosXmlParser {

namespace {

String localName(const String& name) {
    int colonPos = name.indexOf(':');
    return colonPos >= 0 ? name.substring(colonPos + 1) : name;
}

bool namesMatch(const String& xmlName, const String& expectedName) {
    if (xmlName.length() == 0 || expectedName.length() == 0) return false;
    if (xmlName == expectedName) return true;
    return localName(xmlName) == localName(expectedName);
}

bool isNameChar(char c) {
    return isalnum(static_cast<unsigned char>(c)) || c == ':' || c == '_' || c == '-' || c == '.';
}

bool parseTagAt(const String& xml, int openBracketPos, bool& isClosingTag, bool& isSelfClosingTag, String& tagName, int& tagEndPos, String& error) {
    if (openBracketPos < 0 || openBracketPos >= static_cast<int>(xml.length()) || xml[openBracketPos] != '<') {
        error = "Tag parse did not start at '<'";
        return false;
    }

    if (openBracketPos + 1 >= static_cast<int>(xml.length())) {
        error = "Truncated tag start";
        return false;
    }

    int cursor = openBracketPos + 1;
    isClosingTag = false;
    isSelfClosingTag = false;

    if (xml[cursor] == '/') {
        isClosingTag = true;
        cursor++;
    }

    if (cursor >= static_cast<int>(xml.length()) || !isNameChar(xml[cursor])) {
        error = "Tag name missing";
        return false;
    }

    int nameStart = cursor;
    while (cursor < static_cast<int>(xml.length()) && isNameChar(xml[cursor])) {
        cursor++;
    }
    tagName = xml.substring(nameStart, cursor);

    tagEndPos = xml.indexOf('>', cursor);
    if (tagEndPos == -1) {
        error = "Tag missing closing '>'";
        return false;
    }

    int selfClosingProbe = tagEndPos - 1;
    while (selfClosingProbe > openBracketPos && isspace(static_cast<unsigned char>(xml[selfClosingProbe]))) {
        selfClosingProbe--;
    }
    isSelfClosingTag = !isClosingTag && selfClosingProbe > openBracketPos && xml[selfClosingProbe] == '/';

    return true;
}

bool skipSpecialSection(const String& xml, int openBracketPos, int& nextPos, String& error) {
    if (xml.startsWith("<?", openBracketPos)) {
        int end = xml.indexOf("?>", openBracketPos + 2);
        if (end == -1) {
            error = "Unclosed XML declaration or processing instruction";
            return false;
        }
        nextPos = end + 2;
        return true;
    }

    if (xml.startsWith("<!--", openBracketPos)) {
        int end = xml.indexOf("-->", openBracketPos + 4);
        if (end == -1) {
            error = "Unclosed XML comment";
            return false;
        }
        nextPos = end + 3;
        return true;
    }

    if (xml.startsWith("<![CDATA[", openBracketPos)) {
        int end = xml.indexOf("]]>", openBracketPos + 9);
        if (end == -1) {
            error = "Unclosed CDATA section";
            return false;
        }
        nextPos = end + 3;
        return true;
    }

    if (xml.startsWith("<!", openBracketPos)) {
        int end = xml.indexOf('>', openBracketPos + 2);
        if (end == -1) {
            error = "Unclosed XML declaration";
            return false;
        }
        nextPos = end + 1;
        return true;
    }

    return false;
}

int parseEntityCodePoint(const String& entity, bool& ok) {
    ok = false;
    if (entity.length() == 0) return 0;

    int base = 10;
    int start = 0;
    if (entity[0] == '#') {
        start = 1;
        if (start < static_cast<int>(entity.length()) && (entity[start] == 'x' || entity[start] == 'X')) {
            base = 16;
            start++;
        }
    } else {
        return 0;
    }

    long value = 0;
    for (int i = start; i < static_cast<int>(entity.length()); i++) {
        char c = entity[i];
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
        else if (base == 16 && c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
        else return 0;
        value = value * base + digit;
        if (value > 0x10FFFF) return 0;
    }
    ok = true;
    return static_cast<int>(value);
}

String decodeEntities(const String& input) {
    String decoded;
    decoded.reserve(input.length());

    for (int i = 0; i < static_cast<int>(input.length()); i++) {
        if (input[i] != '&') {
            decoded += input[i];
            continue;
        }

        int semicolonPos = input.indexOf(';', i + 1);
        if (semicolonPos == -1) {
            decoded += input[i];
            continue;
        }

        String entity = input.substring(i + 1, semicolonPos);
        if (entity == "amp") decoded += '&';
        else if (entity == "lt") decoded += '<';
        else if (entity == "gt") decoded += '>';
        else if (entity == "quot") decoded += '"';
        else if (entity == "apos") decoded += '\'';
        else {
            bool ok = false;
            int codePoint = parseEntityCodePoint(entity, ok);
            if (ok) {
                if (codePoint <= 0x7F) {
                    decoded += static_cast<char>(codePoint);
                } else if (codePoint <= 0x7FF) {
                    decoded += static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F));
                    decoded += static_cast<char>(0x80 | (codePoint & 0x3F));
                } else if (codePoint <= 0xFFFF) {
                    decoded += static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F));
                    decoded += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                    decoded += static_cast<char>(0x80 | (codePoint & 0x3F));
                } else {
                    decoded += static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07));
                    decoded += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                    decoded += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                    decoded += static_cast<char>(0x80 | (codePoint & 0x3F));
                }
            } else {
                decoded += '&';
                decoded += entity;
                decoded += ';';
            }
        }

        i = semicolonPos;
    }

    return decoded;
}

}  // namespace

XmlLookupResult findTagValue(const String& xml, const String& tag) {
    XmlLookupResult result;
    if (xml.length() == 0) {
        result.error = "XML payload is empty";
        return result;
    }
    if (tag.length() == 0) {
        result.error = "Requested tag is empty";
        return result;
    }

    int scanPos = 0;
    while (scanPos < static_cast<int>(xml.length())) {
        int openBracketPos = xml.indexOf('<', scanPos);
        if (openBracketPos == -1) break;

        int nextPos = openBracketPos + 1;
        String specialError;
        if (skipSpecialSection(xml, openBracketPos, nextPos, specialError)) {
            scanPos = nextPos;
            continue;
        }
        if (specialError.length() > 0) {
            result.error = specialError;
            return result;
        }

        bool isClosingTag = false;
        bool isSelfClosingTag = false;
        String tagName;
        int tagEndPos = -1;
        String parseError;
        if (!parseTagAt(xml, openBracketPos, isClosingTag, isSelfClosingTag, tagName, tagEndPos, parseError)) {
            scanPos = openBracketPos + 1;
            continue;
        }

        if (!isClosingTag && namesMatch(tagName, tag)) {
            if (isSelfClosingTag) {
                result.success = true;
                result.value = "";
                return result;
            }

            int contentStart = tagEndPos + 1;
            int innerScanPos = contentStart;
            int nestedDepth = 0;
            while (innerScanPos < static_cast<int>(xml.length())) {
                int innerOpenPos = xml.indexOf('<', innerScanPos);
                if (innerOpenPos == -1) {
                    result.error = "Tag <" + tag + "> has no closing element";
                    return result;
                }

                int innerNextPos = innerOpenPos + 1;
                String innerSpecialError;
                if (skipSpecialSection(xml, innerOpenPos, innerNextPos, innerSpecialError)) {
                    innerScanPos = innerNextPos;
                    continue;
                }
                if (innerSpecialError.length() > 0) {
                    result.error = innerSpecialError;
                    return result;
                }

                bool innerIsClosing = false;
                bool innerIsSelfClosing = false;
                String innerTagName;
                int innerTagEndPos = -1;
                String innerParseError;
                if (!parseTagAt(xml, innerOpenPos, innerIsClosing, innerIsSelfClosing, innerTagName, innerTagEndPos, innerParseError)) {
                    innerScanPos = innerOpenPos + 1;
                    continue;
                }

                if (namesMatch(innerTagName, tag)) {
                    if (innerIsClosing) {
                        if (nestedDepth == 0) {
                            String rawValue = xml.substring(contentStart, innerOpenPos);
                            rawValue.trim();
                            result.success = true;
                            result.value = decodeEntities(rawValue);
                            return result;
                        }
                        nestedDepth--;
                    } else if (!innerIsSelfClosing) {
                        nestedDepth++;
                    }
                }

                innerScanPos = innerTagEndPos + 1;
            }

            result.error = "Tag <" + tag + "> has no closing element";
            return result;
        }

        scanPos = tagEndPos + 1;
    }

    result.error = "Tag <" + tag + "> not found";
    return result;
}

bool parseTimeToSeconds(const String& value, int& seconds, String& error) {
    seconds = 0;
    String input = value;
    input.trim();
    if (input.length() == 0) {
        error = "Time value is empty";
        return false;
    }

    int firstColon = input.indexOf(':');
    int lastColon = input.lastIndexOf(':');
    if (firstColon == -1) {
        error = "Time value must contain ':'";
        return false;
    }

    auto parsePart = [](const String& part, int& parsed) -> bool {
        if (part.length() == 0) return false;
        for (int i = 0; i < static_cast<int>(part.length()); i++) {
            if (!isdigit(static_cast<unsigned char>(part[i]))) return false;
        }
        parsed = part.toInt();
        return true;
    };

    int h = 0;
    int m = 0;
    int s = 0;

    if (firstColon == lastColon) {
        if (!parsePart(input.substring(0, firstColon), m) || !parsePart(input.substring(firstColon + 1), s)) {
            error = "Invalid MM:SS time value";
            return false;
        }
        if (s >= 60) {
            error = "Seconds out of range in MM:SS";
            return false;
        }
        seconds = m * 60 + s;
        return true;
    }

    if (!parsePart(input.substring(0, firstColon), h) ||
        !parsePart(input.substring(firstColon + 1, lastColon), m) ||
        !parsePart(input.substring(lastColon + 1), s)) {
        error = "Invalid HH:MM:SS time value";
        return false;
    }

    if (m >= 60 || s >= 60) {
        error = "Minutes or seconds out of range in HH:MM:SS";
        return false;
    }

    seconds = h * 3600 + m * 60 + s;
    return true;
}

bool parseInt(const String& value, int& parsed, String& error) {
    parsed = 0;
    String input = value;
    input.trim();
    if (input.length() == 0) {
        error = "Integer value is empty";
        return false;
    }

    int start = 0;
    bool negative = false;
    if (input[0] == '+' || input[0] == '-') {
        negative = input[0] == '-';
        start = 1;
    }
    if (start >= static_cast<int>(input.length())) {
        error = "Integer value has no digits";
        return false;
    }

    long valueParsed = 0;
    for (int i = start; i < static_cast<int>(input.length()); i++) {
        if (!isdigit(static_cast<unsigned char>(input[i]))) {
            error = "Integer value contains non-digit characters";
            return false;
        }
        valueParsed = (valueParsed * 10) + (input[i] - '0');
        if (valueParsed > 2147483647L) {
            error = "Integer value out of range";
            return false;
        }
    }

    parsed = negative ? -static_cast<int>(valueParsed) : static_cast<int>(valueParsed);
    return true;
}

}  // namespace SonosXmlParser
