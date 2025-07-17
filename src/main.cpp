#include <iostream>
#include <string>
#include <fmt/core.h> 
#include "classFileParser.h"
#include "interpreter.h"
#include <filesystem>

int main(int argc, char* argv[]) {
    std::string input_file = "Test.class";
    if (argc==2) {
        input_file = std::string(argv[1]);
    }
    if (input_file.size() <= 6 || input_file.substr(input_file.size() - 6) != ".class") {
        fmt::print("filename must ends with .class");
        return 1;
    }
    std::filesystem::path input_path(input_file);
    std::string class_name = input_path.stem().string();
    // 检查 class_name 不包含特殊符号
    if (class_name.find_first_of("/\\. ") != std::string::npos) {
        fmt::print("invalid class name: {}\n", class_name);
        return 1;
    }
    fmt::print("class_name: {}\n", class_name);

    try {
        // 获取 input_file 所在目录
        std::filesystem::path input_path(input_file);
        std::string input_dir = input_path.has_parent_path() ? input_path.parent_path().string() : "";
        Interpreter interpreter;
        // 将 input_file 所在目录加入 classLoader 的 search_dirs
        if (!input_dir.empty()) {
            interpreter.class_loader.add_search_dir(input_dir);
        }
        fmt::print("Interpreter running\n");
        std::vector<SlotT> args(1);
        interpreter.execute(class_name, "main", "([Ljava/lang/String;)V", args);
        fmt::print("Main done\n");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}