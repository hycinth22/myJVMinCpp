#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <fmt/core.h>
#include "interpreter.h"
#include "runtime.h"
#include <iostream>

uint16_t read_u2_from_code(const std::vector<uint8_t>& code, size_t pc) {
    return (code[pc] << 8) | code[pc + 1];
}

// 解析getstatic指令
void resolve_getstatic(const std::vector<ConstantPoolInfo>& constant_pool, uint16_t index, std::vector<int32_t>& stack) {
    const ConstantPoolInfo& cp = constant_pool[index - 1];
    if (cp.tag != 9 && cp.tag != 10) {
        throw std::runtime_error("Invalid constant pool entry for getstatic");
    }

    // 获取字段所属类名
    uint16_t class_index = (cp.tag == 9) ? cp.fieldref_class_index : cp.methodref_class_index;
    const ConstantPoolInfo& class_cp = constant_pool[class_index - 1];
    std::string class_name = constant_pool[class_cp.class_name_index - 1].utf8_str;

    // 获取字段名和描述符
    uint16_t name_type_index = (cp.tag == 9) ? cp.fieldref_name_type_index : cp.methodref_name_type_index;
    const ConstantPoolInfo& name_type_cp = constant_pool[name_type_index - 1];
    std::string field_name = constant_pool[name_type_cp.name_index - 1].utf8_str;
    std::string descriptor = constant_pool[name_type_cp.descriptor_index - 1].utf8_str;

    fmt::print(u8R"([getstatic] field "{}" of class "{}")", field_name, class_name);
    fmt::print("\n");
    // 模拟System.out处理
    if (class_name == "java/lang/System" && field_name == "out" && descriptor == "Ljava/io/PrintStream;") {
        stack.push_back(0xCAFEBABE); // 压入模拟的PrintStream对象引用
    } else {
        throw std::runtime_error("Unsupported static field: " + class_name + "." + field_name);
    }
}

// 解析ldc指令
void Interpreter::resolve_ldc(const std::vector<ConstantPoolInfo>& constant_pool, uint8_t index, std::vector<int32_t>& stack) {
    const ConstantPoolInfo& cp = constant_pool[index - 1];
    fmt::print("[ldc] constant index {}, typetag {}\n", index, cp.tag);
    switch (cp.tag) {
        case 1: // CONSTANT_Utf8
            stack.push_back(0xDEADBEEF); // 压入字符串引用（模拟）
            break;
        case 3: // CONSTANT_Integer
        case 4: // CONSTANT_Float
            stack.push_back(cp.integerOrFloat);
            break;
        case 8: { // CONSTANT_String
            // 通过字符串索引获取实际字符串
            uint16_t str_idx = cp.string_index;
            const std::string& str = constant_pool[str_idx-1].utf8_str;

            std::vector<uint8_t> obj_data(str.size());
            for (int i=0; i<str.size(); i++) {
                obj_data[i] = str[i];
            }
            object_pool.push_back(move(obj_data));
            int32_t ref = object_pool.size()-1;
            stack.push_back(ref);
            break;
        }
        default:
            throw std::runtime_error(fmt::format("Unsupported ldc type {}", cp.tag));
    }
}

