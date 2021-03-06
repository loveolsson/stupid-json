#include "stupid-json/arena.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace StupidJSON;

static std::string path = "./";
static std::string canadaBody;
static std::string twitterBody;
static std::string citmBody;

static std::string ReadFile(std::string_view name) {
    std::string p(path);
    p += name;
    std::ifstream s(p);
    std::string body((std::istreambuf_iterator<char>(s)),
                     std::istreambuf_iterator<char>());
    return body;
}

TEST(Basic, Class) {
    EXPECT_EQ(sizeof(Element), 56);
    EXPECT_TRUE(std::is_move_constructible<Element>::value);
}

TEST(Allocator, StringPush) {
    ArenaAllocator arena;

    auto a = arena.PushString("Hello");
    auto b = arena.PushString("Foo");
    auto c = arena.PushString("Hello");

    EXPECT_EQ(a, "Hello");
    EXPECT_EQ(a, c);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);

    for (int i = 0; i < 1000; ++i) {
        auto d = arena.PushString("Hello");
        EXPECT_EQ(a, d);
    }
}

TEST(Allocator, Move) {
    ArenaAllocator arena;

    auto a = arena.PushString("Hello");
    auto b = arena.PushString("Foo");
    auto c = arena.PushString("Hello");

    // Make sure we don't double free after move
    ArenaAllocator arena2(std::move(arena));
}

TEST(Parsing, Simple) {
    auto body = ReadFile("/samples/test1.json");

    ArenaAllocator arena;
    auto root = arena.CreateElement();
    root->ParseBody({body.data(), body.size()}, arena);

    if (root->type == Element::Type::Error) {
        std::cout << "JSON Error: " << root->ref << std::endl;
    }

    EXPECT_EQ(root->type, Element::Type::Object);

    auto hello = root->FindChildElement("hello", arena);
    EXPECT_TRUE(hello);

    if (hello) {
        EXPECT_EQ(hello->type, Element::Type::String);
    }

    EXPECT_TRUE(true);
}

TEST(Parsing, BigDoc) {
    auto body = ReadFile("/samples/test2.json");

    ArenaAllocator arena;
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody({body.data(), body.size()}, arena));
    EXPECT_TRUE(root->type == Element::Type::Object);

    if (root->type == Element::Type::Error) {
        std::cout << root->ref << std::endl;
    }

    auto child = root->FindChildElement("web-app", arena);
    EXPECT_TRUE(child);

    if (child) {
        int i = 0;

        EXPECT_TRUE(child->IterateObject(arena, [&i](auto key, auto obj) {
            // std::cout << "Key: " << key << std::endl;
            i++;
        }));

        EXPECT_EQ(i, 3);
        EXPECT_EQ(i, child->childCount);
    }
}

TEST(ValueParse, Simple) {
    ArenaAllocator arena;
    auto body = "{\"a\": 5, \"b\": -10, \"c\": 5.3 }";
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody(body, arena));

    auto a = root->FindChildElement("a", arena);
    auto b = root->FindChildElement("b", arena);
    auto c = root->FindChildElement("c", arena);

    EXPECT_TRUE(a && b && c);

    int aV, bV;
    float cV;

    EXPECT_TRUE(a->GetInteger(aV));
    EXPECT_TRUE(b->GetInteger(bV));
    EXPECT_TRUE(c->GetFloatingPoint(cV));

    EXPECT_TRUE(a->SetNumber(4, arena));
    EXPECT_TRUE(b->SetNumber(-4, arena));
    EXPECT_TRUE(c->SetNumber(4.5, arena));

    EXPECT_EQ(aV, 5);
    EXPECT_EQ(bV, -10);
    EXPECT_FLOAT_EQ(cV, 5.3);
}

TEST(Parsing, HugeDoc) {
    ArenaAllocator arena;
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody({canadaBody.data(), canadaBody.size()}, arena));
    EXPECT_TRUE(root->type == Element::Type::Object);

    if (root->type == Element::Type::Error) {
        std::cout << root->ref << std::endl;
    }

    auto features = root->FindChildElement("features", arena);
    EXPECT_TRUE(features);
    EXPECT_EQ(features->type, Element::Type::Array);
    EXPECT_EQ(features->childCount, 1);

    auto feature = features->GetArrayIndex(0);
    EXPECT_TRUE(feature);
    auto geometry = feature->FindChildElement("geometry", arena);
    EXPECT_TRUE(geometry);
    auto coordinates = geometry->FindChildElement("coordinates", arena);
    EXPECT_TRUE(coordinates);

    testing::MockFunction<void()> mockCallback;
    EXPECT_CALL(mockCallback, Call()).Times(testing::Exactly(480));

    coordinates->IterateArray([&](auto index, auto elem) {
        EXPECT_EQ(elem->type, Element::Type::Array);
        mockCallback.Call();
    });
}

TEST(Parsing, BigThree) {
    ArenaAllocator allocator;
    auto twitter = allocator.CreateElement();
    auto canada = allocator.CreateElement();
    auto citm = allocator.CreateElement();

    twitter->ParseBody({twitterBody.data(), twitterBody.size()}, allocator);
    canada->ParseBody({canadaBody.data(), canadaBody.size()}, allocator);
    citm->ParseBody({citmBody.data(), citmBody.size()}, allocator);

    EXPECT_NE(twitter->type, Element::Type::Error);
    EXPECT_NE(canada->type, Element::Type::Error);
    EXPECT_NE(citm->type, Element::Type::Error);
}

