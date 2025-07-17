#ifndef NATIVEMETHODS_H
#define NATIVEMETHODS_H
#include <string>
#include <functional>
#include <map>
#include <vector>
#include "runtime.h"
#include "interpreter.h"

// native方法的注册表类型
using NativeMethodFunc = std::function<void(Frame&, Interpreter&)>;

// 注册native方法
void register_native(const std::string& class_name, const std::string& method_name, const std::string& descriptor, NativeMethodFunc func);

// 查找native方法
NativeMethodFunc find_native(const std::string& class_name, const std::string& method_name, const std::string& descriptor);

// 注册所有内置native方法
void register_builtin_natives();

#endif