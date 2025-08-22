#pragma once
// 常量池封装
#include <stddef.h>
#include <string>
#include <vector>
#include "fmt/core.h"

using ConstIdxT = uint16_t;
enum ConstantType { 
    CLASS = 7,
    FIELD_REF = 9,
    METHOD_REF = 10,
    INTERFACE_METHOD_REF = 11,
    STRING = 8,
    INTEGER = 3,
    FLOAT = 4,
    LONG = 5,
    DOUBLE = 6,
    NAME_AND_TYPE = 12,
    UTF8 = 1,
    METHOD_HANDLE = 15,
    METHOD_TYPE = 16,
    INVOKE_DYNAMIC = 18,
};

// 常量池类型定义
struct ConstantPoolInfo {
    // 公共字段：类型标识
    uint8_t tag;
    // tag=7: 类名常量
    ConstIdxT class_name_index;
    // tag=9: 字段引用 (Fieldref)
    ConstIdxT fieldref_class_index;
    ConstIdxT fieldref_name_type_index;
    // tag=10: 方法引用常量（tag=10）
    ConstIdxT methodref_class_index;
    ConstIdxT methodref_name_type_index;
    // tag=8: String
    ConstIdxT string_index;
    // tag=3/4: Integer、Float
    uint32_t integerOrFloat;
    // tag=5/6: Long/Double
    uint32_t longOrDouble_high_bytes;
    uint32_t longOrDouble_low_bytes;
    // tag=12: 名称与类型描述符 (NameAndType)
    ConstIdxT name_index;
    ConstIdxT descriptor_index;
    // tag=1: UTF-8字符串常量
    std::string utf8_str;
    // tag=15: MethodHandle
    uint8_t reference_kind;
    ConstIdxT reference_index;
    // tag=16: MethodType
    ConstIdxT descriptor_index_mt;
    // tag=18: InvokeDynamic
    ConstIdxT bootstrap_method_attr_index;
    ConstIdxT name_and_type_index;
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
        void add_constant(const ConstantPoolInfo & c) {
            pool.push_back(c);
        } 
        void add_constant(ConstantPoolInfo && c) {
            pool.push_back(std::move(c));
        } 

        const std::string& get_utf8_str(ConstIdxT index) const {
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

        const std::string& get_class_name(ConstIdxT index) const {
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

        ConstIdxT get_string_idx(ConstIdxT index) const {
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


        std::pair<std::string, std::string> get_name_and_type(ConstIdxT index) const {
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
      
    