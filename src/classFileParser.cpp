#include "classFileParser.h"
#include <fstream>

// 辅助函数：大端序读取
uint16_t read_u1(std::ifstream& in) {
    uint8_t bytes[1];
    in.read(reinterpret_cast<char*>(bytes), 1);
    return bytes[0];
}

uint16_t read_u2(std::ifstream& in) {
    uint8_t bytes[2];
    in.read(reinterpret_cast<char*>(bytes), 2);
    return (bytes[0] << 8) | bytes[1];
}

uint32_t read_u4(std::ifstream& in) {
    uint8_t bytes[4];
    in.read(reinterpret_cast<char*>(bytes), 4);
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

std::optional<ClassInfo> ClassFileParser::parse(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    ClassInfo class_file;
    // 读取魔数
    uint32_t magic = read_u4(in);
    if (magic != 0xCAFEBABE) {
        throw std::runtime_error("Invalid class file magic number");
    }

    // 解析版本号
    class_file.minorVer = read_u2(in); // minor
    class_file.majorVer = read_u2(in); // major

    // 解析常量池
    uint16_t cp_count = read_u2(in);
    for (int i = 1; i < cp_count; ++i) { // 常量池索引从1开始
        uint8_t tag = read_u1(in);
        fmt::print("parsing tag type: {}\n", tag);
        ConstantPoolInfo cp_info;
        cp_info.tag = tag;
        switch (tag) {
            case 7: { // CONSTANT_Class
                cp_info.class_name_index = read_u2(in);
                break;
            }
            case 9: { // CONSTANT_Fieldref
                cp_info.fieldref_class_index = read_u2(in);
                cp_info.fieldref_name_type_index = read_u2(in);
                break;
            }
            case 10: { // CONSTANT_Methodref
                cp_info.methodref_class_index = read_u2(in);
                cp_info.methodref_name_type_index = read_u2(in);
                break;
            }
            case 8: { // CONSTANT_String
                cp_info.string_index = read_u2(in);
                break;
            }
            case 3: // CONSTANT_Integer
            case 4: // CONSTANT_Float
            {
                cp_info.integerOrFloat = read_u4(in);
                break;
            }
            case 5: // CONSTANT_Long
            case 6: // CONSTANT_Double
            {
                cp_info.longOrDouble_high_bytes = read_u4(in);
                cp_info.longOrDouble_low_bytes = read_u4(in);
                class_file.constant_pool.add_constant(cp_info);
                i++; // 跳过下一个无效槽
                cp_info.tag = 0;
                class_file.constant_pool.add_constant(cp_info);
                continue;
            }
            case 12: { // CONSTANT_NameAndType
                cp_info.name_index = read_u2(in);
                cp_info.descriptor_index = read_u2(in);
                break;
            }
            case 1: { // CONSTANT_Utf8
                uint16_t len = read_u2(in);
                cp_info.utf8_str.resize(len);
                in.read(&cp_info.utf8_str[0], len);
                break;
            }
            case 15: { // CONSTANT_MethodHandle
                cp_info.reference_kind = read_u1(in);
                cp_info.reference_index = read_u2(in);
                break;
            }
            case 16: { // CONSTANT_MethodType
                cp_info.descriptor_index_mt = read_u2(in);
                break;
            }
            case 18: { // CONSTANT_InvokeDynamic
                cp_info.bootstrap_method_attr_index = read_u2(in);
                cp_info.name_and_type_index = read_u2(in);
                break;
            }
            default: {
                throw std::runtime_error("Unsupported constant pool tag: " + std::to_string(tag));
            }
        }
        class_file.constant_pool.add_constant(std::move(cp_info));
    }
    class_file.constant_pool.print_all();

    // 解析访问标志、类名、父类名等
    uint16_t access_flags= read_u2(in);
    class_file.this_class = read_u2(in);
    class_file.super_class = read_u2(in);

    // 解析接口
    uint16_t interfaces_count = read_u2(in);
    in.ignore(interfaces_count * 2);

    // 解析字段
    uint16_t fields_count = read_u2(in);
    for (int i = 0; i < fields_count; ++i) {
        uint16_t field_access_flags= read_u2(in);
        uint16_t field_name_index = read_u2(in);
        uint16_t field_descriptor_index = read_u2(in);
        uint16_t field_attributes_count = read_u2(in);
        for (int i = 0; i < field_attributes_count; ++i) {
            uint16_t attribute_name_index= read_u2(in);
            uint16_t attribute_length = read_u4(in);
            in.ignore(attribute_length);
        }
    }

    // 解析方法
    uint16_t methods_count = read_u2(in);
    for (int i = 0; i < methods_count; ++i) {
        MethodInfo method;

        uint16_t access_flags = read_u2(in);
        method.access_flags = access_flags;
        uint16_t name_idx = read_u2(in);
        uint16_t desc_idx = read_u2(in);
        method.name = class_file.constant_pool.get_utf8_str(name_idx);
        method.descriptor = class_file.constant_pool.get_utf8_str(desc_idx);

        // fmt::print("parsing method {}\n", method.name);

        // 解析属性
        bool code_found = false;
        uint16_t attr_count = read_u2(in);
        for (int j = 0; j < attr_count; ++j) {
            AttributeInfo attr = parseAttributeInfo(in, class_file.constant_pool);
            if (attr.name == "Code") {
                method.code = std::move(attr.code->code);
                method.max_stack = attr.code->max_stack;
                method.max_locals = attr.code->max_locals;
                code_found = true;
            }
        }
        if (!code_found) {
            throw std::runtime_error("Found a method without Code attribute");
        }
        class_file.methods.push_back(method);
    }
    return class_file;
}

AttributeInfo ClassFileParser::parseAttributeInfo(std::ifstream& in, const ConstantPool& cp) {
    AttributeInfo attr;
    uint16_t attr_name_idx = read_u2(in);
    uint32_t attr_len = read_u4(in);
    attr.name = cp.get_utf8_str(attr_name_idx);
    if (attr.name == "Code") {
        std::unique_ptr<CodeAttribute> code_attr = std::make_unique<CodeAttribute>();
        code_attr->max_stack = read_u2(in);
        code_attr->max_locals = read_u2(in);
        uint32_t code_len = read_u4(in);
        code_attr->code.resize(code_len);
        in.read(reinterpret_cast<char*>(code_attr->code.data()), code_len);
        // 异常表
        uint16_t exception_table_len = read_u2(in);
        for (int k = 0; k < exception_table_len; ++k) {
            uint16_t start_pc = read_u2(in);
            uint16_t end_pc = read_u2(in);
            uint16_t handler_pc = read_u2(in);
            uint16_t catch_type = read_u2(in);
        }
        // 属性
        uint16_t subattributes_count = read_u2(in);
        for (int i = 0; i < subattributes_count; ++i) {
            AttributeInfo attr = parseAttributeInfo(in, cp);
        }
        attr = AttributeInfo(attr.name, std::move(code_attr));
    } else {
        std::vector<uint8_t> unkown_info(attr_len);
        in.read(reinterpret_cast<char*>(unkown_info.data()), attr_len);
        attr = AttributeInfo(attr.name, unkown_info);
    }
    return attr;
}