// 解析invokevirtual指令
void Interpreter::resolve_invokevirtual(const std::vector<ConstantPoolInfo>& constant_pool, uint16_t index, std::vector<int32_t>& stack) {
    const ConstantPoolInfo& cp = constant_pool[index - 1];
    uint16_t class_index = cp.methodref_class_index;
    uint16_t name_type_index = cp.methodref_name_type_index;

    // 获取方法所属类名
    const ConstantPoolInfo& class_cp = constant_pool[class_index - 1];
    std::string class_name = constant_pool[class_cp.class_name_index - 1].utf8_str;

    // 获取方法名和描述符
    const ConstantPoolInfo& name_type_cp = constant_pool[name_type_index - 1];
    std::string method_name = constant_pool[name_type_cp.name_index - 1].utf8_str;
    std::string descriptor = constant_pool[name_type_cp.descriptor_index - 1].utf8_str;

    // 模拟PrintStream.println
    fmt::print(u8R"([invokevirtual] method "{}" of "{}")", method_name, class_name);
    fmt::print("\n");
    if (class_name == "java/io/PrintStream" && method_name == "println") {
        int32_t arg = stack.back(); stack.pop_back();
        int32_t obj_ref = stack.back(); stack.pop_back();
        if (obj_ref != 0xCAFEBABE) {
            throw std::runtime_error("Invalid object reference");
        }
        const std::vector<uint8_t> &arg_obj = object_pool[arg];
        const std::string msg = std::string(arg_obj.begin(), arg_obj.end());
        fmt::print(u8R"([PROGRAM_println] {})", msg);
        fmt::print("\n");
    } else {
        throw std::runtime_error("Unsupported method: " + class_name + "." + method_name);
    }
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

// 执行指定的方法
std::optional<int32_t> Interpreter::execute(ClassFile& cf, const MethodInfo& method) {
    JVMThread thread;
    Frame frame(method.max_locals, method.max_stack);
    size_t& pc = frame.pc;
    std::vector<uint8_t> code = method.code;
    thread.push_frame(frame);
    while (!thread.empty()) {
        Frame& cur_frame = thread.current_frame();
        pc = cur_frame.pc;
        if (pc >= code.size()) break;
        uint8_t opcode = code[pc++];
        switch (opcode) {
            case 0x00: // nop
                break;
                case 0x03: // iconst_0
                case 0x04: // iconst_1
                case 0x05: // iconst_2
                case 0x06: // iconst_3
                case 0x07: // iconst_4
                case 0x08: // iconst_5
                {
                    int constval = opcode - 0x03;
                    cur_frame.operand_stack.push(constval);
                    break;
                }
                case 0x10: // bipush
                    cur_frame.operand_stack.push((int8_t)code[pc++]);
                    break;
                case 0x1a: // iload_0
                case 0x1b: // iload_1
                case 0x1c: // iload_2
                case 0x1d: // iload_3
                {
                    int local_index = opcode - 0x1a;
                    cur_frame.operand_stack.push(cur_frame.local_vars[local_index]);
                    break;
                }
                case 0x36: // istore
                {
                    uint8_t idx = code[pc++];
                    cur_frame.local_vars[idx] = cur_frame.operand_stack.pop();
                    break;
                }
                case 0x3b: // istore_0
                case 0x3c: // istore_1
                case 0x3d: // istore_2
                case 0x3e: // istore_3
                {
                    int local_index = opcode - 0x3b;
                    cur_frame.local_vars[local_index] = cur_frame.operand_stack.pop();
                    break;
                }
                case 0x60: // iadd
                {
                    int32_t v2 = cur_frame.operand_stack.pop();
                    int32_t v1 = cur_frame.operand_stack.pop();
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
                case 0xac: // ireturn（int方法返回）
                {
                    int32_t ret = cur_frame.operand_stack.pop();
                    std::cout << "方法返回值: " << ret << std::endl;
                    if (!thread.empty()) {
                        // 返回值压入调用者帧的操作数栈
                        thread.current_frame().operand_stack.push(ret);
                    }
                    break;
                }
                case 0xb1: // return（void方法返回）
                    thread.pop_frame();
                    break;
                case 0xb6: // invokevirtual
                {
                    uint16_t idx = (code[pc] << 8) | code[pc+1];
                    pc += 2;
                    // 解析常量池，查找方法名和描述符
                    // 这里只支持本类方法调用
                    // 真实JVM需支持多类查找
                        
                    // 解析方法名和描述符
                    const ConstantPoolInfo& cp_entry = cf.constant_pool[idx];
                    uint16_t name_type_idx = cp_entry.methodref_name_type_index;
                    const ConstantPoolInfo& nt = cf.constant_pool[name_type_idx];
                    std::string mname = nt.utf8_str; // 这里简化，实际需查name_index
                    std::string mdesc = "()V"; // 假设void方法
                    MethodInfo* target = find_method(cf, mname, mdesc);
                    if (target) {
                        execute(cf, *target);
                    }
                    break;
                }
                case 0xb8: // invokestatic
                {
                    uint16_t idx = (code[pc] << 8) | code[pc+1];
                    pc += 2;
                    // 解析常量池，查找方法名和描述符
                    const ConstantPoolInfo& cp_entry = cf.constant_pool[idx];
                    uint16_t name_type_idx = cp_entry.methodref_name_type_index;
                    const ConstantPoolInfo& nt = cf.constant_pool[name_type_idx];
                    std::string mname = nt.utf8_str; // 这里简化，实际需查name_index
                    std::string mdesc = "()V"; // 假设void方法
                    MethodInfo* target = find_method(cf, mname, mdesc);
                    if (target) {
                        auto ret = execute(cf, *target);
                        if (ret.has_value()) {
                            cur_frame.operand_stack.push(ret.value());
                        }
                    }
                    break;
                }
            default:
                std::cerr << "暂不支持的字节码: 0x" << std::hex << (int)opcode << std::endl;
                return {};
        }
        cur_frame.pc = pc;
    }
    return {};
}
