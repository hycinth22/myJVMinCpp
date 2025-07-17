#ifndef RUNTIME_H
#define RUNTIME_H
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <stack>
#include <map>
using ByteT = int8_t;
using ShortT = int16_t;
using IntT = int32_t;
using UIntT = uint32_t;
using LongT = int64_t;
using ULongT = uint64_t;
using CharT = uint16_t;
using FloatT = float;
using DoubleT = double;
using RefT = int32_t;
using SlotT = uint32_t;
using TwoSlotT = uint64_t;
const size_t SLOT_WIDTH = 32;

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
    std::vector<SlotT> vars;
    LocalVars(size_t size) : vars(size, 0) {}
    SlotT& operator[](size_t i) { return vars[i]; }
    LongT get_long(size_t i) const {
        return get2(i);
    }
    void set_long(size_t i, LongT v) {
        set2(i, v);
    }
    DoubleT get_double(size_t i) const {
        union { DoubleT d; TwoSlotT l; } u;
        SlotT low = vars[i+1];
        SlotT high = vars[i];
        u.l = ((TwoSlotT)high << SLOT_WIDTH) | low;
        return u.d;
    }
    void set_double(size_t i, DoubleT d) {
        union { DoubleT d; TwoSlotT l; } u;
        u.d = d;
        vars[i] = (SlotT)(u.l >> SLOT_WIDTH);
        vars[i+1] = (SlotT)(u.l & 0xFFFFFFFF);
    }
    void set_uint(size_t i, UIntT v) {
        vars[i] = v;
    }
    UIntT get_uint(size_t i) const {
        return vars[i];
    }
    // int
    void set_int(size_t i, IntT v) { vars[i] = v; }
    IntT get_int(size_t i) const { return vars[i]; }
    // short
    void set_short(size_t i, ShortT v) { vars[i] = static_cast<SlotT>(v); }
    ShortT get_short(size_t i) const { return static_cast<ShortT>(vars[i]); }
    // float
    void set_float(size_t i, FloatT v) {
        union { FloatT f; UIntT u; } u;
        u.f = v;
        vars[i] = u.u;
    }
    FloatT get_float(size_t i) const {
        union { FloatT f; UIntT u; } u;
        u.u = vars[i];
        return u.f;
    }
    // ref
    void set_ref(size_t i, RefT v) { vars[i] = v; }
    RefT get_ref(size_t i) const { return vars[i]; }
    // char
    void set_char(size_t i, CharT v) { vars[i] = v; }
    CharT get_char(size_t i) const { return static_cast<CharT>(vars[i]); }
private:
    int64_t get2(size_t i) const {
        SlotT low = vars[i+1];
        SlotT high = vars[i];
        return ((TwoSlotT)high << SLOT_WIDTH) | low;
    }
    void set2(size_t i, TwoSlotT v) {
        vars[i] = (SlotT)(v >> SLOT_WIDTH);
        vars[i+1] = (SlotT)(v & 0xFFFFFFFF);
    }
};

// 操作数栈
class OperandStack {
public:
    std::vector<SlotT> stack;
    void push(SlotT val) { stack.push_back(val); }
    SlotT pop() { SlotT v = stack.back(); stack.pop_back(); return v; }
    void push_long(LongT d) {
        push2(d);
    }
    LongT pop_long() {
        return pop2();
    }
    void push_double(DoubleT d) {
        union { DoubleT d; TwoSlotT l; } u;
        u.d = d;
        push2(static_cast<int64_t>(u.l));
    }
    DoubleT pop_double() {
        union { DoubleT d; TwoSlotT l; } u;
        u.l = static_cast<TwoSlotT>(pop2());
        return u.d;
    }
    void push_uint(UIntT v) { stack.push_back(v); }
    UIntT pop_uint() { UIntT v = stack.back(); stack.pop_back(); return v; }
    void push_int(IntT v) { stack.push_back(v); }
    IntT pop_int() { IntT v = stack.back(); stack.pop_back(); return v; }
    void push_short(ShortT v) { stack.push_back(static_cast<SlotT>(v)); }
    ShortT pop_short() { ShortT v = static_cast<ShortT>(stack.back()); stack.pop_back(); return v; }
    void push_float(FloatT v) { union { FloatT f; UIntT u; } u; u.f = v; stack.push_back(u.u); }
    FloatT pop_float() { union { FloatT f; UIntT u; } u; u.u = stack.back(); stack.pop_back(); return u.f; }
    void push_ref(RefT v) { stack.push_back(v); }
    RefT pop_ref() { RefT v = stack.back(); stack.pop_back(); return v; }
    void push_char(CharT v) { stack.push_back(static_cast<SlotT>(v)); }
    CharT pop_char() { CharT v = static_cast<CharT>(stack.back()); stack.pop_back(); return v; }
    size_t size() const { return stack.size(); }
private:
    void push2(TwoSlotT v) {
        stack.push_back((SlotT)(v >> SLOT_WIDTH));
        stack.push_back((SlotT)(v & 0xFFFFFFFF));
    }
    TwoSlotT pop2() {
        SlotT low = stack.back(); stack.pop_back();
        SlotT high = stack.back(); stack.pop_back();
        return ((TwoSlotT)high << SLOT_WIDTH) | (SlotT)low;
    }
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