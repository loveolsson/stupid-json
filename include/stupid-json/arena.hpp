#pragma once
#include "fast_float/fast_float.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstring>
#include <ostream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace StupidJSON {

class ArenaAllocator;

struct StringView {
    const char *begin;
    const char *end;

    inline StringView() : begin(nullptr), end(nullptr) {}
    inline StringView(const char *_begin, const char *_end)
        : begin(_begin), end(_end) {}
    inline StringView(const char *_begin, size_t _size)
        : begin(_begin), end(_begin + _size) {}
    inline StringView(const char *str) : StringView(str, strlen(str)) {}

    inline size_t Size() const { return static_cast<size_t>(end - begin); }
    inline bool Empty() const { return begin == end; }

#if 0
    inline bool operator==(const StringView &o) const {
        return Size() == o.Size() && memcmp(begin, o.begin, Size()) == 0;
    }
#else
    // Faster for shorter strings, such as keys
    inline bool operator==(const StringView &o) const {
        if (Size() != o.Size())
            return false;
        if (begin == o.begin) // Unlikely unless comparing to user insertions
            return true;
        for (int i = 0; i < Size(); ++i) {
            if (begin[i] != o.begin[i])
                return false;
        }
        return true;
    }
#endif

    inline bool operator!=(const StringView &o) const { return !(*this == o); }
    inline bool operator==(const char *o) const {
        return *this == StringView(o);
    }
    inline bool operator!=(const char *o) const {
        return *this != StringView(o);
    }

    std::string_view ToStd() const { return {begin, Size()}; }

    friend std::ostream &operator<<(std::ostream &os, const StringView &s) {
        os << s.ToStd();
        return os;
    }
};

struct Element {
    enum class Type {
        Error = 0,
        Key,
        String,
        Number,
        Object,
        Array,
        Null,
        True,
        False,
    };

    Type type;
    StringView ref;
    Element *next;
    Element *firstChild;
    union {
        struct {
            Element *lastChild;
            size_t childCount;
        };
        StringView
            cleanRef; // Only used for String or Key to contain escaped version
    };

    bool ParseBody(StringView body, ArenaAllocator &arena,
                   const char **term = nullptr);

    bool Serialize(ArenaAllocator &arena, std::ostream &s, int level = 0);

    StringView GetString(ArenaAllocator &arena);
    StringView GetEscapedString(ArenaAllocator &arena);
    void SetString(StringView str);
    void Setkey(StringView key);

    bool ArrayPush(Element *value);
    bool ObjectPush(Element *key, Element *value);
    bool ObjectPush(StringView key, Element *value, ArenaAllocator &arena);
    bool ObjectAssign(StringView key, Element *value, ArenaAllocator &arena);
    bool ValuePush(Element *key);

    void EscapeStr(ArenaAllocator &arena);
    bool UnescapeStr(ArenaAllocator &arena);

    template <typename T> bool GetInteger(T &val);
    template <typename T> bool GetFloatingPoint(T &val);
    template <typename T> bool SetNumber(T val, ArenaAllocator &arena);

    Element *GetArrayIndex(uint32_t index);
    Element *FindKey(StringView name, ArenaAllocator &arena);
    Element *FindChildElement(StringView name, ArenaAllocator &arena);

    template <typename L> bool IterateArray(L l) {
        if (type != Type::Array) {
            return false;
        }

        size_t index = 0;

        for (auto it = firstChild; it != nullptr; it = it->next) {
            l(index++, it);
        }

        return true;
    }

    template <typename L> bool IterateObject(ArenaAllocator &arena, L l) {
        if (type != Type::Object) {
            return false;
        }

        for (auto it = firstChild; it != nullptr; it = it->next) {
            assert(it->type == Type::Key);
            l(it->GetString(arena), it->firstChild);
        }

        return true;
    }

    inline std::vector<Element *> GetChildrenAsVector() {
        std::vector<Element *> arr;

        if (type == Type::Array) {
            arr.reserve(childCount);
            for (auto it = firstChild; it != nullptr; it = it->next) {
                arr.push_back(it);
            }
        }

        return arr;
    }

    inline std::unordered_map<std::string_view, Element *>
    GetObjectAsMap(ArenaAllocator &arena) {
        std::unordered_map<std::string_view, Element *> map;
        map.reserve(childCount);

        IterateObject(arena, [&map](auto name, Element *elem) {
            map[name.ToStd()] = elem;
        });

        return map;
    }
};

class ArenaAllocator {
    struct ElementAllocHeader {
        size_t head;
        size_t size;
        ElementAllocHeader *next;
    };

    struct StringAllocHeader {
        size_t head;
        size_t size;
        StringAllocHeader *next;

        inline size_t Remain() { return size - head; }
    };

    ElementAllocHeader *nextElementAlloc = nullptr;
    StringAllocHeader *nextStringAlloc = nullptr;
    size_t elementAllocSize = 64;

    StringAllocHeader *AllocateStrings(size_t size);
    void AllocateElements();

  public:
    ArenaAllocator() = default;
    ArenaAllocator(const ArenaAllocator &) = delete;
    ArenaAllocator(ArenaAllocator &&) noexcept;
    ~ArenaAllocator();

