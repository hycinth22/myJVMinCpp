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
#include "runtime.h"

class Interpreter {
    // 对象池，模拟JVM堆
    std::vector<JVMObject> object_pool;
public:
    Interpreter() {
        new_object();
        new_object();
    }
    // 执行指定方法
    std::optional<int32_t> execute(ClassFile& cf, const MethodInfo& method, const std::vector<int32_t>& args);
    // 根据方法名和描述符查找方法
    MethodInfo* find_method(ClassFile& cf, const std::string& name, const std::string& descriptor);
    // 分配新对象，返回对象引用（索引）
    int new_object();
    // 设置对象字段
    void put_field(int obj_ref, const std::string& field, int32_t value);
    // 获取对象字段
    int32_t get_field(int obj_ref, const std::string& field);
private:
    void resolve_getstatic(ClassFile& cf, uint16_t index, Frame& frame);
    void resolve_ldc(ClassFile& cf, uint8_t index, Frame& frame);
    void resolve_invokevirtual(ClassFile& cf, uint16_t index, Frame& frame);
    // 处理栈操作、算术、控制流等指令
    void execute_instruction(const std::vector<ConstantPoolInfo>& constant_pool, const std::vector<uint8_t>& code, size_t& pc, std::vector<int32_t>& stack, std::vector<int32_t>& locals);
};

#endif // INTERPRETER_H