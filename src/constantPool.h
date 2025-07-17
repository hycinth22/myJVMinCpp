// 常量池封装
#include <stddef.h>
#include <string>
#include <vector>
#include "fmt/core.h"

enum ConstantType { 
    UTF8 = 1,
    INTEGER = 3,
    FLOAT = 4,
    CLASS = 7,
    STRING = 8,
    FIELD_REF = 9,
    METHOD_REF = 10,
    NAME_AND_TYPE = 12,
};

// 常量池类型定义
struct ConstantPoolInfo {
    // 公共字段：类型标识
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
};

class ConstantPool {
    public:
        std::vector<ConstantPoolInfo> pool;
        ConstantPool() :pool(1) {}
        ConstantPool(const std::vector<ConstantPoolInfo>& cp) : pool(cp) {}
        size_t size() const { return pool.size(); }
        const ConstantPoolInfo& operator[](size_t idx) const { return pool[idx]; }
        ConstantPoolInfo& operator[](size_t idx) { return pool[idx]; }
        // 打印所有常量池内容
        void print_all() const;
        void add_constant(ConstantPoolInfo && c) {
            pool.push_back(std::move(c));
        } 

        const std::string& get_utf8_str(uint16_t index) const {
            if (index == 0 || index > pool.size()) {
                fmt::print("ConstantPool.get_utf8_str: index {} out of bound", index);
                exit(1);
            }
            if (pool[index].tag != ConstantType::UTF8) {
                fmt::print("ConstantPool.get_utf8_str: index {} is not utf8 entry", index);
                exit(1);
            }
            return pool[index].utf8_str;
        }

        const std::string& get_class_name(uint16_t index) const {
            if (index == 0 || index > pool.size()) {
                fmt::print("ConstantPool.get_class_name: index {} out of bound", index);
                exit(1);
            }
            if (pool[index].tag != ConstantType::CLASS) {
                fmt::print("ConstantPool.get_class_name: index {} is not class entry", index);
                exit(1);
            }
            return get_utf8_str(pool[index].class_name_index);
        }

        uint16_t get_string_idx(uint16_t index) const {
            if (index == 0 || index > pool.size()) {
                fmt::print("ConstantPool.get_string_idx: index {} out of bound", index);
                exit(1);
            }
            if (pool[index].tag != ConstantType::STRING) {
                fmt::print("ConstantPool.get_string_idx: index {} is not string entry", index);
                exit(1);
            }
            return pool[index].string_index;
        }


        std::pair<std::string, std::string> get_name_and_type(uint16_t index) const {
            if (index == 0 || index > pool.size()) {
                fmt::print("ConstantPool.get_name_and_type: index {} out of bound", index);
                exit(1);
            }
            if (pool[index].tag != ConstantType::NAME_AND_TYPE) {
                fmt::print("ConstantPool.get_name_and_type: index {} is not name_and_type entry", index);
                exit(1);
            }

            const ConstantPoolInfo& nt = pool[index];
            std::string name = get_utf8_str(nt.name_index);
            std::string desc = get_utf8_str(nt.descriptor_index);
            return {name, desc};
        }

    };
      
    