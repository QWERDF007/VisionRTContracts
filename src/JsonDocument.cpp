#include "JsonDocument.h"

#include "visionrt/contracts/TypeSpec.h"

#include <rapidjson/error/en.h>
#include <rapidjson/reader.h>

#include <algorithm>
#include <deque>
#include <string>
#include <utility>

namespace visionrt::decljson {
namespace {

struct Frame
{
    Node        node;
    std::string pendingKey;
    bool        hasKey = false;
};

// Builds the tree while the reader walks the document. Objects keep member order so a rewritten
// file keeps the author's layout; duplicate keys are rejected here because a tree that silently
// keeps the last one would hide a real authoring mistake. Each open container carries its own
// pending key: an object nested as a value must not borrow the key its parent is waiting on.
class TreeBuilder : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, TreeBuilder>
{
public:
    bool Null()
    {
        return attach(Node{});
    }

    bool Bool(bool value)
    {
        Node node;
        node.kind    = Kind::Bool;
        node.boolean = value;
        return attach(std::move(node));
    }

    // Numbers arrive as their source text; the schema layer decides whether the declared type
    // accepts that text, so the tree never rounds an integer through a double.
    bool Int(int value)
    {
        return number(std::to_string(value));
    }
    bool Uint(unsigned value)
    {
        return number(std::to_string(value));
    }
    bool Int64(std::int64_t value)
    {
        return number(std::to_string(value));
    }
    bool Uint64(std::uint64_t value)
    {
        return number(std::to_string(value));
    }
    bool Double(double)
    {
        // Raw numbers are enabled, so the reader never hands a parsed double to this handler.
        return fail("数值解析异常");
    }
    bool RawNumber(const char *text, rapidjson::SizeType length, bool)
    {
        return number(std::string(text, length));
    }

    bool String(const char *text, rapidjson::SizeType length, bool)
    {
        Node node;
        node.kind = Kind::String;
        node.text.assign(text, length);
        return attach(std::move(node));
    }

    bool StartObject()
    {
        return open(Kind::Object);
    }

    bool Key(const char *text, rapidjson::SizeType length, bool)
    {
        if (frames_.empty() || frames_.back().node.kind != Kind::Object)
            return fail("对象键出现在非对象位置");
        auto &frame = frames_.back();
        frame.pendingKey.assign(text, length);
        frame.hasKey = true;
        const auto duplicate = std::any_of(frame.node.members.begin(), frame.node.members.end(),
                                           [&frame](const auto &member) { return member.first == frame.pendingKey; });
        if (duplicate)
            return fail("对象包含重复字段：" + frame.pendingKey);
        return true;
    }

    bool EndObject(rapidjson::SizeType)
    {
        return close(Kind::Object);
    }

    bool StartArray()
    {
        return open(Kind::Array);
    }

    bool EndArray(rapidjson::SizeType)
    {
        return close(Kind::Array);
    }

    Node take()
    {
        return std::move(root_);
    }

    const std::string &error() const noexcept
    {
        return error_;
    }

private:
    bool number(std::string text)
    {
        Node node;
        node.kind = Kind::Number;
        node.text = std::move(text);
        return attach(std::move(node));
    }

    bool open(Kind kind)
    {
        if (frames_.size() >= kMaxDepth)
            return fail("JSON 嵌套超过 " + std::to_string(kMaxDepth) + " 层");
        Frame frame;
        frame.node.kind = kind;
        frames_.push_back(std::move(frame));
        return true;
    }

    bool close(Kind kind)
    {
        if (frames_.empty() || frames_.back().node.kind != kind)
            return fail(kind == Kind::Object ? "对象结束标记不匹配" : "数组结束标记不匹配");
        auto node = std::move(frames_.back().node);
        frames_.pop_back();
        return attach(std::move(node));
    }

    // Hands a finished value to the container that is currently open. The root has no container, so
    // it is stored directly.
    bool attach(Node node)
    {
        if (frames_.empty())
        {
            if (haveRoot_)
                return fail("JSON 文档包含多个根值");
            root_     = std::move(node);
            haveRoot_ = true;
            return true;
        }
        auto &frame = frames_.back();
        if (frame.node.kind == Kind::Object)
        {
            if (!frame.hasKey)
                return fail("对象值缺少键");
            frame.node.members.emplace_back(std::move(frame.pendingKey), std::move(node));
            frame.pendingKey.clear();
            frame.hasKey = false;
            return true;
        }
        frame.node.items.push_back(std::move(node));
        return true;
    }

    bool fail(std::string message)
    {
        if (error_.empty())
            error_ = std::move(message);
        return false;
    }

    std::deque<Frame> frames_;
    Node              root_;
    bool              haveRoot_ = false;
    std::string       error_;
};

} // namespace

const Node *Node::find(std::string_view key) const
{
    for (const auto &[name, value] : members)
        if (name == key)
            return &value;
    return nullptr;
}

Node parse(std::string_view json)
{
    rapidjson::Reader reader;
    TreeBuilder       builder;
    rapidjson::MemoryStream stream(json.data(), json.size());
    const auto result = reader.Parse<rapidjson::kParseNumbersAsStringsFlag | rapidjson::kParseValidateEncodingFlag>(
        stream, builder);
    if (result.IsError())
    {
        if (!builder.error().empty())
            throw ContractError(builder.error());
        throw ContractError(std::string("JSON 解析失败（位置 ") + std::to_string(result.Offset()) + "）："
                            + rapidjson::GetParseError_En(result.Code()));
    }
    if (!builder.error().empty())
        throw ContractError(builder.error());
    return builder.take();
}

} // namespace visionrt::decljson
