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
            default:
                printf("(other)\n");
        }
    }
}