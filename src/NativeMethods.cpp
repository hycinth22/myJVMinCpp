#include "NativeMethods.h"
#include <map>
#include <tuple>
#include <fmt/core.h>

static std::map<std::tuple<std::string, std::string, std::string>, NativeMethodFunc> native_table;

void register_native(const std::string& class_name, const std::string& method_name, const std::string& descriptor, NativeMethodFunc func) {
    native_table[{class_name, method_name, descriptor}] = func;
}

NativeMethodFunc find_native(const std::string& class_name, const std::string& method_name, const std::string& descriptor) {
    auto it = native_table.find({class_name, method_name, descriptor});
    if (it != native_table.end()) return it->second;
    return nullptr;
}

// hashCode: 返回对象引用本身作为hash
std::optional<SlotT> Object_hashCode(Frame& frame, Interpreter&) {
    RefT objref = frame.local_vars.get_ref(0);
    frame.operand_stack.push(objref);
    fmt::print("Object_hashCode {}\n", objref);
    return {};
}

// getClass: 返回Class对象引用
std::optional<SlotT> Object_getClass(Frame& frame, Interpreter& interp) {
    // 获取 this 指针
    RefT objref = frame.local_vars.get_ref(0);
    fmt::print("Object_getClass {}\n", objref);
    // // 获取对象的类名
    // const JVMObject& obj = interp.get_object(objref);
    // std::string class_name = obj.class_name;

    // 创建一个 java/lang/Class 对象
    int class_obj_ref = interp.new_object("java/lang/Class");
    JVMObject& class_obj = interp.get_object(class_obj_ref);
    class_obj.class_name = "java/lang/Class";
    // 存储原始类名，便于反射
    // class_obj.fields["name"] = class_name;

    // 压入操作数栈
    return class_obj_ref;
}

// clone: 返回自身克隆
std::optional<SlotT> Object_clone(Frame& frame, Interpreter& interp) {
    RefT objref = frame.local_vars.get_ref(0);
    RefT new_obj_ref = interp.shallow_clone_object(objref); 
    frame.operand_stack.push(objref);
    fmt::print("Object_clone {} {}\n", objref, new_obj_ref);
    return {};
}

std::optional<SlotT> Object_registerNatives(Frame& frame, Interpreter& interp) {
    RefT objref = frame.local_vars.get_ref(0);
    fmt::print("Object_registerNatives {}\n", objref);
    // todo
    return {};
}

// // notify/notifyAll/wait: 先做空实现
// std::optional<SlotT> Object_notify(Frame&, Interpreter& interp) { return {}; }
// std::optional<SlotT> Object_notifyAll(Frame&, Interpreter& interp) { return {}; }
// std::optional<SlotT> Object_wait(Frame&, Interpreter& interp) { return {}; }

// 注册所有内置native方法
void register_builtin_natives() {
    register_native("java/lang/Object", "hashCode", "()I", Object_hashCode);
    register_native("java/lang/Object", "getClass", "()Ljava/lang/Class;", Object_getClass);
    register_native("java/lang/Object", "clone", "()Ljava/lang/Object;", Object_clone);
    // register_native("java/lang/Object", "notify", "()V", Object_notify);
    // register_native("java/lang/Object", "notifyAll", "()V", Object_notifyAll);
    // register_native("java/lang/Object", "wait", "()V", Object_wait);
    // register_native("java/lang/Object", "wait", "(J)V", Object_wait);
    // register_native("java/lang/Object", "wait", "(JI)V", Object_wait);
    register_native("java/lang/Object", "registerNatives", "()V", Object_registerNatives);
    register_native("java/lang/System", "registerNatives", "()V", Object_registerNatives);
}