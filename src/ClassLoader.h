#ifndef CLASSLOADER_H
#define CLASSLOADER_H
#include <map>
#include <string>
#include "classFileParser.h"
#include "classFileParser_types.h"

class ClassLoader {
public:
    ClassLoader(const std::vector<std::string>& dirs = {});
    // 设置查找目录
    void set_search_dirs(const std::vector<std::string>& dirs);
    // 添加单个目录
    void add_search_dir(const std::string& dir);
    void print_search_dirs();
    // 加载并返回指定类，已加载则直接返回
    ClassInfo& load_class(const std::string& class_name);
private:
    std::map<std::string, ClassInfo> class_table;
    ClassFileParser parser;

    std::vector<std::string> search_dirs;
    std::string find_class_file(const std::string& class_name);
};

#endif // CLASSLOADER_H 