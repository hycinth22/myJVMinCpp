#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <fmt/core.h>
#include "interpreter.h"
#include "runtime.h"

JVMThread thread;

uint16_t read_u2_from_code(const std::vector<uint8_t>& code, size_t pc) {
    return (code[pc] << 8) | code[pc + 1];
}

// 根据方法名和描述符查找方法
MethodInfo* Interpreter::find_method(ClassFile& cf, const std::string& name, const std::string& descriptor) {
    for (auto& m : cf.methods) {
        if (m.name == name && m.descriptor == descriptor) {
            return &m;
        }
    }
    return nullptr;
}

// 解析方法描述符，返回参数个数
int count_method_args(const std::string& desc) {
    int count = 0;
    bool inType = false;
    for (size_t i = 1; i < desc.size() && desc[i] != ')'; ++i) {
        if (desc[i] == 'L') { // 对象类型
            while (desc[i] != ';') ++i;
            ++count;
        } else if (desc[i] == '[') {
            // 跳过数组标记
        } else {
            ++count; // 基本类型
        }
    }
    return count;
}

// 执行指定的方法
std::optional<int32_t> Interpreter::execute(ClassFile& cf, const MethodInfo& method, const std::vector<int32_t>& args) {
    auto installFrame = [](const std::string& _classname, const MethodInfo& _method, const std::vector<int32_t>& _args) {
        Frame frame(_method.max_locals, _method.max_stack, _classname, _method);
        for (int i=0; i<_args.size(); i++) {
            frame.local_vars[i] = _args[i];
        }
        thread.push_frame(frame);
    };
    installFrame(cf.constant_pool.get_class_name(cf.this_class), method, args);
    while (!thread.empty()) {
        Frame& cur_frame = thread.current_frame();
        auto &pc = cur_frame.pc;
        auto &code = cur_frame.method.code;
        if (pc >= code.size()) {
            fmt::print("pc reach code end but no return");
            exit(1);
        }
        uint8_t opcode = code[pc++];
        fmt::print("[execute] className:{} method: {}", cur_frame.classname, cur_frame.method.name);
        fmt::print(" pc 0x{:x} op 0x{:x} \n", pc, opcode);
        switch (opcode) {
            case 0x00: // nop
                break;
                case 0x3: // iconst_0
                case 0x4: // iconst_1
                case 0x5: // iconst_2
                case 0x6: // iconst_3
                case 0x7: // iconst_4
                case 0x8: // iconst_5
                {
                    int constval = opcode - 0x3;
                    cur_frame.operand_stack.push(constval);
                    fmt::print("Put constant {} on the stack\n", constval);
                    break;
                }
                case 0x10: // bipush
                {
                    int8_t val = (int8_t)code[pc++];
                    cur_frame.operand_stack.push(val);
                    fmt::print("bipush: Put imm {} on the stack\n", val);
                    break;
                }
                case 0x12: // ldc
                {
                    uint8_t idx = code[pc++];
                    const ConstantPoolInfo& cpe = cf.constant_pool[idx];
                    if (cpe.tag == ConstantType::INTEGER || cpe.tag == ConstantType::FLOAT) {
                        cur_frame.operand_stack.push(cpe.integerOrFloat);
                    } else {
                        fmt::print("[ldc] 暂不支持的常量类型 tag={}\n", (int)cpe.tag);
                        exit(1);
                    }
                    break;
                }
                case 0x1a: // iload_0
                case 0x1b: // iload_1
                case 0x1c: // iload_2
                case 0x1d: // iload_3
                { // Load int from local variable
                    int local_index = opcode - 0x1a;
                    cur_frame.operand_stack.push(cur_frame.local_vars[local_index]);
                    fmt::print("Load int from local variable. index {} val {}\n", local_index, cur_frame.local_vars[local_index]);
                    break;
                }
                case 0x2a: // aload_0
                case 0x2b: // aload_1
                case 0x2c: // aload_2
                case 0x2d: // aload_3
                { // Load reference from local variable
                    int local_index = opcode - 0x2a;
                    cur_frame.operand_stack.push(cur_frame.local_vars[local_index]);
                    fmt::print("aload: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
                    break;
                }
                case 0x36: // istore
                { // Store int into local variable
                    uint8_t idx = code[pc++];
                    cur_frame.local_vars[idx] = cur_frame.operand_stack.pop();
                    break;
                }
                case 0x3b: // istore_0
                case 0x3c: // istore_1
                case 0x3d: // istore_2
                case 0x3e: // istore_3
                { // Store int into local variable
                    int local_index = opcode - 0x3b;
                    cur_frame.local_vars[local_index] = cur_frame.operand_stack.pop();
                    fmt::print("istore: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
                    break;
                }
                case 0x4b: // astore_0  
                case 0x4c: // astore_1
                case 0x4d: // astore_2
                case 0x4e: // astore_3
                { // Store reference into local variable
                    int local_index = opcode - 0x4b;
                    cur_frame.local_vars[local_index] = cur_frame.operand_stack.pop();
                    fmt::print("astore: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
                    break;
                }
                case 0x59: // dup
                { // Duplicate the top operand stack value
                    int32_t val = cur_frame.operand_stack.stack.back();
                    cur_frame.operand_stack.push(val);
                    fmt::print("dup: dupicate the val {} on the stack\n", val);
                    break;
                }
                case 0x60: // iadd
                {
                    int32_t v2 = cur_frame.operand_stack.pop();
                    int32_t v1 = cur_frame.operand_stack.pop();
                    fmt::print("iadd {} + {}\n", v1, v2);
                    cur_frame.operand_stack.push(v1 + v2);
                    break;
                }
                case 0x84: // iinc
                {
                    uint8_t idx = code[pc++];
                    int8_t inc = (int8_t)code[pc++];
                    cur_frame.local_vars[idx] += inc;
                    break;
                }
                case 0x9f: // if_icmpeq
                {
                    int32_t v2 = cur_frame.operand_stack.pop();
                    int32_t v1 = cur_frame.operand_stack.pop();
                    int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
                    pc += 2;
                    if (v1 == v2) {
                        pc = (size_t)((int)pc + offset - 3);
                    }
                    break;
                }
                case 0xa0: // if_icmpne
                {
                    int32_t v2 = cur_frame.operand_stack.pop();
                    int32_t v1 = cur_frame.operand_stack.pop();
                    int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
                    pc += 2;
                    if (v1 != v2) {
                        pc = (size_t)((int)pc + offset - 3);
                    }
                    break;
                }
                case 0xa1: // if_icmplt
                {
                    int32_t v2 = cur_frame.operand_stack.pop();
                    int32_t v1 = cur_frame.operand_stack.pop();
                    int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
                    pc += 2;
                    if (v1 < v2) {
                        pc = (size_t)((int)pc + offset - 3);
                    }
                    break;
                }
                case 0xa2: // if_icmpge
                {
                    int32_t v2 = cur_frame.operand_stack.pop();
                    int32_t v1 = cur_frame.operand_stack.pop();
                    int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
                    pc += 2;
                    if (v1 >= v2) {
                        pc = (size_t)((int)pc + offset - 3);
                    }
                    break;
                }
                case 0xa3: // if_icmpgt
                {
                    int32_t v2 = cur_frame.operand_stack.pop();
                    int32_t v1 = cur_frame.operand_stack.pop();
                    int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
                    pc += 2;
                    if (v1 > v2) {
                        pc = (size_t)((int)pc + offset - 3);
                    }
                    break;
                }
                case 0xa4: // if_icmple
                {
                    int32_t v2 = cur_frame.operand_stack.pop();
                    int32_t v1 = cur_frame.operand_stack.pop();
                    int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
                    pc += 2;
                    if (v1 <= v2) {
                        pc = (size_t)((int)pc + offset - 3);
                    }
                    break;
                }
                case 0xa7: // goto
                {
                    int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
                    pc += 2;
                    pc = (size_t)((int)pc + offset - 3); // -3: opcode+2字节已读
                    break;
                }
                case 0xac: // ireturn
                { // int方法返回
                    int32_t ret = cur_frame.operand_stack.pop();
                    thread.pop_frame();
                    if (thread.empty()) {
                        return ret; // program exit
                    }
                    thread.current_frame().operand_stack.push(ret);
                    break;
                }
                case 0xb1: // return
                { // void方法返回
                    thread.pop_frame();
                    if (thread.empty()) {
                        return {}; // program exit
                    }
                    break;
                }
                case 0xb2: // getstatic
                {
                    uint16_t idx = (code[pc] << 8) | code[pc+1];
                    pc += 2;
                    resolve_getstatic(cf, idx, cur_frame);
                    break;
                }
                case 0xb4: // getfield
                {
                    int16_t idx = (code[pc] << 8) | code[pc+1];
                    pc += 2;
                    // 解析字段名和类型
                    const ConstantPoolInfo& fieldref = cf.constant_pool[idx];
                    uint16_t name_type_idx = fieldref.fieldref_name_type_index;
                    auto [field_name, field_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
                    int obj_ref = cur_frame.operand_stack.pop();
                    int32_t val = get_field(obj_ref, field_name);
                    cur_frame.operand_stack.push(val);
                    fmt::print("getfield: get obj:{} field:{} val:{}\n", obj_ref, field_name, val);
                    break;
                }
                case 0xb5: // putfield
                {
                    int16_t idx = (code[pc] << 8) | code[pc+1];
                    pc += 2;
                    // 解析字段名和类型
                    const ConstantPoolInfo& fieldref = cf.constant_pool[idx];
                    uint16_t name_type_idx = fieldref.fieldref_name_type_index;
                    auto [field_name, field_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
                    int32_t val = cur_frame.operand_stack.pop();
                    int obj_ref = cur_frame.operand_stack.pop();
                    put_field(obj_ref, field_name, val);
                    fmt::print("putfield: set obj:{} field:{} val:{}\n", obj_ref, field_name, val);
                    break;
                }
                case 0xb6: // invokevirtual
                {
                    uint16_t idx = (code[pc] << 8) | code[pc+1];
                    pc += 2;
                    // 解析常量池，获取方法所属类名、方法名和描述符
                    const ConstantPoolInfo& methodref = cf.constant_pool[idx];
                    uint16_t class_idx = methodref.methodref_class_index;
                    uint16_t name_type_idx = methodref.methodref_name_type_index;
                    std::string class_name = cf.constant_pool.get_utf8_str(cf.constant_pool[class_idx].class_name_index);
                    auto [method_name, method_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
                    // 特殊处理System.out.println
                    if (class_name == "java/io/PrintStream" && method_name == "println") {
                        int32_t val = cur_frame.operand_stack.pop(); // 要打印的值
                        int obj_ref = cur_frame.operand_stack.pop(); // System.out对象引用

                        fmt::print("[System.out.println] {}\n", val);
                        break;
                    }
                    fmt::print("[invokevirtual] classname:{} method_name:{} method_desc:{}\n", class_name.c_str(), method_name.c_str(), method_desc.c_str());
                    // 弹出参数
                    int arg_count = count_method_args(method_desc);
                    arg_count++; // obj ref
                    std::vector<int32_t> args(arg_count);
                    for (int i = arg_count - 1; i >= 0; --i) {
                        args[i] = cur_frame.operand_stack.pop();
                        fmt::print("setup args[{}]={}\n", i, args[i]);
                    }
                    fmt::print("objref {} \n", args[0]);
                    // 支持本类方法调用
                    MethodInfo* target = find_method(cf, method_name, method_desc);
                    if (target) {
                        // 设置新帧并压入被调用栈
                        installFrame(class_name, *target, args);
                        break;
                    } else {
                        fmt::print("invokevirtual invaid method");
                        exit(1);
                    }
                    break;
                }
                case 0xb7: // invokespecial
                {
                    uint16_t idx = (code[pc] << 8) | code[pc+1];
                    pc += 2;
                    // 解析常量池，查找方法名和描述符
                    const ConstantPoolInfo& cp_entry = cf.constant_pool[idx];
                    uint16_t methodref_class_index = cp_entry.methodref_class_index;
                    uint16_t name_type_idx = cp_entry.methodref_name_type_index;
                    auto class_name = cf.constant_pool.get_class_name(methodref_class_index);
                    auto [method_name, method_desc] = cf.constant_pool.get_name_and_type(name_type_idx);

                    fmt::print("name_type_idx: {}", name_type_idx);
                    fmt::print("[invokespecial] idx{} classname:{} method_name:{} method_desc:{}\n", idx, class_name, method_name, method_desc);

                    if (class_name=="java/lang/Object" && method_name=="<init>") {
                        // todo: remove here if implement multilclass
                        break;
                    }

                     // 弹出参数
                    int arg_count = count_method_args(method_desc);
                    arg_count++; // object ref
                    std::vector<int32_t> args(arg_count);
                    for (int i = arg_count - 1; i >= 0; --i) {
                        args[i] = cur_frame.operand_stack.pop();
                        fmt::print("setup args[{}]={}\n", i, args[i]);
                    }
                    if (args[0]==0) break;
                    fmt::print("objref {} \n", args[0]);

                    MethodInfo* target = find_method(cf, method_name, method_desc);
                    if (target) {
                        // 设置新帧并压入被调用栈
                        installFrame(class_name, *target, args);
                        break;
                    } else {
                        fmt::print("invokespecial invaid method");
                        exit(1);
                    }
                    break;
                }
                case 0xb8: // invokestatic
                {
                    uint16_t idx = (code[pc] << 8) | code[pc+1];
                    pc += 2;
                    // 解析常量池，获取方法所属类名、方法名和描述符
                    const ConstantPoolInfo& methodref = cf.constant_pool[idx];
                    uint16_t class_idx = methodref.methodref_class_index;
                    uint16_t name_type_idx = methodref.methodref_name_type_index;
                    std::string class_name = cf.constant_pool.get_utf8_str(cf.constant_pool[class_idx].class_name_index);
                    auto [method_name, method_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
                    fmt::print("[invokestatic] classname:{} method_name:{} method_desc:{}\n", class_name.c_str(), method_name.c_str(), method_desc.c_str());
                    // 弹出参数
                    int arg_count = count_method_args(method_desc);
                    std::vector<int32_t> args(arg_count);
                    for (int i = arg_count - 1; i >= 0; --i) {
                        args[i] = cur_frame.operand_stack.pop();
                        fmt::print("setup args[{}]={}\n", i, args[i]);
                    }

                    // 支持本类静态方法调用
                    MethodInfo* target = find_method(cf, method_name, method_desc);
                    if (target) {
                        // 设置新帧并压入被调用栈
                        installFrame(class_name, *target, args);
                        break;
                    } else {
                        fmt::print("invokestatic invaid method");
                        exit(1);
                    }
                    break;
                }
                case 0xbb: // new
                {
                    pc += 2; // 跳过常量池索引
                    int obj_ref = new_object();
                    cur_frame.operand_stack.push(obj_ref);
                    fmt::print("alloc new object {}\n", obj_ref);
                    break;
                }
            default:
                fmt::print("暂不支持的字节码 0x{:x}\n", (int)opcode);
                return {};
        }
        cur_frame.pc = pc;
    }
    fmt::print("pc reach code end end but no return");
    exit(1);
}


// 分配新对象，返回对象引用（索引）
int Interpreter::new_object() {
    JVMObject obj;
    object_pool.push_back(obj);
    return object_pool.size() - 1; // 返回对象在池中的索引
}
// 设置对象字段
void Interpreter::put_field(int obj_ref, const std::string& field, int32_t value) {
    if (obj_ref >= 0 && obj_ref < object_pool.size()) {
        object_pool[obj_ref].fields[field] = value;
    }
}
// 获取对象字段
int32_t Interpreter::get_field(int obj_ref, const std::string& field) {
    if (obj_ref >= 0 && obj_ref < object_pool.size()) {
        return object_pool[obj_ref].fields[field];
    }
    return 0;
}

// 处理getstatic
void Interpreter::resolve_getstatic(ClassFile& cf, uint16_t index, Frame& frame) {
    // 解析常量池
    const ConstantPoolInfo& fieldref = cf.constant_pool[index];
    uint16_t class_idx = fieldref.fieldref_class_index;
    uint16_t name_type_idx = fieldref.fieldref_name_type_index;

    std::string class_name = cf.constant_pool.get_class_name(class_idx);
    auto [field_name, field_desc] = cf.constant_pool.get_name_and_type(name_type_idx);

    // 特殊处理 System.out
    if (class_name == "java/lang/System" && field_name == "out" && field_desc == "Ljava/io/PrintStream;") {
        // 压入一个特殊的PrintStream对象引用
        frame.operand_stack.push(0xCAFEBABE);
    } else {
        fmt::print("[getstatic] 找不到静态字段:  {}.{} {}\n", class_name, field_name, field_desc);
        frame.operand_stack.push(0); // todo: 抛异常
    }
}


