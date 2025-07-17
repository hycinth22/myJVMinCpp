#include "constantPool.h"
#include <stdio.h>

void ConstantPool::print_all() const {
    printf("constant pool:\n");
    for (size_t i = 0; i < pool.size(); ++i) {
        const auto& cp = pool[i];
        printf("#%zu: tag=%d ", i+1, cp.tag);
        switch (cp.tag) {
            case ConstantType::UTF8:
                printf("Utf8: %s\n", cp.utf8_str.c_str());
                break;
            case ConstantType::CLASS:
                printf("Class: name_index=%d\n", cp.class_name_index);
                break;
            case ConstantType::FIELD_REF:
                printf("Fieldref: class_index=%d, name_and_type_index=%d\n", cp.fieldref_class_index, cp.fieldref_name_type_index);
                break;
            case ConstantType::METHOD_REF:
                printf("Methodref: class_index=%d, name_and_type_index=%d\n", cp.methodref_class_index, cp.methodref_name_type_index);
                break;
            case ConstantType::INTERFACE_METHOD_REF:
                printf("InterfaceMethodref: class_index=%d, name_and_type_index=%d\n", cp.methodref_class_index, cp.methodref_name_type_index);
                break;
            case ConstantType::NAME_AND_TYPE:
                printf("NameAndType: name_index=%d, descriptor_index=%d\n", cp.name_index, cp.descriptor_index);
                break;
            case ConstantType::STRING:
                printf("String: string_index=%d\n", cp.string_index);
                break;
            case ConstantType::INTEGER:
                printf("Integer: %d\n", cp.integerOrFloat);
                break;
            case ConstantType::FLOAT:
                printf("Float: %u\n", cp.integerOrFloat);
                break;
            case ConstantType::LONG:
                printf("Long: high=%u, low=%u\n", cp.longOrDouble_high_bytes, cp.longOrDouble_low_bytes);
                break;
            case ConstantType::DOUBLE:
                printf("Double: high=%u, low=%u\n", cp.longOrDouble_high_bytes, cp.longOrDouble_low_bytes);
                break;
            case ConstantType::METHOD_HANDLE:
                printf("MethodHandle: reference_kind=%u, reference_index=%u\n", cp.reference_kind, cp.reference_index);
                break;
            case ConstantType::METHOD_TYPE:
                printf("MethodType: descriptor_index=%u\n", cp.descriptor_index_mt);
                break;
            case ConstantType::INVOKE_DYNAMIC:
                printf("InvokeDynamic: bootstrap_method_attr_index=%u, name_and_type_index=%u\n", cp.bootstrap_method_attr_index, cp.name_and_type_index);
                break;
            default:
                printf("(other)\n");
        }
    }
}