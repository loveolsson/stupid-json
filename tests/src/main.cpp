#include "stupid-json/stupid.hpp"
#include "gtest/gtest.h"
#include <fstream>
#include <iostream>

using namespace StupidJSON;

static std::string path;

TEST(Basic, Class) {
    std::cout << "Size of Element: " << sizeof(Element) << std::endl;

    EXPECT_TRUE(std::is_move_constructible<Element>::value);
}

TEST(Parsing, Simple) {
    std::ifstream s(path + "/samples/test1.json");
    std::string body((std::istreambuf_iterator<char>(s)),
                     std::istreambuf_iterator<char>());

    Element root(body);

    if (!root.IsValid()) {
        std::string_view err;
        root.GetErrorString(err);
        std::cout << "JSON Error: " << err << std::endl;
    }

    EXPECT_EQ(root.GetType(), Element::Type::Object);

    auto hello = root.GetMember("hello");
    EXPECT_TRUE(hello);

    if (hello) {
        EXPECT_EQ(hello->GetType(), Element::Type::String);
        std::string_view raw;
        hello->GetRawString(raw);

        std::string str;
        hello->GetString(str);

        // std::cout << raw << std::endl;
        // std::cout << str << std::endl;
    }

    EXPECT_TRUE(true);
}

int main(int argc, char **argv) {
    if (argc == 3) {
        path = argv[2];
        std::cout << "Using path: " << path << std::endl;
    }

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}