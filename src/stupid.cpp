#include "stupid-json/stupid.hpp"
#include <algorithm>

namespace StupidJSON {
template <typename StrItr> StrItr FwdSpaces(StrItr begin, StrItr end) {
    while (begin != end && isspace(*begin))
        ++begin;

    return begin;
}

template <typename StrItr>
StrItr FwdCommaOrTerm(StrItr begin, StrItr end, char term) {
    begin = FwdSpaces(begin, end);
    if (begin == end || *begin != ',' && *begin != term) {
        return end;
    }

    if (*begin == ',') {
        begin++;
    }

    return FwdSpaces(begin, end);
}

template <typename StrItr>
StrItr ConsumeString(const StrItr begin, const StrItr end) {
    auto it = std::find(begin, end, '\"');
    if (it == begin || it == end) {
        return it;
    }

    while (it != end && *(it - 1) == '\\') {
        it = std::find(it + 1, end, '\"');
    }

    return it;
}

Element::Element(StrItr begin, StrItr end, StrItr *term) {
    begin = FwdSpaces(begin, end);
    if (begin == end) {
        backing.emplace<ViewType>("Element not found before end of document");
        return;
    }

    if (*begin == '"') {
        auto str = ConsumeString(begin + 1, end);
        if (str == end) {
            backing.emplace<ViewType>(
                "String not terminated before end of document");
            return;
        }

        this->backing.emplace<ViewType>(&*(begin + 1),
                                        std::distance(begin + 1, str));
        if (term)
            *term = str + 1;
        type = Type::String;
        return;
    }

    switch (*begin) {
    case '{':
        if (ParseObject(begin + 1, end, term)) {
            type = Type::Object;
        }
        break;
    case '[':
        if (ParseArray(begin + 1, end, term)) {
            type = Type::Array;
        }
        break;

    case 'n':
        if (ParseToken(begin, end, term, "null")) {
            type = Type::Null;
        }
        break;

    case 't':
        if (ParseToken(begin, end, term, "true")) {
            type = Type::True;
        }
        break;

    case 'f':
        if (ParseToken(begin, end, term, "false")) {
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
        if (ParseNumber(begin, end, term)) {
            type = Type::Number;
        }
        break;

    default:
        backing.emplace<StrType>("Reached end of parsing with remainder: " +
                                 std::string(begin, end));
        break;
    }
}

Element *Element::GetMember(ViewType name) {
    if (type != Type::Object) {
        return nullptr;
    }

    assert(std::holds_alternative<ObjType>(backing));

    auto &obj = std::get<ObjType>(backing);
    auto it = obj.find(name);
    if (it != obj.end()) {
        return &it->second;
    }

    return nullptr;
}

Element *Element::GetIndex(size_t index) {
    if (type != Type::Array) {
        return nullptr;
    }

    assert(std::holds_alternative<ArrType>(backing));

    auto &arr = std::get<ArrType>(backing);
    if (index < arr.size()) {
        return &arr[index];
    }

    return nullptr;
}

size_t Element::GetCount() {
    size_t size = 0;

    switch (type) {
    case Type::Array: {
        assert(std::holds_alternative<ArrType>(backing));
        auto &arr = std::get<ArrType>(backing);
        size = arr.size();
    } break;

    case Type::Object: {
        assert(std::holds_alternative<ObjType>(backing));
        auto &arr = std::get<ObjType>(backing);
        size = arr.size();
    } break;

    case Type::String:
    case Type::Number: {
        std::string_view view;
        if (GetRawStringHelper(view, type)) {
            size = view.size();
        }
    } break;
    default:
        break;
    }

    return 0;
}

bool Element::GetString(std::ostream &s) {
    ViewType view;
    if (!GetRawStringHelper(view, Type::String)) {
        return false;
    }

    size_t begin = 0;

    size_t it = view.find('\\');
    while (it != ViewType::npos) {
        s << view.substr(begin, it - begin);

        switch (view[it + 1]) {
        case 'b':
            s << '\b';
            it += 2;
            break;
        case 'f':
            s << '\f';
            it += 2;
            break;
        case 'n':
            s << '\n';
            it += 2;
            break;
        case 'r':
            s << '\r';
            it += 2;
            break;
        case 't':
            s << '\t';
            it += 2;
            break;
        case '\"':
            s << '\"';
            it += 2;
            break;
        case '\\':
            s << '\\';
            it += 2;
            break;
        default:
            // s << '\\';
            it += 1;
        }

        begin = it;
        it = view.find('\\', begin);
    }

    s << view.substr(begin);

    return true;
}

bool Element::GetString(std::string &str) {
    std::ostringstream s;
    if (GetString(s)) {
        str = s.str();
        return true;
    }

    return false;
}

bool Element::GetRawStringHelper(ViewType &view, Type testType) {
    if (type != testType)
        return false;

    if (std::holds_alternative<ViewType>(backing)) {
        view = std::get<ViewType>(backing);
    } else {
        assert(std::holds_alternative<StrType>(backing));
        view = std::get<StrType>(backing);
    }

    return true;
}

bool Element::ParseToken(StrItr begin, StrItr end, Element::StrItr *term,
                         ViewType token) {
    auto tIt = token.begin();

    while (tIt != token.end()) {
        if (begin == end || *begin++ != *tIt++) {
            backing.emplace<ViewType>("Invalid token");
            return false;
        }
    }

    if (term)
        *term = begin;
    return true;
}

bool Element::ParseNumber(StrItr begin, StrItr end, Element::StrItr *term) {
    auto numEnd = begin + 1;
    while (numEnd != end && std::isdigit(*numEnd) || *numEnd == '.') {
        numEnd++;
    }

    this->backing.emplace<ViewType>(&*begin, std::distance(begin, numEnd));

    if (term) {
        *term = numEnd;
    }

    return true;
}

bool Element::ParseArray(StrItr begin, StrItr end, Element::StrItr *term) {
    ArrType temp;

    begin = FwdSpaces(begin, end);

    while (begin != end) {
        if (*begin == ']') {
            backing.emplace<ArrType>(std::move(temp));
            begin++;

            if (term) {
                *term = begin;
            }

            return true;
        }

        auto &el = temp.emplace_back(Element{begin, end, &begin});
        if (!el.IsValid()) {
            ViewType err;
            el.GetErrorString(err);
            backing.emplace<ViewType>(err);
            return false;
        }

        begin = FwdCommaOrTerm(begin, end, ']');
    }

    backing.emplace<ViewType>("Invalid separator in array");
    return false;
}

bool Element::ParseObject(StrItr begin, StrItr end, Element::StrItr *term) {
    ObjType temp;

    begin = FwdSpaces(begin, end);

    while (begin != end) {
        if (*begin == '}') {
            backing.emplace<ObjType>(std::move(temp));
            if (term)
                *term = begin + 1;
            return true;
        }

        if (begin == end || *begin != '"') {
            backing.emplace<ViewType>("Key not found in object");
            return false;
        }

        auto str = ConsumeString(begin + 1, end);
        if (str == end) {
            backing.emplace<ViewType>(
                "Key not terminated before end of stream");
            return false;
        }

        ViewType key(&*(begin + 1), std::distance(begin + 1, str));

        begin = FwdSpaces(str + 1, end);

        if (begin == end || *begin != ':') {
            backing.emplace<ViewType>("Invalid char after key");
            return false;
        }

        auto [el, success] = temp.emplace(key, Element{begin + 1, end, &begin});
        if (!success || !el->second.IsValid()) {
            ViewType err;
            el->second.GetErrorString(err);
            backing.emplace<ViewType>(err);
            return false;
        }

        begin = FwdCommaOrTerm(begin, end, '}');
    }

    return false;
}

} // namespace StupidJSON