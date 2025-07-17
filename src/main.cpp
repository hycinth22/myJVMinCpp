#include <iostream>
#include <string>
#include <fmt/core.h> 
#include "classFileParser.h"
#include "interpreter.h"

int main(int argc, char* argv[]) {
    std::string input_file = "Test.class";
    if (argc==2) {
        input_file = std::string(argv[1]);
    }
    try {
        fmt::print("ClassFileParser running on {}\n", input_file);
        ClassFileParser parser;
        ClassFile cf = parser.parse(input_file);
        fmt::print("Version: {}.{}\n", cf.majorVer, cf.minorVer);
        fmt::print("Method List:\n");
        for (const auto& method : cf.methods) {
            fmt::print("  Name: {}, Descriptor: {}, codesize:{}, max_stack:{}, max_locals:{}\n", method.name, method.descriptor, method.code.size(), method.max_stack, method.max_locals);

        }
        fmt::print("Interpreter running\n");
        for (const auto& method : cf.methods) {
            if (method.name == "main") {
                Interpreter interpreter;
                interpreter.execute(cf, method);
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}