#ifndef INTERPRETER_H
#define INTERPRETER_H
#include "classFileParser_types.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <stack>
#include <map>
#include <string>
#include <optional>

class Interpreter {
    // 对象池，模拟JVM堆
    std::vector< std::vector<uint8_t> > object_pool;
public:
    // 执行指定方法（支持多方法查找与调用）
    std::optional<int32_t> execute(ClassFile& cf, const MethodInfo& method);
    // 根据方法名和描述符查找方法
    MethodInfo* find_method(ClassFile& cf, const std::string& name, const std::string& descriptor);

private:
    // 处理ldc指令（常量池加载）
    void resolve_ldc(const std::vector<ConstantPoolInfo>& constant_pool, uint8_t index, std::vector<int32_t>& stack);
    // 处理invokevirtual指令（方法调用）
    void resolve_invokevirtual(const std::vector<ConstantPoolInfo>& constant_pool, uint16_t index, std::vector<int32_t>& stack);
    // 处理栈操作、算术、控制流等指令
    void execute_instruction(const std::vector<ConstantPoolInfo>& constant_pool, const std::vector<uint8_t>& code, size_t& pc, std::vector<int32_t>& stack, std::vector<int32_t>& locals);
};

#endif // INTERPRETER_H