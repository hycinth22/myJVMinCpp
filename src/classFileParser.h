#ifndef CLASSFILEPARSER_H
#define CLASSFILEPARSER_H
#include "classFileParser_types.h"

// Class文件解析器
class ClassFileParser {
public:
    ClassFile parse(const std::string& filename);
private:
    AttributeInfo parseAttributeInfo(std::ifstream& in, const std::vector<ConstantPoolInfo>& cp);
    std::string get_utf8_str(const std::vector<ConstantPoolInfo>& cp, uint16_t index) const;
};

#endif //CLASSFILEPARSER_H
