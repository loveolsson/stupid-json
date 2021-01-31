#include "stupid-json/arena.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace StupidJSON {

static inline bool isSpace(char c) {
    static const bool table[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return table[c];
}

static inline bool isDigit(char c) {
    static const bool table[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return table[c];
}

static inline bool isDigitOrDot(char c) {
    static const bool table[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return table[c];
}

/*
static inline std::array<int8_t, 256> GenerateHexTable() {
    std::array<int8_t, 256> res;
    res.fill(-1);
    res['0'] = 0;
    res['1'] = 1;
    res['2'] = 2;
    res['3'] = 3;
    res['4'] = 4;
    res['5'] = 5;
    res['6'] = 6;
    res['7'] = 7;
    res['8'] = 8;
    res['9'] = 9;
    res['a'] = res['A'] = 10;
    res['b'] = res['B'] = 11;
    res['c'] = res['C'] = 12;
    res['d'] = res['D'] = 13;
    res['e'] = res['E'] = 14;
    res['f'] = res['F'] = 15;

    std::cout << std::endl;
    for (int c : res) {
        std::cout << c << ", ";
    }
    std::cout << std::endl;
    return res;
}
*/

static inline int8_t GetHexValue(char c) {
    // static const auto table = GenerateHexTable();
    static const int8_t table[] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,
        6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1,
    };
    return table[c];
}

static inline char GetHexChar(char v) {
    char table[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    return table[v];
}

static inline const char *FwdSpaces(const char *begin, const char *end) {
    while (begin != end && isSpace(*begin))
        ++begin;

    return begin;
}

static inline const char *FwdCommaOrTerm(const char *begin, const char *end,
                                         char term) {
    begin = FwdSpaces(begin, end);
    if (begin == end || *begin != ',' && *begin != term) {
        return end;
    }

    if (*begin == ',') {
        begin++;
    }

    return FwdSpaces(begin, end);
}

static inline const char *FindChar(const char *begin, const char *end, char c) {
    while (begin != end && *begin != c) {
        begin++;
    }

    return begin;
}

static inline size_t CountChar(const char *begin, const char *end, char c) {
    size_t count = 0;
    while (begin != end) {
        if (*begin == c)
            count++;

        begin++;
    }

    return count;
}

static inline const char *ConsumeString(const char *begin, const char *end) {
    auto it = FindChar(begin, end, '\"');
    if (it == begin || it == end) {
        return it;
    }

    while (it != end && *(it - 1) == '\\') {
        it = FindChar(it + 1, end, '\"');
    }

    return it;
}

static bool ParseToken(Element *elem, const char *begin, const char *end,
                       const char **term, StringView token) {
    auto tIt = token.begin;

    while (tIt != token.end) {
        if (begin == end || *begin++ != *tIt++) {
            elem->ref = "Invalid token";
            return false;
        }
    }

    if (term)
        *term = begin;
    return true;
}

static bool ParseString(Element *elem, const char *begin, const char *end,
                        const char **term) {
    auto strEnd = ConsumeString(begin, end);
    if (strEnd == end) {
        elem->ref = "String not terminated before end of document";
        return false;
    }

    elem->ref = {begin, strEnd};
    elem->type = Element::Type::String;
    if (term)
        *term = strEnd + 1;
    return true;
}

static bool ParseNumber(Element *elem, const char *begin, const char *end,
                        const char **term) {
    // Skip past first char, as it's confirmed to be valid, and might be a '-'
    auto numEnd = begin + 1;
    int dotCount = 0;
    int numCount = 0;
    bool malformed = true;

    // A number can only start with a dash or a digit, if we got here and it's
    // not a dash, it's safe to assume it's a digit
    if (*begin != '-') {
        malformed = false;
        numCount++;
    }

    while (numEnd != end && isDigitOrDot(*numEnd)) {
        if (*numEnd == '.') {
            // Dot
            malformed = true; // A number is not allowed to end with a dot
            if (numCount == 0 || ++dotCount > 1) {
                // A dot must be preceded by a number, and there can only be one
                // dot in a number
                break;
            }
        } else {
            // Digit
            numCount++;
            malformed = false;
        }
        numEnd++;
    }

    if (malformed) {
        elem->ref = "Malformed number";
        return false;
    }

    elem->type = Element::Type::Number;
    elem->ref = {begin, numEnd};

    if (term) {
        *term = numEnd;
    }

    return true;
}

void Element::EscapeStr(ArenaAllocator &arena) {
    auto count = CountChar(ref.begin, ref.end, '\\');

    size_t totalSize = cleanRef.Size() + count;
    char *target = arena.AllocateString(totalSize);
    char *t = target;
    bool isDirty = false;

    for (auto c = ref.begin; c != ref.end;) {
        // if (*c >= 0x80) {
        //     // Unicode
        //     size_t u = 0;

        //     *(t++) = '\\';
        //     *(t++) = 'u';
        //     *(t++) = GetHexChar((u >> 12) & 0xf);
        //     *(t++) = GetHexChar((u >> 8) & 0xf);
        //     *(t++) = GetHexChar((u >> 4) & 0xf);
        //     *(t++) = GetHexChar(u & 0xf);

        //     continue;
        // }

        switch (*c) {
        case '\b':
            *(t++) = '\\';
            *(t++) = 'b';
            c++;
            isDirty = true;
            break;
        case '\f':
            *(t++) = '\\';
            *(t++) = 'f';
            c++;
            isDirty = true;
            break;
        case '\n':
            *(t++) = '\\';
            *(t++) = 'n';
            c++;
            isDirty = true;
            break;
        case '\r':
            *(t++) = '\\';
            *(t++) = 'r';
            c++;
            isDirty = true;
            break;
        case '\t':
            *(t++) = '\\';
            *(t++) = 't';
            c++;
            isDirty = true;
            break;
        case '\"':
            *(t++) = '\\';
            *(t++) = '\"';
            c++;
            isDirty = true;
            break;
        case '\\':
            *(t++) = '\\';
            *(t++) = '\\';
            c++;
            isDirty = true;
            break;
        default:
            *(t++) = *(c++);
        }
    }

    if (!isDirty) {
        ref = cleanRef;
        arena.ReturnUnused(totalSize);
    } else {
        ref = {target, t};
        arena.ReturnUnused(totalSize - std::distance(target, t));
    }
}

static int ReadUnicodeLiteral(const char *begin, const char *end,
                              const char **term) {
    if (begin == end || *(begin++) != '\\')
        return -1;
    if (begin == end || *(begin++) != 'u')
        return -1;

    uint16_t u = 0;
    for (int i = 0; i < 4; ++i) {
        if (begin == end)
            return -1;

        int8_t val = GetHexValue(*(begin++));
        if (val == -1)
            return false;

        u <<= 4;
        u |= val;
    }

    if (term)
        *term = begin;

    return u;
}

bool Element::UnescapeStr(ArenaAllocator &arena) {
    auto count = CountChar(ref.begin, ref.end, '\\');
    if (count == 0) {
        cleanRef = ref; // This is a clean string, it can be used as is
        return true;
    }

    size_t totalSize = ref.Size();
    char *target = arena.AllocateString(totalSize);
    char *t = target;

    for (auto c = ref.begin; c != ref.end;) {
        if (*c == '\\') {
            if (c + 1 == ref.end) {
                return false;
            }

            switch (*(c + 1)) {
            case 'b':
                *(t++) = '\b';
                c += 2;
                break;
            case 'f':
                *(t++) = '\f';
                c += 2;
                break;
            case 'n':
                *(t++) = '\n';
                c += 2;
                break;
            case 'r':
                *(t++) = '\r';
                c += 2;
                break;
            case 't':
                *(t++) = '\t';
                c += 2;
                break;
            case '\"':
                *(t++) = '\"';
                c += 2;
                break;
            case '\\':
                *(t++) = '\\';
                c += 2;
                break;
            case 'u': {
                int lit = ReadUnicodeLiteral(c, ref.end, &c);
                if (lit == -1) {
                    return false;
                }

                uint32_t u = lit;

                if (0xD800 <= lit && lit <= 0xDBFF) {
                    lit = ReadUnicodeLiteral(c, ref.end, &c);
                    if (0xDC00 > lit || lit > 0xDFFF) {
                        return false;
                    }

                    u &= 0x03FF;
                    u <<= 10;
                    u |= 0x10000;

                    u |= lit & 0x03FF;
                }

                const char byteMask = 0b00111111;
                const char prefix = 0b10000000;

                if (u <= 0x007f) {
                    *(t++) = u;
                } else if (u <= 0x07ff) {
                    *(t++) = 0b11000000 | (u >> 6);
                    *(t++) = prefix | (u & byteMask);
                } else if (u <= 0xffff) {
                    *(t++) = 0b11100000 | (u >> 12);
                    *(t++) = prefix | ((u >> 6) & byteMask);
                    *(t++) = prefix | (u & byteMask);
                } else if (u <= 0x10ffff) {
                    *(t++) = 0b11110000 | (u >> 18);
                    *(t++) = prefix | ((u >> 12) & byteMask);
                    *(t++) = prefix | ((u >> 6) & byteMask);
                    *(t++) = prefix | (u & byteMask);
                } else {
                    return false;
                }
            } break;
            default:
                return false;
            }
        } else {
            *(t++) = *(c++);
        }
    }

    arena.ReturnUnused(totalSize - std::distance(target, t));
    cleanRef = {target, t};
    // std::cout << "Unclean str: " << cleanRef.ToStd() << std::endl;
    return true;
}

static bool ParseObject(Element *elem, const char *begin, const char *end,
                        ArenaAllocator &arena, const char **term) {
    elem->type = Element::Type::Object; // Set type at the start, so that
                                        // the helper works
    begin = FwdSpaces(begin, end);

    while (begin != end) {
        if (*begin == '}') {
            if (term)
                *term = begin + 1;
            return true;
        }

        if (begin == end || *begin != '\"') {
            elem->type = Element::Type::Error;
            elem->ref = "Key not found in object";
            return false;
        }

        auto strEnd = ConsumeString(begin + 1, end);
        if (strEnd == end) {
            elem->ref = "Key not terminated before end of stream";
            return false;
        }

        begin++; // Skip over opening quote
        Element *key = arena.CreateElement();
        Element *value = arena.CreateElement();

        if (!key || !value) {
            elem->ref = "Failed to allocate element";
            return false;
        }

        key->type = Element::Type::Key;
        key->ref = {begin, strEnd};
        if (!key->UnescapeStr(arena)) {
            elem->type = Element::Type::Error;
            elem->ref = "Key contains incorrectly escaped characters";
            return false;
        }

        strEnd++; // Skip over closing quote

        begin = FwdSpaces(strEnd, end);

        if (begin == end || *begin != ':') {
            elem->type = Element::Type::Error;
            elem->ref = "Invalid char after key";
            return false;
        }

        begin++; // Skip over colon
        if (value->ParseBody({begin, end}, arena, &begin)) {
            if (!elem->ObjectPush(key, value)) {
                elem->type = Element::Type::Error;
                elem->ref = "Failed to append key to object";
            }
        } else {
            elem->type = Element::Type::Error;
            elem->ref = value->ref;
            return false;
        }

        begin = FwdCommaOrTerm(begin, end, '}');
    }

    elem->type = Element::Type::Error;
    elem->ref = "End of stream reached before end of object";
    return false;
}

static bool ParseArray(Element *elem, const char *begin, const char *end,
                       ArenaAllocator &arena, const char **term) {
    elem->type = Element::Type::Array; // Set type at the start, so that the
                                       // helper works
    begin = FwdSpaces(begin, end);

    while (begin != end) {
        if (*begin == ']') {
            begin++;

            if (term) {
                *term = begin;
            }

            return true;
        }

        Element *el = arena.CreateElement();
        if (!el) {
            elem->type = Element::Type::Error;
            elem->ref = "Failed to allocate element";
            return false;
        }

        if (el->ParseBody({begin, end}, arena, &begin)) {
            elem->ArrayPush(el);
        } else {
            elem->type = Element::Type::Error;
            elem->ref = el->ref; // Get the error message from the child
                                 // element that failed to parse
            return false;
        }

        // NOTE: Check if this incorrectly skips past missing comma
        begin = FwdCommaOrTerm(begin, end, ']');
    }

    elem->type = Element::Type::Error;
    elem->ref = "Invalid separator in array";
    return false;
}

bool Element::ParseBody(StringView body, ArenaAllocator &arena,
                        const char **term) {
    auto begin = body.begin, end = body.end;

    // Reset element, in case it is being reused
    type = Type::Error;
    next = nullptr;
    firstChild = nullptr;
    lastChild = nullptr;
    childCount = 0;

    begin = FwdSpaces(begin, end);
    if (begin == end) {
        ref = "Element not found before end of document";
        return false;
    }

    switch (*begin) {
    case '\"':
        ParseString(this, begin + 1, end, term);
        if (!UnescapeStr(arena)) {
            type = Type::Error;
            ref = "String contains incorrectly escaped characters";
        }
        break;

    case '{':
        ParseObject(this, begin + 1, end, arena, term);
        break;

    case '[':
        ParseArray(this, begin + 1, end, arena, term);
        break;

    case 'n':
        if (ParseToken(this, begin, end, term, "null")) {
            type = Type::Null;
        }
        break;

    case 't':
        if (ParseToken(this, begin, end, term, "true")) {
            type = Type::True;
        }
        break;

    case 'f':
        if (ParseToken(this, begin, end, term, "false")) {
            type = Type::False;
        }
        break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        ParseNumber(this, begin, end, term);
        break;

    default:
        ref = "Reached end of parsing";
        break;
    }

    return type != Type::Error;
}

bool Element::Serialize(ArenaAllocator &arena, std::ostream &s, int level) {
    bool res = true;

    auto addTabs = [](auto &s, int level) {
        for (int i = 0; i < level; ++i) {
            s << "  ";
        }
    };

    switch (type) {
    case Type::String:
        s << '\"' << GetEscapedString(arena) << '\"';
        break;
    case Type::Number:
        s << ref;
        break;
    case Type::Object: {
        s << '{' << std::endl;

        size_t index = 0;
        for (auto it = firstChild; it != nullptr; it = it->next) {
            if (index++ != 0) {
                s << ',' << std::endl;
            }

            addTabs(s, level + 1);

            s << '\"' << it->GetEscapedString(arena) << "\": ";
            if (!it->firstChild) {
                res = false;
                break;
            }

            it->firstChild->Serialize(arena, s, level + 1);
        }

        if (res == false)
            break;

        s << std::endl;
        addTabs(s, level);
        s << '}';
    } break;
    case Type::Array:
        s << '[' << std::endl;

        res = IterateArray([&](auto index, auto e) {
            if (index != 0) {
                s << ',' << std::endl;
            }

            addTabs(s, level + 1);

            e->Serialize(arena, s, level + 1);
        });

        s << std::endl;
        addTabs(s, level);
        s << ']';
        break;
    case Type::Null:
        s << "null";
        break;
    case Type::True:
        s << "true";
        break;
    case Type::False:
        s << "false";
        break;
    default:
        res = false;
        break;
    }

    return res;
}

ArenaAllocator::ArenaAllocator(ArenaAllocator &&o) noexcept
    : nextElementAlloc(o.nextElementAlloc), nextStringAlloc(o.nextStringAlloc) {
    o.nextElementAlloc = nullptr;
    o.nextStringAlloc = nullptr;
}

ArenaAllocator::~ArenaAllocator() { Reset(); }

void ArenaAllocator::Reset() {
    auto itrFree = [](auto **root) {
        for (auto it = *root; it != nullptr;) {
            auto next = it->next;
            // printf("Free: %lu\n", it->size);

            free(it);
            it = next;
        }

        *root = nullptr;
    };

    itrFree(&nextElementAlloc);
    itrFree(&nextStringAlloc);
    elementAllocSize = 64;
}

ArenaAllocator::StringAllocHeader *
ArenaAllocator::AllocateStrings(size_t size) {
    assert(size >= 1024);

    StringAllocHeader *alloc = reinterpret_cast<StringAllocHeader *>(
        malloc(size + sizeof(StringAllocHeader)));
    if (!alloc) {
        return nullptr;
    }

    alloc->head = 0;
    alloc->size = size;
    alloc->next = nextStringAlloc;
    nextStringAlloc = alloc;

    return alloc;
}

void ArenaAllocator::AllocateElements() {
    auto alloc = reinterpret_cast<ElementAllocHeader *>(
        calloc(elementAllocSize, sizeof(Element)));
    if (!alloc) {
        return;
    }

    alloc->head = 1;
    alloc->size = elementAllocSize;
    alloc->next = nextElementAlloc;
    nextElementAlloc = alloc;

    // printf("Alloc grow: %lu\n", elementAllocSize);
    if (elementAllocSize < (1 << 16)) {
        elementAllocSize <<= 1;
    }
};

char *ArenaAllocator::AllocateString(size_t size) {
    StringAllocHeader *it = nextStringAlloc;
    int searchLength = 3; // Max steps-1 to search for free space

    // Find allocation where the string fits
    while (it) {
        if (it->Remain() >= size) {
            break;
        }

        if (searchLength-- == 0) {
            it = nullptr;
            break;
        }

        it = it->next;
    }

    if (!it) {
        it = AllocateStrings(std::max<size_t>(size, 1024));
    }

    assert(it != nullptr);

    // Put the string at the write head of the allocation, and forward
    // the write head
    char *ptr =
        reinterpret_cast<char *>(it) + sizeof(StringAllocHeader) + it->head;

    it->head += size;

    return ptr;
}

void ArenaAllocator::ReturnUnused(size_t size) {
    if (!nextStringAlloc)
        return;
    if (nextStringAlloc->head < size)
        return;
    nextStringAlloc->head -= size;
}

StringView ArenaAllocator::PushString(StringView view) {
    auto target = AllocateString(view.Size());
    memcpy(target, view.begin, view.Size());
    return {target, view.Size()};
}

} // namespace StupidJSON