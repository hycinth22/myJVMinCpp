#ifndef CONSTANTPOOLINFO_H
#define CONSTANTPOOLINFO_H
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

// 常量池类型定义
struct ConstantPoolInfo {
    // 公共字段
    uint8_t tag;
    // tag=7: 类名常量
    uint16_t class_name_index;
    // tag=9: 字段引用 (Fieldref)
    uint16_t fieldref_class_index;
    uint16_t fieldref_name_type_index;
    // tag=10: 方法引用常量（tag=10）
    uint16_t methodref_class_index;
    uint16_t methodref_name_type_index;
    // tag=8: String
    uint16_t string_index;
    // tag=3/4: Integer、Float
    uint32_t integerOrFloat;
    // tag=5/6: Long/Double
    uint32_t longOrDouble_high_bytes;
    uint32_t longOrDouble_low_bytes;
    // tag=12: 名称与类型描述符 (NameAndType)
    uint16_t name_index;
    uint16_t descriptor_index;
    // tag=1: UTF-8字符串常量
    std::string utf8_str;
    // 其他类型可在此扩展
};

// 方法信息结构
struct MethodInfo {
    uint16_t access_flags;
    std::string name;
    std::string descriptor;
    std::vector<uint8_t> code;
    uint16_t max_stack;
    uint16_t max_locals;
};

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

// 类文件结构
class ClassFile {
public:
    std::vector<ConstantPoolInfo> constant_pool;
    std::vector<MethodInfo> methods;
    uint16_t majorVer, minorVer;
};

#endif //CONSTANTPOOLINFO_H
