#ifndef RUNTIME_H
#define RUNTIME_H
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <stack>
#include <map>

// 局部变量表（本地变量区）
class LocalVars {
public:
    std::vector<int32_t> vars;
    LocalVars(size_t size) : vars(size, 0) {}
    int32_t& operator[](size_t i) { return vars[i]; }
};

// 操作数栈
class OperandStack {
public:
    std::vector<int32_t> stack;
    void push(int32_t val) { stack.push_back(val); }
    int32_t pop() { int32_t v = stack.back(); stack.pop_back(); return v; }
    size_t size() const { return stack.size(); }
};

// 栈帧（方法调用帧）
struct Frame {
    LocalVars local_vars;
    OperandStack operand_stack;
    size_t pc; // 程序计数器
    Frame(size_t max_locals, size_t max_stack) : local_vars(max_locals), operand_stack(), pc(0) {}
};

// JVM对象模型
struct JVMObject {
    std::map<std::string, int32_t> fields; // 字段名到值的映射
};

// JVM线程
class JVMThread {
public:
    std::stack<Frame> call_stack;
    void push_frame(const Frame& frame) { call_stack.push(frame); }
    void pop_frame() { call_stack.pop(); }
    Frame& current_frame() { return call_stack.top(); }
    bool empty() const { return call_stack.empty(); }
};

#endif // RUNTIME_H 