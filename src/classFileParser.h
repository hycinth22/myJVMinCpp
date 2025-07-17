#ifndef CLASSFILEPARSER_H
#define CLASSFILEPARSER_H
#include "classFileParser_types.h"
#include <optional>

// Class文件解析器
class ClassFileParser {
public:
    std::optional<ClassInfo> parse(const std::string& filename);
private:
    AttributeInfo parseAttributeInfo(std::ifstream& in, const ConstantPool& cp);
};

#endif //CLASSFILEPARSER_H
