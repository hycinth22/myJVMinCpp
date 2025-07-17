#include "ClassLoader.h"
#include <stdexcept>
#include <fstream>

ClassLoader::ClassLoader(const std::vector<std::string>& dirs)
    : search_dirs(dirs) {}

void ClassLoader::set_search_dirs(const std::vector<std::string>& dirs) {
    search_dirs = dirs;
}

void ClassLoader::add_search_dir(const std::string& dir) {
    search_dirs.push_back(dir);
}

std::string ClassLoader::find_class_file(const std::string& class_name) {
    std::string filename = class_name + ".class";
    for (const auto& dir : search_dirs) {
        std::string fullpath = dir.empty() ? filename : (dir + "/" + filename);
        std::ifstream in(fullpath, std::ios::binary);
        if (in.is_open()) {
            return fullpath;
        }
    }
    // 兼容：如果没设置目录，尝试当前目录
    std::ifstream in(filename, std::ios::binary);
    if (in.is_open()) return filename;
    throw std::runtime_error("Class file not found in search dirs: " + filename);
}

ClassInfo& ClassLoader::load_class(const std::string& class_name) {
    auto it = class_table.find(class_name);
    if (it != class_table.end()) return it->second;

    // 先查找目录，确认文件存在
    std::string filename = find_class_file(class_name);

    fmt::print("ClassLoader running on {}\n", filename);
    std::optional<ClassInfo> cf_opt = parser.parse(filename);
    if (!cf_opt) {
        throw std::runtime_error("Invalid class file");
    }
    ClassInfo &cf = cf_opt.value();
    fmt::print("Version: {}.{}\n", cf.majorVer, cf.minorVer);
    fmt::print("Method List:\n");
    for (const auto& method : cf.methods) {
        fmt::print("  Name: {}, Descriptor: {}, codesize:{}, max_stack:{}, max_locals:{}\n", method.name, method.descriptor, method.code.size(), method.max_stack, method.max_locals);
    }
    fmt::print("{} loaded successfully\n", filename);

    // 递归加载父类（除Object外）
    if (cf.super_class != 0) {
        std::string super_name = cf.constant_pool.get_class_name(cf.super_class);
        if (super_name != "java/lang/Object") {
            load_class(super_name);
        }
    }

    auto [inserted, _] = class_table.emplace(class_name, std::move(cf));
    return inserted->second;
} 