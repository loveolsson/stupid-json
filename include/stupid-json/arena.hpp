#pragma once
#include <algorithm>
#include <cassert>
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

    StringView() : begin(nullptr), end(nullptr) {}
    StringView(const char *_begin, const char *_end)
        : begin(_begin), end(_end) {}
    StringView(const char *_begin, size_t _size)
        : begin(_begin), end(begin + _size) {}

    static StringView FromStr(const char *str) {
        size_t size = strlen(str);
        StringView res{str, size};
        return res;
    }

    size_t Size() const { return std::distance(begin, end); }

    bool Empty() const { return begin == end; }

    bool operator==(const StringView &o) const {
        return Size() == o.Size() && strncmp(begin, o.begin, Size()) == 0;
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
    std::string_view ref;

    bool ParseBody(std::string_view::iterator begin,
                   std::string_view::iterator end, ArenaAllocator &arena,
                   std::string_view::iterator *term);

    bool Serialize(std::ostream &s, int level = 0);

    inline bool ParseBody(std::string_view body, ArenaAllocator &arena) {
        return ParseBody(body.begin(), body.end(), arena, nullptr);
    }

    bool ArrayPush(Element *value);
    bool ObjectPush(Element *key);
    bool ValuePush(Element *key);

    Element *GetArrayIndex(uint32_t index) {
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

    Element *FindChildElement(const std::string_view &name);

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

        IterateObject([&map](auto name, Element *elem) { map[name] = elem; });

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
    std::string_view PushString(std::string_view view);
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

inline bool Element::ObjectPush(Element *key) {
    if (type != Type::Object) {
        return false;
    }

    assert(key->type == Type::Key);
    assert(key->firstChild && key->lastChild);

    if (!firstChild || !lastChild) {
        firstChild = key;
    } else {
        lastChild->next = key;
    }

    lastChild = key;
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

inline Element *Element::FindChildElement(const std::string_view &name) {
    if (type != Type::Object) {
        return nullptr;
    }

    auto it = firstChild;

    while (it) {
        assert(it->type == Type::Key);

        if (it->ref == name) {
            return it->firstChild;
        }

        it = it->next;
    }

    return nullptr;
}
} // namespace StupidJSON