TEST(Parsing, BigDocMalformedNumber) {
    auto body = ReadFile("/samples/test2_malformed_number.json");

    ArenaAllocator arena;
    auto root = arena.CreateElement();
    EXPECT_FALSE(root->ParseBody(StringView{body.data(), body.size()}, arena));
}

TEST(Parsing, WithCopy) {
    ArenaAllocator arena;
    StringView view;

    {
        auto body = ReadFile("/samples/test2.json");
        view = arena.PushString({body.data(), body.size()});
        body = "foo";
    }

    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody(view, arena));
    EXPECT_TRUE(root->type == Element::Type::Object);
}

TEST(Parsing, MultiLine) {
    ArenaAllocator arena;
    auto body = ReadFile("/tmp/one-json-per-line.jsons");

    std::string line;
    std::istringstream s(body);
    while (std::getline(s, line)) {
        auto root = arena.CreateElement();

        EXPECT_TRUE(root->ParseBody({line.data(), line.size()}, arena));
        EXPECT_TRUE(root->type == Element::Type::Object);

        arena.Reset();
    }
}

TEST(Malformed, NumberPlus) {
    ArenaAllocator arena;
    auto body_valid = "{\"a\": -4 }";
    auto body_malformed = "{\"a\": +4 }";
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody(body_valid, arena));
    int val;
    root->FindChildElement("a", arena)->GetInteger(val);
    EXPECT_EQ(val, -4);

    EXPECT_FALSE(root->ParseBody(body_malformed, arena));
}

TEST(Malformed, NumberExtraDot) {
    ArenaAllocator arena;
    auto body_valid = "{\"a\": 4.0 }";
    auto body_malformed = "{\"a\": 4.0.0 }";
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody(body_valid, arena));
    EXPECT_FALSE(root->ParseBody(body_malformed, arena));
}

TEST(Malformed, StringEscapedQuote) {
    ArenaAllocator arena;
    auto body_valid = "{\"a\": \"b\" }";
    auto body_malformed = "{\"a\": \"b\\\" }";
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody(body_valid, arena));
    EXPECT_FALSE(root->ParseBody(body_malformed, arena));
}

TEST(Malformed, ObjectMissingKey) {
    ArenaAllocator arena;
    auto body_valid = "{\"a\": \"b\" }";
    auto body_malformed = "{ \"b\" }";
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody(body_valid, arena));
    EXPECT_FALSE(root->ParseBody(body_malformed, arena));
}

TEST(STLTypes, Vector) {
    ArenaAllocator arena;
    auto body = "[1, 2, 3, 6, 7, 8]";
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody(body, arena));

    auto vec = root->GetChildrenAsVector();
    EXPECT_EQ(vec.size(), 6);
    EXPECT_EQ(vec[2]->ref, "3");
}

TEST(STLTypes, Map) {
    ArenaAllocator arena;
    auto body = "{\"a\": 1, \"b\": 2, \"c\": 3}";
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody(body, arena));

    auto vec = root->GetObjectAsMap(arena);
    EXPECT_TRUE(vec.count("b"));
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec["b"]->ref, "2");
}

TEST(Serialize, Simple) {
    // auto body = ReadFile("/samples/test2.json");
    auto &body = citmBody;

    ArenaAllocator arena;
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody({body.data(), body.size()}, arena));
    EXPECT_TRUE(root->type == Element::Type::Object);

    std::ostringstream s;
    EXPECT_TRUE(root->Serialize(arena, s));
    auto str = s.str();
    EXPECT_TRUE(str.size() > 1000);
}

#if 1
TEST(Serialize, ToFile) {
    // auto body = ReadFile("/samples/test2.json");
    // auto &body = citmBody;
    auto body = ReadFile("/samples/test1.json");

    ArenaAllocator arena;
    auto root = arena.CreateElement();
    EXPECT_TRUE(root->ParseBody({body.data(), body.size()}, arena));
    EXPECT_TRUE(root->type == Element::Type::Object);

    auto hello = root->FindChildElement("hello", arena);
    auto view = hello->GetString(arena);
    // std::cout << view.ToStd() << std::endl;
    EXPECT_TRUE(view == "ffǌdsfsd😄🐶");
    hello->SetString(view);

    std::string tempDir = path + "/tmp";
    if (!std::filesystem::is_directory(tempDir)) {
        std::filesystem::create_directory(tempDir);
    }

    if (std::filesystem::is_directory(tempDir)) {
        std::ofstream s(tempDir + "/dump.json");
        EXPECT_TRUE(s.is_open());
        EXPECT_TRUE(root->Serialize(arena, s));
        s.close();
    }
}
#endif

int main(int argc, char **argv) {
    if (argc == 3) {
        path = argv[2];
        std::cout << "Using path: " << path << std::endl;
    }

    canadaBody = ReadFile("/tmp/canada.json");
    twitterBody = ReadFile("/tmp/twitter.json");
    citmBody = ReadFile("/tmp/citm_catalog.json");

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}