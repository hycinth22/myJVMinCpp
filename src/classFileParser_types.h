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
    AttributeInfo() : ty(TYPE::EMPTY) {}
    AttributeInfo(AttributeInfo&& rhs) {
        ty = rhs.ty;
        if (ty==TYPE::CODE) {
            code = std::move(rhs.code);
        } else if (ty==TYPE::UNKOWN) {
            unkown_info = std::move(rhs.unkown_info);
        }
    }
    // ~AttributeInfo() {
    //     if (ty==TYPE::CODE) {
    //         code.~unique_ptr();
    //     } else if (ty==TYPE::UNKOWN) {
    //         unkown_info.~vector<uint8_t>();
    //     }
    // }
    AttributeInfo& operator=(AttributeInfo&& rhs) {
        this->~AttributeInfo();
        ty = rhs.ty;
        if (ty==TYPE::CODE) {
            code = std::move(rhs.code);
        } else if (ty==TYPE::UNKOWN) {
            unkown_info = std::move(rhs.unkown_info);
        }
        return *this;
    }
    AttributeInfo(std::string name, std::unique_ptr<CodeAttribute> code) : ty(TYPE::CODE), name(name), code(move(code)) {}
    AttributeInfo(std::string name, std::vector<uint8_t> unkown_info)  : ty(TYPE::UNKOWN), name(name), unkown_info(unkown_info) {}
    std::string name;
    std::unique_ptr<CodeAttribute> code;
    std::vector<uint8_t> unkown_info;
private:
    enum class TYPE {EMPTY, CODE, UNKOWN};
    TYPE ty;
};

struct CodeAttribute {
    uint16_t max_stack;
    uint16_t max_locals;
    std::vector<uint8_t> code;
    std::vector<AttributeInfo> exception_table;
    std::vector<AttributeInfo> attributes;
};

#endif //CONSTANTPOOLINFO_H
