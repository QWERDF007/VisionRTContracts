#include "visionrt/contracts/ValueJson.h"

#include "DeclarationValue.h"

namespace visionrt {
struct ValueJsonDocument::Impl
{
    decljson::Node root;
};
ValueJsonDocument::ValueJsonDocument(std::string_view json)
    : document_(std::make_shared<Impl>(Impl{decljson::parse(json)})) {}

PortValue ValueJsonDocument::read(std::span<const std::string> path, const TypeSpec &type) const
{
    const auto *node = &document_->root;
    for (const auto &segment : path)
    {
        if (node->kind == decljson::Kind::Array)
        {
            const auto index = parseInt64(segment);
            if (index < 0 || static_cast<std::size_t>(index) >= node->items.size()) throw ContractError("JSON 数组路径不存在：" + segment);
            node = &node->items[static_cast<std::size_t>(index)];
        }
        else node = node->find(segment);
        if (!node) throw ContractError("JSON 值路径不存在：" + segment);
    }
    return decljson::decodeValue(*node, type);
}

PortValue readValueJson(std::string_view json, const TypeSpec &type)
{
    return ValueJsonDocument(json).read({}, type);
}
std::string writeValueJson(const PortValue &value, const TypeSpec &type)
{
    rapidjson::StringBuffer buffer;
    decljson::Writer writer(buffer);
    decljson::encodeValue(writer, value, type);
    return {buffer.GetString(), buffer.GetSize()};
}
}
