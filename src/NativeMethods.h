#ifndef NATIVEMETHODS_H
#define NATIVEMETHODS_H
#include <string>
#include <functional>
#include <map>
#include <vector>
#include "runtime.h"
#include "interpreter.h"

using NativeMethodFunc = std::function<void(Frame&, Interpreter&)>;
void register_builtin_natives();
void register_native(const std::string& class_name, const std::string& method_name, const std::string& descriptor, NativeMethodFunc func);
NativeMethodFunc find_native(const std::string& class_name, const std::string& method_name, const std::string& descriptor);

#endif