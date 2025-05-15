#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <fmt/core.h>
#include "interpreter.h"

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

void Interpreter::execute(const std::vector<ConstantPoolInfo>& constant_pool, const MethodInfo& method) {
    std::vector<int32_t> operand_stack;
    std::vector<int32_t> locals(method.max_locals, 0);
    size_t pc = 0;

    auto printStack = [&]() {
        fmt::print("Stack: [");
        for (auto x: operand_stack) {
            fmt::print("{} ", x);
        }
        fmt::print("]");
    };
    auto printLocals = [&]() {
        fmt::print("Locals: [");
        for (auto x: locals) {
            fmt::print("{} ", x);
        }
        fmt::print("]");
    };

    fmt::print("  ");
    printStack();
    fmt::print(" ");
    printLocals();
    fmt::print("\n");
    while (pc < method.code.size()) {
        uint8_t opcode = method.code[pc];
        fmt::print("# Pc:{} Opcode:0x{:0x}\n", pc, opcode);
        pc++;

        float constant_float_one = 1.0f;

        switch (opcode) {
            case 0x00: // nop
                break;
            case 0x03: // iconst_0
                operand_stack.push_back(0);
                break;
            case 0x04: // iconst_1
                operand_stack.push_back(1);
                break;
            case 0x05: // iconst_2
                operand_stack.push_back(2);
                break;
            case 0x06: // iconst_3
                operand_stack.push_back(3);
                break;
            case 0x0d: // fconst_1 (浮点常量1.0)
                operand_stack.push_back(*reinterpret_cast<int32_t*>(&constant_float_one));
                break;
            case 0x12: { // ldc (加载常量池项)
                uint8_t index = method.code[pc];
                pc++; // 跳过1字节的索引
                resolve_ldc(constant_pool, index, operand_stack);
                break;
            }
            case 0x1a: // iload_0
                operand_stack.push_back(locals[0]);
                break;
            case 0x1b: {
                if (method.max_locals <= 1) {
                    throw std::runtime_error("Local variable index 1 out of bounds");
                }
                operand_stack.push_back(locals[1]);
                break;
            }

            case 0x3b: // istore_0
                locals[0] = operand_stack.back();
                operand_stack.pop_back();
                break;
            case 0x3c: { // istore_1
                if (operand_stack.empty()) {
                    throw std::runtime_error("Stack underflow for istore_1");
                }
                locals[1] = operand_stack.back();
                operand_stack.pop_back();
                break;
            }
            case 0x60: { // iadd
                int32_t a = operand_stack.back(); operand_stack.pop_back();
                int32_t b = operand_stack.back(); operand_stack.pop_back();
                operand_stack.push_back(a + b);
                break;
            }
            case 0x84: {
                uint8_t index = method.code[pc++];
                int8_t increment = static_cast<int8_t>(method.code[pc++]);

                // 边界检查
                if (index >= method.max_locals) {
                    throw std::runtime_error(
                        fmt::format("Local variable index {} out of bounds (max_locals={})",
                                    index, method.max_locals)
                    );
                }

                // 执行自增操作
                locals[index] += increment;

                // 调试输出（可选）
                fmt::print("iinc: local[{}] += {} → new value={}\n",
                            index, increment, locals[index]);
                break;
            }
            case 0xac: // ireturn
                fmt::print("Method returned int {}\n", operand_stack.back());
                return;
            case 0xb1: // return (void方法返回)
                fmt::print("Method returned void\n");
                return;
            case 0xb2: { // getstatic (获取静态字段)
                uint16_t index = read_u2_from_code(method.code, pc);
                pc += 2; // 跳过2字节的索引
                resolve_getstatic(constant_pool, index, operand_stack);
                break;
            }
            case 0xb6: { // invokevirtual (调用实例方法)
                uint16_t index = read_u2_from_code(method.code, pc);
                pc += 2; // 跳过2字节的索引
                resolve_invokevirtual(constant_pool, index, operand_stack);
                break;
            }
            default:
                throw std::runtime_error("Unsupported opcode: " + fmt::format("0x{:0x}", opcode));
        }
        fmt::print("  ");
        printStack();
        fmt::print(" ");
        printLocals();
        fmt::print("\n");
    }
}
