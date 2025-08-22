#ifndef RUNTIME_H
#define RUNTIME_H
#include "constantPool.h"
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <stack>
#include <map>
#include <unordered_map>
using ByteT = int8_t;
using ShortT = int16_t;
using IntT = int32_t;
using UIntT = uint32_t;
using LongT = int64_t;
using ULongT = uint64_t;
using CharT = uint16_t;
using FloatT = float;
using DoubleT = double;
using RefT = uint32_t;
using SlotT = uint32_t;
using TwoSlotT = uint64_t;
using LocalIdxT = size_t;
using OpCodeT = uint8_t;
const size_t SLOT_WIDTH = 32;

const uint16_t ACC_NATIVE = 0x0100;
const uint16_t ACC_ABSTRACT = 0x0400;

struct MethodInfo {
    uint16_t access_flags;
    std::string name;
    std::string descriptor;
    std::vector<uint8_t> code;
    uint16_t max_stack;
    uint16_t max_locals;
};

struct ClassInfo {
    ConstantPool constant_pool;
    std::vector<MethodInfo> methods;
    uint16_t majorVer, minorVer;
    ConstIdxT this_class, super_class;
};

class LocalVars {
public:
    std::vector<SlotT> vars;
    LocalVars(LocalIdxT size) : vars(size, 0) {}
    SlotT& operator[](LocalIdxT i) { return vars[i]; }
    LongT get_long(LocalIdxT i) const {
        return get2(i);
    }
    void set_long(LocalIdxT i, LongT v) {
        set2(i, v);
    }
    DoubleT get_double(LocalIdxT i) const {
        union { DoubleT d; TwoSlotT l; } u;
        SlotT low = vars[i+1];
        SlotT high = vars[i];
        u.l = ((TwoSlotT)high << SLOT_WIDTH) | low;
        return u.d;
    }
    void set_double(LocalIdxT i, DoubleT d) {
        union { DoubleT d; TwoSlotT l; } u;
        u.d = d;
        vars[i] = (SlotT)(u.l >> SLOT_WIDTH);
        vars[i+1] = (SlotT)(u.l & 0xFFFFFFFF);
    }
    void set_uint(LocalIdxT i, UIntT v) {
        vars[i] = v;
    }
    UIntT get_uint(LocalIdxT i) const {
        return vars[i];
    }
    // int
    void set_int(LocalIdxT i, IntT v) { vars[i] = v; }
    IntT get_int(LocalIdxT i) const { return vars[i]; }
    // short
    void set_short(LocalIdxT i, ShortT v) { vars[i] = static_cast<SlotT>(v); }
    ShortT get_short(LocalIdxT i) const { return static_cast<ShortT>(vars[i]); }
    // float
    void set_float(LocalIdxT i, FloatT v) {
        union { FloatT f; UIntT u; } u;
        u.f = v;
        vars[i] = u.u;
    }
    FloatT get_float(LocalIdxT i) const {
        union { FloatT f; UIntT u; } u;
        u.u = vars[i];
        return u.f;
    }
    // ref
    void set_ref(LocalIdxT i, RefT v) { vars[i] = v; }
    RefT get_ref(LocalIdxT i) const { return vars[i]; }
    // char
    void set_char(LocalIdxT i, CharT v) { vars[i] = v; }
    CharT get_char(LocalIdxT i) const { return static_cast<CharT>(vars[i]); }
private:
    LongT get2(LocalIdxT i) const {
        SlotT low = vars[i+1];
        SlotT high = vars[i];
        return ((TwoSlotT)high << SLOT_WIDTH) | low;
    }
    void set2(LocalIdxT i, TwoSlotT v) {
        vars[i] = (SlotT)(v >> SLOT_WIDTH);
        vars[i+1] = (SlotT)(v & 0xFFFFFFFF);
    }
};

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
        push2(static_cast<LongT>(u.l));
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

struct Frame {
    LocalVars local_vars;
    OperandStack operand_stack;
    size_t pc; // 程序计数器
    const ClassInfo& class_info;
    const MethodInfo& method_info;
    Frame(size_t max_locals, size_t max_stack, const ClassInfo &class_info, const MethodInfo& method_info) : local_vars(max_locals), operand_stack(), pc(0), method_info(method_info), class_info(class_info)  {}
};

struct JVMObject {
    std::string class_name;
    std::unordered_map<std::string, RefT> fields; // 字段名到值的映射
};

};

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