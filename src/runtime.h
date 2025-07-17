#ifndef RUNTIME_H
#define RUNTIME_H
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <stack>
#include <map>

// 方法信息结构
struct MethodInfo {
    uint16_t access_flags;
    std::string name;
    std::string descriptor;
    std::vector<uint8_t> code;
    uint16_t max_stack;
    uint16_t max_locals;
};

// 方法信息结构
struct ClassInfo {
    ConstantPool constant_pool;
    std::vector<MethodInfo> methods;
    uint16_t majorVer, minorVer;
    uint16_t this_class, super_class;
};

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
    const ClassInfo& class_info;
    const MethodInfo& method_info;
    Frame(size_t max_locals, size_t max_stack, const ClassInfo &class_info, const MethodInfo& method_info) : local_vars(max_locals), operand_stack(), pc(0), method_info(method_info), class_info(class_info)  {}
};

// JVM对象模型
struct JVMObject {
    std::map<std::string, int32_t> fields; // 字段名到值的映射
};

// JVM线程
class JVMThread {
public:
    std::stack<Frame> call_stack;
    void push_frame(const Frame& frame) { 
        // printf("push frame of %s\n", frame.method.name.c_str());
        call_stack.push(frame);
    }
    void pop_frame() { call_stack.pop(); }
    Frame& current_frame() { 
        // printf("top frame is %s\n", call_stack.top().method.name.c_str());
        return call_stack.top();
     }
    bool empty() const { return call_stack.empty(); }
};

#endif // RUNTIME_H 