#pragma once
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
    enum class Type : uint8_t {
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
    uint32_t childCount;
    Element *next;
    Element *firstChild;
    Element *lastChild;
    StringView ref;

    bool ParseBody(StringView body, ArenaAllocator &arena,
                   const char **term = nullptr);

    bool Serialize(std::ostream &s, int level = 0);

    bool ArrayPush(Element *value);
    bool ObjectPush(StringView key, Element *value, ArenaAllocator &arena);
    bool ObjectAssign(StringView key, Element *value, ArenaAllocator &arena);
    bool ValuePush(Element *key);

    template <typename T> bool GetNumber(T &val);
    template <typename T> bool SetNumber(T val, ArenaAllocator &arena);

    Element *GetArrayIndex(uint32_t index);
    Element *FindKey(StringView name);
    Element *FindChildElement(StringView name);

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

    template <typename L> bool IterateObject(L l) {
        if (type != Type::Object) {
            return false;
        }

        for (auto it = firstChild; it != nullptr; it = it->next) {
            assert(it->type == Type::Key);
            l(it->ref, it->firstChild);
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

    inline std::unordered_map<std::string_view, Element *> GetObjectAsMap() {
        std::unordered_map<std::string_view, Element *> map;
        map.reserve(childCount);

        IterateObject(
            [&map](auto name, Element *elem) { map[name.ToStd()] = elem; });

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

    /**
     * Push a string to a stable position in the arena and return a
     * string_view pointig to it.
     */
    StringView PushString(StringView view);
};

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

inline bool Element::ObjectPush(StringView key, Element *value,
                                ArenaAllocator &arena) {
    if (type != Type::Object) {
        return false;
    }

    assert(value->type != Type::Key);
    assert(value->type != Type::Error);

    Element *keyElem = arena.CreateElement();
    if (!keyElem)
        return false;

    keyElem->type = Type::Key;
    keyElem->ref = key;
    keyElem->ValuePush(value);

    if (!firstChild || !lastChild) {
        firstChild = keyElem;
    } else {
        lastChild->next = keyElem;
    }

    lastChild = keyElem;
    childCount++;
    return true;
}

inline bool Element::ValuePush(Element *value) {
    if (type != Type::Key) {
        return false;
    }

    assert(value->type != Type::Key);

    firstChild = value;
    lastChild = value;

    childCount = 1;
    return true;
}

template <typename T> bool Element::GetNumber(T &val) {
    if (type != Type::Number)
        return false;
    std::from_chars_result res = std::from_chars(ref.begin, ref.end, val);

    return res.ec == std::errc();
}

template <typename T> bool Element::SetNumber(T val, ArenaAllocator &arena) {
    char buf[128];
    std::to_chars_result res =
        std::to_chars(std::begin(buf), std::end(buf), val);

    if (res.ec == std::errc()) {
        type = Type::Number;
        ref = arena.PushString({buf, res.ptr});
    } else {
        type = Type::Error;
        ref = "Failed to set number";
        return false;
    }
    return res.ec == std::errc();
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

inline Element *Element::FindKey(StringView name) {
    if (type != Type::Object) {
        return nullptr;
    }

    auto it = firstChild;

    while (it) {
        assert(it->type == Type::Key);

        if (it->ref == name) {
            return it;
        }

        it = it->next;
    }

    return nullptr;
}

inline Element *Element::FindChildElement(StringView name) {
    Element *key = FindKey(name);
    if (key)
        return key->firstChild;

    return nullptr;
}

inline bool Element::ObjectAssign(StringView key, Element *value,
                                  ArenaAllocator &arena) {
    if (value->type == Type::Key || value->type == Type::Error)
        return false;

    Element *find = FindChildElement(key);
    if (find) {
        find->ValuePush(value);
        return true;
    }

    return ObjectPush(key, value, arena);
}

} // namespace StupidJSON