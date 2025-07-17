#ifndef CLASSFILEPARSER_H
#define CLASSFILEPARSER_H
#include "classFileParser_types.h"

// Class文件解析器
class ClassFileParser {
public:
    ClassFile parse(const std::string& filename);
private:
    AttributeInfo parseAttributeInfo(std::ifstream& in, const ConstantPool& cp);
};

#endif //CLASSFILEPARSER_H
