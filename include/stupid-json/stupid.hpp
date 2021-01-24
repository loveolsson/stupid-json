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

    Element(StrItr begin, StrItr end, StrItr *term = nullptr);
    Element(const std::string_view &view) : Element(view.begin(), view.end()) {}

    bool IsError() { return type == Type::Error; }
    bool IsNumber() { return type == Type::Number; }
    bool IsObject() { return type == Type::Object; }
    bool IsArray() { return type == Type::Array; }
    bool IsNull() { return type == Type::Null; }
    bool IsTrue() { return type == Type::True; }
    bool IsFalse() { return type == Type::False; }
    bool IsBool() { return IsTrue() || IsFalse(); }
    bool IsValid() { return !IsError(); }

    Type GetType() { return type; }

    Element *GetMember(ViewType name);
    Element *GetIndex(size_t index);
    size_t GetCount();

    bool GetString(std::ostream &s);
    bool GetString(std::string &str);

    bool GetRawString(ViewType &view) {
        return GetRawStringHelper(view, Type::String);
    }

    bool GetErrorString(ViewType &view) {
        return GetRawStringHelper(view, Type::Error);
    }

  private:
    bool GetRawStringHelper(ViewType &view, Type testType);
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