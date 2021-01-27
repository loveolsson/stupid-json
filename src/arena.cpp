#include "stupid-json/arena.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace StupidJSON {

static inline bool isSpace(char c) {
    switch (c) {
    case '\t':
    case '\n':
    case '\r':
    case ' ':
        return true;
    default:
        return false;
    }
}

static inline bool isEndOrNull(const char *begin, const char *end) {
    return begin == end || *begin == 0;
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

static inline const char *ConsumeString(const char *begin, const char *end) {
    auto it = std::find(begin, end, '\"');
    if (it == begin || it == end) {
        return it;
    }

    while (it != end && *(it - 1) == '\\') {
        it = std::find(it + 1, end, '\"');
    }

    return it;
}

static inline bool isDigit(char c) { return (c >= '0' && c <= '9'); }
static inline bool isDigitOrDot(char c) { return c == '.' || isDigit(c); }

static bool ParseToken(Element *elem, const char *begin, const char *end,
                       const char **term, StringView token) {
    auto tIt = token.begin;

    while (tIt != token.end) {
        if (begin == end || *begin++ != *tIt++) {
            elem->ref = StringView::FromStr("Invalid token");
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
        elem->ref =
            StringView::FromStr("String not terminated before end of document");
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
        elem->ref = StringView::FromStr("Malformed number");
        return false;
    }

    elem->type = Element::Type::Number;
    elem->ref = {begin, numEnd};

    if (term) {
        *term = numEnd;
    }

    return true;
}

static bool ParseObject(Element *elem, const char *begin, const char *end,
                        ArenaAllocator &arena, const char **term) {
    elem->type = Element::Type::Object; // Set type at the start, so that the
                                        // helper works
    begin = FwdSpaces(begin, end);

    while (begin != end) {
        if (*begin == '}') {
            if (term)
                *term = begin + 1;
            return true;
        }

        if (begin == end || *begin != '"') {
            elem->type = Element::Type::Error;
            elem->ref = StringView::FromStr("Key not found in object");
            return false;
        }

        auto strEnd = ConsumeString(begin + 1, end);
        if (strEnd == end) {
            elem->ref =
                StringView::FromStr("Key not terminated before end of stream");
            return false;
        }

        begin++; // Skip over opening quote
        Element *key = arena.CreateElement();
        Element *value = arena.CreateElement();

        if (!key || !value) {
            elem->ref = StringView::FromStr("Failed to allocate element");
            return false;
        }

        key->type = Element::Type::Key;
        key->ref = {begin, strEnd};
        strEnd++; // Skip over closing quote

        begin = FwdSpaces(strEnd, end);

        if (begin == end || *begin != ':') {
            elem->type = Element::Type::Error;
            elem->ref = StringView::FromStr("Invalid char after key");
            return false;
        }

        begin++; // Skip over colon
        if (value->ParseBody(begin, end, arena, &begin)) {
            key->ValuePush(value);
            elem->ObjectPush(key);
        } else {
            elem->type = Element::Type::Error;
            elem->ref = value->ref;
            return false;
        }

        begin = FwdCommaOrTerm(begin, end, '}');
    }

    elem->type = Element::Type::Error;
    elem->ref =
        StringView::FromStr("End of stream reached before end of object");
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
            elem->ref = StringView::FromStr("Failed to allocate element");
            return false;
        }

        if (el->ParseBody(begin, end, arena, &begin)) {
            elem->ArrayPush(el);
        } else {
            elem->type = Element::Type::Error;
            elem->ref = el->ref; // Get the error message from the child element
                                 // that failed to parse
            return false;
        }

        begin = FwdCommaOrTerm(begin, end, ']');
    }

    elem->type = Element::Type::Error;
    elem->ref = StringView::FromStr("Invalid separator in array");
    return false;
}

bool Element::ParseBody(const char *begin, const char *end,
                        ArenaAllocator &arena, const char **term) {
    type = Type::Error;
    next = nullptr;
    firstChild = nullptr;
    lastChild = nullptr;
    childCount = 0;

    begin = FwdSpaces(begin, end);
    if (begin == end) {
        ref = StringView::FromStr("Element not found before end of document");
        return false;
    }

    switch (*begin) {
    case '\"':
        ParseString(this, begin + 1, end, term);
        break;

    case '{':
        ParseObject(this, begin + 1, end, arena, term);
        break;

    case '[':
        ParseArray(this, begin + 1, end, arena, term);
        break;

    case 'n':
        if (ParseToken(this, begin, end, term, StringView::FromStr("null"))) {
            type = Type::Null;
        }
        break;

    case 't':
        if (ParseToken(this, begin, end, term, StringView::FromStr("true"))) {
            type = Type::True;
        }
        break;

    case 'f':
        if (ParseToken(this, begin, end, term, StringView::FromStr("false"))) {
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
        ref = StringView::FromStr("Reached end of parsing");
        break;
    }

    return type != Type::Error;
}

bool Element::Serialize(std::ostream &s, int level) {
    bool res = true;

    auto addTabs = [](auto &s, int level) {
        for (int i = 0; i < level; ++i) {
            s << "  ";
        }
    };

    switch (type) {
    case Type::String:
        s << '\"' << ref << '\"';
        break;
    case Type::Number:
        s << ref;
        break;
    case Type::Object: {
        s << '{' << std::endl;

        size_t index = 0;
        res = IterateObject([&](auto key, auto e) {
            if (index++ != 0) {
                s << ',' << std::endl;
            }

            addTabs(s, level + 1);

            s << '\"' << key << "\": ";
            e->Serialize(s, level + 1);
        });

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

            e->Serialize(s, level + 1);
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
    if (elementAllocSize < 1 << 16) {
        elementAllocSize *= 2;
    }
};

StringView ArenaAllocator::PushString(StringView view) {
    if (view.Empty()) {
        return StringView::FromStr("");
    }

    StringAllocHeader *it = nextStringAlloc;
    int searchLength = 3; // Max steps-1 to search for free space

    // Find allocation that fits the string
    while (it) {
        if (it->Remain() >= view.Size()) {
            break;
        }

        if (searchLength-- == 0) {
            it = nullptr;
            break;
        }

        it = it->next;
    }

    if (!it) {
        it = AllocateStrings(std::max<size_t>(view.Size(), 1024));
    }

    assert(it != nullptr);

    // Put the string at the write head of the allocation, and forward
    // the write head
    char *ptr =
        reinterpret_cast<char *>(it) + sizeof(StringAllocHeader) + it->head;

    memcpy(ptr, view.begin, view.Size());
    it->head += view.Size();
    return {ptr, view.Size()};
}

} // namespace StupidJSON