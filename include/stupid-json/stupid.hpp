#pragma once
#include <cassert>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace StupidJSON {
class Element {
  public:
    enum class Type {
        Error = 0,
        String = 1,
        Number = 2,
        Object = 3,
        Array = 4,
        Null = 5,
        True = 6,
        False = 7,
    };

    using ArrType = std::vector<Element>;
    using ViewType = std::string_view;
    using ObjType = std::map<ViewType, Element>;
    using StrType = std::string;
    using StrItr = std::string_view::iterator;

    Element() = default;
    Element(const Element &) = delete;
    Element(Element &&) = default;

    bool Parse(StrItr begin, StrItr end, StrItr *term = nullptr);
    inline bool Parse(const std::string_view &view) {
        return Parse(view.begin(), view.end());
    }

    inline bool IsError() { return type == Type::Error; }
    inline bool IsNumber() { return type == Type::Number; }
    inline bool IsObject() { return type == Type::Object; }
    inline bool IsArray() { return type == Type::Array; }
    inline bool IsNull() { return type == Type::Null; }
    inline bool IsTrue() { return type == Type::True; }
    inline bool IsFalse() { return type == Type::False; }
    inline bool IsBool() { return IsTrue() || IsFalse(); }
    inline bool IsValid() { return !IsError(); }

    Type GetType() { return type; }

    Element *GetMember(ViewType name);
    Element *GetIndex(size_t index);
    size_t GetCount();

    ObjType::iterator ObjBegin() {
        assert(IsObject());
        return std::get<ObjType>(backing).begin();
    }

    ObjType::iterator ObjEnd() {
        assert(IsObject());
        return std::get<ObjType>(backing).end();
    }

    ArrType::iterator ArrBegin() {
        assert(IsArray());
        return std::get<ArrType>(backing).begin();
    }

    ArrType::iterator ArrEnd() {
        assert(IsArray());
        return std::get<ArrType>(backing).end();
    }

    bool GetString(std::ostream &s);
    bool GetString(std::string &str);

    inline bool GetRawString(ViewType &view) {
        return GetRawStringHelper(view, Type::String);
    }

    inline bool GetErrorString(ViewType &view) {
        return GetRawStringHelper(view, Type::Error);
    }

    bool GetRawStringHelper(ViewType &view, Type testType);

  private:
    bool ParseToken(StrItr begin, StrItr end, Element::StrItr *term,
                    ViewType token);
    bool ParseNumber(StrItr begin, StrItr end, Element::StrItr *term);
    bool ParseArray(StrItr begin, StrItr end, Element::StrItr *term);
    bool ParseObject(StrItr begin, StrItr end, Element::StrItr *term);

    std::variant<ArrType, ObjType, StrType, ViewType> backing;
    Type type = Type::Error;
};

static_assert(sizeof(Element) == 64, "Element has wrong size");

} // namespace StupidJSON