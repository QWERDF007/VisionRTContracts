#pragma once

// A JSON tree that keeps every number's original text. Declarations store exact integers (a port
// default of 2^53+1 must survive a round trip) and RapidJSON's default DOM stores numbers as
// double, so the reader builds this tree through a SAX handler that hands numbers over as raw
// text. Nothing here is installed: it is the private reading substrate of DeclarationJson.cpp.

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace visionrt::decljson {

enum class Kind
{
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct Node
{
    Kind        kind    = Kind::Null;
    bool        boolean = false;
    // Raw number text for Kind::Number, the decoded content for Kind::String.
    std::string text;
    std::vector<Node>                        items;
    std::vector<std::pair<std::string, Node>> members;

    const Node *find(std::string_view key) const;
    bool        has(std::string_view key) const
    {
        return find(key) != nullptr;
    }
};

// Parses one document. Rejects malformed JSON, invalid UTF-8, duplicate object keys, trailing
// content and nesting deeper than kMaxDepth with a message naming the byte offset.
Node parse(std::string_view json);

inline constexpr std::size_t kMaxDepth = 64;

} // namespace visionrt::decljson
