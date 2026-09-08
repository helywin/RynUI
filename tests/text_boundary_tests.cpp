#include "input/text_boundary.hpp"

#if __has_include(<utf8proc.h>)
#error utf8proc headers must not propagate through the boundary target
#endif
#ifdef UTF8PROC_STATIC
#error utf8proc compile definitions must remain private
#endif

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <sstream>

namespace {
void require(bool value, const char* message) {
    if(!value) { throw std::runtime_error(message); }
}
std::string bytes(std::u8string_view text) {
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
void expect(std::string_view text, std::initializer_list<std::size_t> expected) {
    ryn::input::TextBoundaryMap map;
    require(map.assign(text), "valid input rejected");
    require(std::ranges::equal(map.grapheme_bytes(), expected), "grapheme mismatch");
    for(std::size_t i = 0; i <= map.scalar_count(); ++i) {
        require(map.byte_to_scalar(*map.scalar_to_byte(i)) == i, "scalar roundtrip");
    }
    for(std::size_t i = 0; i <= text.size(); ++i) {
        require(map.floor(i) <= i && map.ceil(i) >= i, "clamp direction");
        require(map.is_boundary(map.floor(i)) && map.is_boundary(map.ceil(i)), "clamp boundary");
    }
}
void unit_tests() {
    using ryn::input::TextBoundaryMap;
    require(TextBoundaryMap::dependency_version() == "2.11.3", "dependency version");
    require(TextBoundaryMap::unicode_version() == "17.0.0", "Unicode version");
    expect("", {0});
    expect("abc", {0,1,2,3});
    expect(std::string_view("a\0b", 3), {0,1,2,3});
    expect(bytes(u8"a\u0301中"), {0,3,6});
    expect(bytes(u8"中文"), {0,3,6});
    expect(bytes(u8"👍🏽"), {0,8});
    expect(bytes(u8"🇨🇳🇺🇸🇯"), {0,8,16,20});
    expect(bytes(u8"👨‍👩‍👧‍👦"), {0,25});
    expect("\r\n", {0,2});
    expect(bytes(u8"각"), {0,9});
    TextBoundaryMap map;
    require(map.is_boundary(0) && map.scalar_count() == 0, "default empty");
    require(map.assign(bytes(u8"中a")), "initial map");
    for(const auto& invalid : {std::string("\x80"), std::string("\xc0\xaf"),
            std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"),
            std::string("\xe4\xb8"), std::string("a\xff"), std::string("\xf5\x80\x80\x80")}) {
        require(!map.assign(invalid), "invalid UTF-8 accepted");
        require(map.size_bytes() == 4 && map.scalar_count() == 2 && map.is_boundary(3), "rollback");
    }
    require(!map.byte_to_scalar(1) && !map.byte_to_scalar(2), "interior scalar byte");
    const auto huge = std::numeric_limits<std::size_t>::max();
    require(!map.byte_to_scalar(huge) && !map.scalar_to_byte(huge), "large index");
    require(map.floor(huge) == 4 && map.ceil(huge) == 4, "large clamp");
    require(map.previous(huge) == 4 && map.next(huge) == 4, "large navigation");
    require(map.previous(0) == 0 && map.next(0) == 3 && map.previous(3) == 0, "navigation");
    bool overflow = false;
    try { map.reserve(huge); } catch(const std::length_error&) { overflow = true; }
    require(overflow && map.size_bytes() == 4, "reserve overflow");
    const std::string large(100000, 'a');
    map.reserve(large.size());
    require(map.assign(large) && map.scalar_count() == large.size(), "large input");
    const auto* identity = map.grapheme_bytes().data();
    for(std::size_t i = 0; i < 10000; ++i) {
        require(map.next(i) == i + 1 && map.grapheme_bytes().data() == identity, "retained map");
    }
    ryn::input::Utf8ScalarIterator iterator(std::string_view("a\0\xff", 3));
    require(iterator.next()->value == U'a' && iterator.next()->value == U'\0', "iterator NUL");
    require(!iterator.next() && !iterator.valid() && iterator.offset() == 2 && !iterator.next(), "iterator failure");
}
void corpus(const char* path) {
    std::ifstream stream(path);
    require(stream.good(), "corpus missing");
    std::string line;
    std::size_t count = 0;
    while(std::getline(stream, line)) {
        if(line.empty() || line.front() == '#') { continue; }
        const auto split = line.find(' ');
        require(split != std::string::npos, "bad fixture");
        std::string text;
        for(std::size_t i = 0; i < split; i += 2) {
            text.push_back(static_cast<char>(std::stoul(line.substr(i, 2), nullptr, 16)));
        }
        std::vector<std::size_t> expected;
        std::istringstream offsets(line.substr(split + 1));
        std::string number;
        while(std::getline(offsets, number, ',')) { expected.push_back(std::stoull(number)); }
        ryn::input::TextBoundaryMap map;
        require(map.assign(text), "corpus decode");
        if(!std::ranges::equal(map.grapheme_bytes(), expected)) {
            throw std::runtime_error("UAX#29 case " + std::to_string(count + 1));
        }
        ++count;
    }
    require(count > 700, "incomplete corpus");
    std::cout << "UAX#29 Unicode 17: " << count << " cases passed\n";
}
}
int main(int argc, char** argv) {
    try {
        unit_tests();
        if(argc == 2) { corpus(argv[1]); }
        std::cout << "Text boundary tests passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