    /**
     * Free all allocations so that the allocator can be reused.
     */
    void Reset();

    /**
     * Add a new element and return a poiner to the new element
     */
    inline Element *CreateElement() {
        // Ensure that the element fits
        static_assert(sizeof(Element) >= sizeof(ElementAllocHeader));

        if (!nextElementAlloc ||
            nextElementAlloc->head == nextElementAlloc->size) {
            AllocateElements();
        }

        if (!nextElementAlloc) {
            return nullptr;
        }

        auto elems = reinterpret_cast<Element *>(nextElementAlloc);
        auto &el = elems[nextElementAlloc->head++];

        return &el;
    }

    char *AllocateString(size_t size);
    void ReturnUnused(size_t size);

    /**
     * Push a string to a stable position in the arena and return a
     * string_view pointig to it.
     */
    StringView PushString(StringView view);
};

inline StringView Element::GetString(ArenaAllocator &arena) {
    if (type != Type::String && type != Type::Key) {
        return {};
    }

    if (cleanRef.begin != nullptr) {
        return cleanRef;
    }

    UnescapeStr(arena);
    return cleanRef;
}

inline StringView Element::GetEscapedString(ArenaAllocator &arena) {
    if (type != Type::String && type != Type::Key) {
        return {};
    }

    if (ref.begin != nullptr) {
        return ref;
    }

    EscapeStr(arena);
    return ref;
}

inline void Element::SetString(StringView str) {
    type = Type::String;
    ref = {};
    firstChild = nullptr;
    cleanRef = str;
}

inline void Element::Setkey(StringView key) {
    type = Type::Key;
    ref = {};
    cleanRef = key;
}

inline bool Element::ArrayPush(Element *value) {
    if (type != Type::Array) {
        return false;
    }

    assert(value->type != Type::Key);

    if (!firstChild || !lastChild) {
        firstChild = value;
    } else {
        lastChild->next = value;
    }

    lastChild = value;
    childCount++;
    return true;
}

inline bool Element::ObjectPush(Element *key, Element *value) {
    if (type != Type::Object) {
        return false;
    }

    assert(key->type == Type::Key);
    assert(value->type != Type::Key);
    assert(value->type != Type::Error);

    key->ValuePush(value);

    if (!firstChild || !lastChild) {
        firstChild = key;
    } else {
        lastChild->next = key;
    }

    lastChild = key;
    childCount++;
    return true;
}

inline bool Element::ObjectPush(StringView key, Element *value,
                                ArenaAllocator &arena) {
    if (type != Type::Object) {
        return false;
    }

    Element *keyElem = arena.CreateElement();
    if (!keyElem)
        return false;

    keyElem->type = Type::Key;
    keyElem->ref = {};
    keyElem->cleanRef = key;

    return ObjectPush(keyElem, value);
}

inline bool Element::ValuePush(Element *value) {
    if (type != Type::Key) {
        return false;
    }

    assert(value->type != Type::Key);

    firstChild = value;
    return true;
}

template <typename T> bool Element::GetInteger(T &val) {
    if (type != Type::Number)
        return false;
    auto res = std::from_chars(ref.begin, ref.end, val);

    return res.ec == std::errc();
}

template <typename T> bool Element::GetFloatingPoint(T &val) {
    if (type != Type::Number)
        return false;
    auto res = fast_float::from_chars(ref.begin, ref.end, val);

    return res.ec == std::errc();
}

template <typename T> bool Element::SetNumber(T val, ArenaAllocator &arena) {
    if (!std::isfinite(val)) {
        type = Type::Error;
        ref = "Failed to set number";
        return false;
    }

    // Temporary hack while I can't be bothered
    auto buf = std::to_string(val);
    type = Type::Number;
    ref = arena.PushString({buf.data(), buf.size()});

    return true;
}

inline Element *Element::GetArrayIndex(uint32_t index) {
    if (type != Type::Array || childCount <= index) {
        return nullptr;
    }

    uint32_t i = 0;
    for (auto it = firstChild; it != nullptr; it = it->next) {
        if (i++ == index) {
            return it;
        }
    }

    return nullptr;
}

inline Element *Element::FindKey(StringView name, ArenaAllocator &arena) {
    if (type != Type::Object) {
        return nullptr;
    }

    auto it = firstChild;

    while (it) {
        assert(it->type == Type::Key);

        if (it->GetString(arena) == name) {
            return it;
        }

        it = it->next;
    }

    return nullptr;
}

inline Element *Element::FindChildElement(StringView name,
                                          ArenaAllocator &arena) {
    Element *key = FindKey(name, arena);
    if (key)
        return key->firstChild;

    return nullptr;
}

inline bool Element::ObjectAssign(StringView key, Element *value,
                                  ArenaAllocator &arena) {
    if (value->type == Type::Key || value->type == Type::Error)
        return false;

    Element *find = FindChildElement(key, arena);
    if (find) {
        find->ValuePush(value);
        return true;
    }

    return ObjectPush(key, value, arena);
}

} // namespace StupidJSON