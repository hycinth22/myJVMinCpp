#ifndef CONSTANTPOOLINFO_H
#define CONSTANTPOOLINFO_H
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "constantPool.h"
#include "runtime.h"

struct ExceptionTable {
    uint16_t start_pc;
    uint16_t end_pc;
    uint16_t handler_pc;
    uint16_t catch_type;
};

struct CodeAttribute;
struct AttributeInfo {
    AttributeInfo() = default;
    AttributeInfo(AttributeInfo&&) = default;
    AttributeInfo& operator=(AttributeInfo&&) = default;
    AttributeInfo(const AttributeInfo&) = default;
    AttributeInfo& operator=(const AttributeInfo&) = default;
    AttributeInfo(std::string name, std::unique_ptr<CodeAttribute> code)
        : name(std::move(name)), code(std::move(code)) {}
    AttributeInfo(std::string name, std::vector<uint8_t> unkown_info)
        : name(std::move(name)), unkown_info(std::move(unkown_info)) {}

    std::string name;
    std::unique_ptr<CodeAttribute> code;
    std::vector<uint8_t> unkown_info;
};

struct CodeAttribute {
    uint16_t max_stack;
    uint16_t max_locals;
    std::vector<uint8_t> code;
    std::vector<AttributeInfo> exception_table;
    std::vector<AttributeInfo> attributes;
};

#endif //CONSTANTPOOLINFO_H
