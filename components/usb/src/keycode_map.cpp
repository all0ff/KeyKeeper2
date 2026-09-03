#include "usb/keycode_map.hpp"

namespace usb {

KeyMapping ascii_to_hid(char c)
{
    // Lowercase letters a-z
    if (c >= 'a' && c <= 'z') {
        return { static_cast<uint8_t>(0x04 + (c - 'a')), modifier::NONE };
    }
    // Uppercase letters A-Z
    if (c >= 'A' && c <= 'Z') {
        return { static_cast<uint8_t>(0x04 + (c - 'A')), modifier::LEFT_SHIFT };
    }

    // Digits and shifted symbols
    switch (c) {
        case '1':  return { 0x1E, modifier::NONE };
        case '!':  return { 0x1E, modifier::LEFT_SHIFT };
        case '2':  return { 0x1F, modifier::NONE };
        case '@':  return { 0x1F, modifier::LEFT_SHIFT };
        case '3':  return { 0x20, modifier::NONE };
        case '#':  return { 0x20, modifier::LEFT_SHIFT };
        case '4':  return { 0x21, modifier::NONE };
        case '$':  return { 0x21, modifier::LEFT_SHIFT };
        case '5':  return { 0x22, modifier::NONE };
        case '%':  return { 0x22, modifier::LEFT_SHIFT };
        case '6':  return { 0x23, modifier::NONE };
        case '^':  return { 0x23, modifier::LEFT_SHIFT };
        case '7':  return { 0x24, modifier::NONE };
        case '&':  return { 0x24, modifier::LEFT_SHIFT };
        case '8':  return { 0x25, modifier::NONE };
        case '*':  return { 0x25, modifier::LEFT_SHIFT };
        case '9':  return { 0x26, modifier::NONE };
        case '(':  return { 0x26, modifier::LEFT_SHIFT };
        case '0':  return { 0x27, modifier::NONE };
        case ')':  return { 0x27, modifier::LEFT_SHIFT };

        case '\n':  return { keycode::ENTER,     modifier::NONE };
        case '\r':  return { keycode::ENTER,     modifier::NONE };
        case '\t':  return { keycode::TAB,       modifier::NONE };
        case ' ':  return { keycode::SPACE,     modifier::NONE };

        case '-':  return { 0x2D, modifier::NONE };
        case '_':  return { 0x2D, modifier::LEFT_SHIFT };
        case '=':  return { 0x2E, modifier::NONE };
        case '+':  return { 0x2E, modifier::LEFT_SHIFT };
        case '[':  return { 0x2F, modifier::NONE };
        case '{':  return { 0x2F, modifier::LEFT_SHIFT };
        case ']':  return { 0x30, modifier::NONE };
        case '}':  return { 0x30, modifier::LEFT_SHIFT };
        case '\\': return { 0x31, modifier::NONE };
        case '|':  return { 0x31, modifier::LEFT_SHIFT };
        case ';':  return { 0x33, modifier::NONE };
        case ':':  return { 0x33, modifier::LEFT_SHIFT };
        case '\'': return { 0x34, modifier::NONE };
        case '"':  return { 0x34, modifier::LEFT_SHIFT };
        case '`':  return { 0x35, modifier::NONE };
        case '~':  return { 0x35, modifier::LEFT_SHIFT };
        case ',':  return { 0x36, modifier::NONE };
        case '<':  return { 0x36, modifier::LEFT_SHIFT };
        case '.':  return { 0x37, modifier::NONE };
        case '>':  return { 0x37, modifier::LEFT_SHIFT };
        case '/':  return { 0x38, modifier::NONE };
        case '?':  return { 0x38, modifier::LEFT_SHIFT };

        default:
            return { keycode::NONE, modifier::NONE };
    }
}

} // namespace usb
