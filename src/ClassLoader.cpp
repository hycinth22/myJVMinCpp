#include "ClassLoader.h"
#include <stdexcept>
#include <fstream>
#include <filesystem>

ClassLoader::ClassLoader(const std::vector<std::string>& dirs)
    : search_dirs(dirs) {
    }

void ClassLoader::set_search_dirs(const std::vector<std::string>& dirs) {
    search_dirs = dirs;
}

void ClassLoader::add_search_dir(const std::string& dir) {
    search_dirs.push_back(dir);
}

void ClassLoader::print_search_dirs() {
    fmt::print("ClassLoader search_dirs: ");
    for (auto& dir: search_dirs) {
        fmt::print("{};", dir);
    }
    fmt::print("\n");
}

// class_name 形如 java/io/PrintStream
std::string ClassLoader::find_class_file(const std::string& class_name) {
    std::string relpath = class_name + ".class";
    for (const auto& dir : search_dirs) {
        std::string fullpath = dir.empty() ? relpath : (dir + "/" + relpath);
        std::ifstream in(fullpath, std::ios::binary);
        if (in.is_open()) return fullpath;
    }
    // 兼容：如果没设置目录，尝试当前目录
    std::ifstream in(relpath, std::ios::binary);
    if (in.is_open()) return relpath;
    throw std::runtime_error("Class file not found in search dirs: " + relpath);
}

ClassInfo& ClassLoader::load_class(const std::string& class_name, LoadClassCallback loaded_callback) {
    auto it = class_table.find(class_name);
    if (it != class_table.end()) return it->second;

    fmt::print("ClassLoader searching for {}\n", class_name);
    std::string filename = find_class_file(class_name);
    fmt::print("ClassFileParser running on {}\n", filename);
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
            load_class(super_name, loaded_callback);
        }
    }

    auto [inserted, _] = class_table.emplace(class_name, std::move(cf));
    loaded_callback(class_name);
    return inserted->second;
} 