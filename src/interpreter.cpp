#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <fmt/core.h>
#include "interpreter.h"
#include "runtime.h"
#include <functional>
#include <limits>
#include <cmath>

JVMThread thread;

void installFrame(const ClassInfo& _class, const MethodInfo& _method, const std::vector<SlotT>& _args) {
    Frame frame(_method.max_locals, _method.max_stack, _class, _method);
    for (int i=0; i<_args.size(); i++) {
        frame.local_vars[i] = _args[i];
    }
    thread.push_frame(frame);
}

uint16_t read_u2_from_code(const std::vector<uint8_t>& code, size_t pc) {
    return (code[pc] << 8) | code[pc + 1];
}

// 根据方法名和描述符查找方法
MethodInfo* Interpreter::find_method(ClassInfo& cf, const std::string& name, const std::string& descriptor) {
    for (auto& m : cf.methods) {
        if (m.name == name && m.descriptor == descriptor) {
            return &m;
        }
    }
    // 查父类
    if (cf.super_class != 0) {
        std::string super_name = cf.constant_pool.get_class_name(cf.super_class);
        if (super_name != "java/lang/Object") {
            ClassInfo& super_cf = load_class(super_name);
            return find_method(super_cf, name, descriptor);
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
std::optional<SlotT> Interpreter::execute(const std::string& class_name, const std::string& method_name, const std::string& method_desc, const std::vector<SlotT>& args) {
    ClassInfo& cf = load_class(class_name);
    MethodInfo* method = find_method(cf, method_name, method_desc);
    if (!method) {
        fmt::print("[execute] cannot find method {}.{} {}\n", class_name, method_name, method_desc);
        return {};
    }
    return _execute(cf, *method, args);
}

void Interpreter::init_opcode_table() {
    opcode_table.resize(256, [](Frame&, size_t&pc, const std::vector<uint8_t>&code, const ClassInfo&, Interpreter&) {
        fmt::print("暂不支持的字节码 {:x}\n", code[pc-1]);
        exit(1);
    });
    // nop
    opcode_table[0x00] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {};
    // aconst_null
    opcode_table[0x01] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        RefT constval = -1;
        cur_frame.operand_stack.push(constval);
        fmt::print("aconst_null\n");
    };
    // iconst_m1
    opcode_table[0x02] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT constval = -1;
        cur_frame.operand_stack.push(constval);
        fmt::print("iconst_m1\n");
    };
    // iconst_0 ~ iconst_5
    for (uint8_t opcode = 0x03; opcode <= 0x08; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            IntT constval = opcode - 0x03;
            cur_frame.operand_stack.push(constval);
            fmt::print("Put int {} on the stack\n", constval);
        };
    }
    // lconst_0 ~ lconst_1
    for (uint8_t opcode = 0x09; opcode <= 0x0a; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            LongT constval = opcode - 0x09;
            cur_frame.operand_stack.push_long(constval);
            fmt::print("Put long {} on the stack\n", constval);
        };
    }
    // fconst_0
    opcode_table[0x0b] = [](Frame& frame, ...) {
        frame.operand_stack.push_float(0.0f);
        fmt::print("Put float 0.0 on the stack\n");
    };
    // fconst_1
    opcode_table[0x0c] = [](Frame& frame, ...) {
        frame.operand_stack.push_float(1.0f);
        fmt::print("Put float 1.0 on the stack\n");
    };
    // fconst_2
    opcode_table[0x0d] = [](Frame& frame, ...) {
        frame.operand_stack.push_float(2.0f);
        fmt::print("Put float 2.0 on the stack\n");
    };
    // dconst_0
    opcode_table[0x0e] = [](Frame& frame, ...) {
        frame.operand_stack.push_double(0.0);
        fmt::print("Put double 0.0 on the stack\n");
    };
    // dconst_1
    opcode_table[0x0f] = [](Frame& frame, ...) {
        frame.operand_stack.push_double(1.0);
        fmt::print("Put double 1.0 on the stack\n");
    };
    // bipush
    opcode_table[0x10] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        ByteT val = (ByteT)code[pc++];
        cur_frame.operand_stack.push(val);
        fmt::print("bipush: Put byte {} on the stack\n", val);
    };
    // sipush
    opcode_table[0x11] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        ShortT val = (static_cast<ShortT>(code[pc]) << 8) | code[pc+1];
        pc += 2;
        cur_frame.operand_stack.push(val);
        fmt::print("sipush: Put short {} on the stack\n", val);
    };
    // ldc
    opcode_table[0x12] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter&) {
        size_t idx = code[pc++];
        const ConstantPoolInfo& cpe = cf.constant_pool[idx];
        if (cpe.tag == ConstantType::INTEGER || cpe.tag == ConstantType::FLOAT) {
            int32_t val = cpe.integerOrFloat;
            cur_frame.operand_stack.push(val);
        } else {
            fmt::print("[ldc] 暂不支持的常量类型 tag={}\n", (int)cpe.tag);
            exit(1);
        }
    };
    // ldc_w
    opcode_table[0x13] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter&) {
        size_t idx = (static_cast<uint16_t>(code[pc]) << 8) | code[pc+1];
        pc += 2;
        const ConstantPoolInfo& cpe = cf.constant_pool[idx];
        if (cpe.tag == ConstantType::INTEGER || cpe.tag == ConstantType::FLOAT) {
            int32_t val = cpe.integerOrFloat;
            cur_frame.operand_stack.push(val);
        } else {
            fmt::print("[ldc_w] 暂不支持的常量类型 tag={}\n", (int)cpe.tag);
            exit(1);
        }
        fmt::print("ldc_w idx={} value={}\n", idx, cpe.integerOrFloat);
    };
    // ldc2_w
    opcode_table[0x14] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter&) {
        size_t idx = (static_cast<uint16_t>(code[pc]) << 8) | code[pc+1];
        pc += 2;
        const ConstantPoolInfo& cpe = cf.constant_pool[idx];
        if (cpe.tag == ConstantType::LONG || cpe.tag == ConstantType::DOUBLE) {
            int64_t value = ((int64_t)cpe.longOrDouble_high_bytes << 32) | cpe.longOrDouble_low_bytes;
            cur_frame.operand_stack.push_long(value);
            fmt::print("ldc2_w idx={} value={}\n", idx, value);
        } else {
            fmt::print("[ldc2_w] 暂不支持的常量类型 tag={}\n", (int)cpe.tag);
            exit(1);
        }
    };
    // iload
    opcode_table[0x15] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        cur_frame.operand_stack.push(cur_frame.local_vars[idx]);
        fmt::print("iload {}\n", idx);
    };
    // lload
    opcode_table[0x16] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        LongT v = cur_frame.local_vars.get_long(idx);
        cur_frame.operand_stack.push_long(v);
        fmt::print("lload {}\n", idx);
    };
    // fload
    opcode_table[0x17] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        cur_frame.operand_stack.push(cur_frame.local_vars[idx]);
        fmt::print("fload {}\n", idx);
    };
    // dload
    opcode_table[0x18] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        DoubleT v = cur_frame.local_vars.get_double(idx);
        cur_frame.operand_stack.push_double(v);
        fmt::print("dload {}\n", idx);
    };
    // aload
    opcode_table[0x19] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        cur_frame.operand_stack.push(cur_frame.local_vars[idx]);
        fmt::print("aload {}\n", idx);
    };
    // iload_0 ~ iload_3
    for (uint8_t opcode = 0x1a; opcode <= 0x1d; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x1a;
            cur_frame.operand_stack.push(cur_frame.local_vars[local_index]);
            fmt::print("Load int from local variable. index {} val {}\n", local_index, cur_frame.local_vars[local_index]);
        };
    }
    // lload_0 ~ lload_3
    for (uint8_t opcode = 0x1e; opcode <= 0x21; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x1e;
            LongT v = cur_frame.local_vars.get_long(local_index);
            cur_frame.operand_stack.push_long(v);
            fmt::print("lload_{}\n", local_index);
        };
    }
    // fload_0 ~ fload_3
    for (uint8_t opcode = 0x22; opcode <= 0x25; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x22;
            cur_frame.operand_stack.push(cur_frame.local_vars[local_index]);
            fmt::print("fload_{}\n", local_index);
        };
    }
    // dload_0 ~ dload_3
    for (uint8_t opcode = 0x26; opcode <= 0x29; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x26;
            DoubleT v = cur_frame.local_vars.get_double(local_index);
            cur_frame.operand_stack.push_double(v);
            fmt::print("dload_{}\n", local_index);
        };
    }
    // aload_0 ~ aload_3: Load reference from local variable
    for (uint8_t opcode = 0x2a; opcode <= 0x2d; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x2a;
            cur_frame.operand_stack.push(cur_frame.local_vars[local_index]);
            fmt::print("aload: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
        };
    }
    // iaload: Load int from array
    opcode_table[0x2e] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();

        fmt::print("iaload: arrayref={}, index={}\n", arrayref, index);
        // TODO: 这里应查找 arrayref 指向的数组对象，并取出 index 位置的元素
        cur_frame.operand_stack.push(0);
    };
    // laload: Load long from array
    opcode_table[0x2f] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("laload: arrayref={}, index={}\n", arrayref, index);
        // TODO: 这里应查找 arrayref 指向的数组对象，并取出 index 位置的元素
        cur_frame.operand_stack.push(0);
    };
    // faload
    opcode_table[0x30] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("faload: arrayref={}, index={}\n", arrayref, index);
        cur_frame.operand_stack.push(0); // TODO: 这里应查找 arrayref 指向的数组对象，并取出 index 位置的元素
    };
    // daload
    opcode_table[0x31] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("daload: arrayref={}, index={}\n", arrayref, index);
        cur_frame.operand_stack.push(0); // TODO: 这里应查找 arrayref 指向的数组对象，并取出 index 位置的元素
    };
    // aaload
    opcode_table[0x32] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("aaload: arrayref={}, index={}\n", arrayref, index);
        cur_frame.operand_stack.push(0); // TODO: 这里应查找 arrayref 指向的数组对象，并取出 index 位置的元素
    };
    // baload
    opcode_table[0x33] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("baload: arrayref={}, index={}\n", arrayref, index);
        cur_frame.operand_stack.push(0); // TODO: 这里应查找 arrayref 指向的数组对象，并取出 index 位置的元素
    };
    // caload
    opcode_table[0x34] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("caload: arrayref={}, index={}\n", arrayref, index);
        cur_frame.operand_stack.push(0); // TODO: 这里应查找 arrayref 指向的数组对象，并取出 index 位置的元素
    };
    // saload
    opcode_table[0x35] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("saload: arrayref={}, index={}\n", arrayref, index);
        cur_frame.operand_stack.push(0); // TODO: 这里应查找 arrayref 指向的数组对象，并取出 index 位置的元素
    };    
    // istore
    opcode_table[0x36] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        cur_frame.local_vars[idx] = cur_frame.operand_stack.pop();
        fmt::print("istore {}\n", idx);
    };
    // lstore
    opcode_table[0x37] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        LongT v = cur_frame.operand_stack.pop_long();
        cur_frame.local_vars.set_long(idx, v);
        fmt::print("lstore {} (long value={})\n", idx, v);
    };
    // fstore
    opcode_table[0x38] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        cur_frame.local_vars[idx] = cur_frame.operand_stack.pop();
        fmt::print("fstore {}\n", idx);
    };
    // dstore
    opcode_table[0x39] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        DoubleT v = cur_frame.operand_stack.pop_double();
        cur_frame.local_vars.set_double(idx, v);
        fmt::print("dstore {}\n", idx);
    };
    // astore
    opcode_table[0x3a] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        cur_frame.local_vars[idx] = cur_frame.operand_stack.pop();
        fmt::print("astore {}\n", idx);
    };
    // istore_0 ~ istore_3: Store int into local variable
    for (uint8_t opcode = 0x3b; opcode <= 0x3e; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x3b;
            cur_frame.local_vars[local_index] = cur_frame.operand_stack.pop();
            fmt::print("istore: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
        };
    }
   // lstore_0 ~ lstore_3: Store long into local variable
    for (uint8_t opcode = 0x3f; opcode <= 0x42; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x3f;
            LongT v = cur_frame.operand_stack.pop_long();
            cur_frame.local_vars.set_long(local_index, v);
            fmt::print("lstore_{}\n", local_index);
        };
    }
    // fstore_0 ~ fstore_3: Store float into local variable
    for (uint8_t opcode = 0x43; opcode <= 0x46; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x43;
            cur_frame.local_vars[local_index] = cur_frame.operand_stack.pop();
            fmt::print("fstore_{}\n", local_index);
        };
    }
    // dstore_0 ~ dstore_3: Store double into local variable
    for (uint8_t opcode = 0x47; opcode <= 0x4a; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x47;
            DoubleT v = cur_frame.operand_stack.pop_double();
            cur_frame.local_vars.set_double(local_index, v);
            fmt::print("dstore_{}\n", local_index);
        };
    }
    // astore_0 ~ astore_3: Store reference into local variable
    for (uint8_t opcode = 0x4b; opcode <= 0x4e; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            size_t local_index = opcode - 0x4b;
            cur_frame.local_vars[local_index] = cur_frame.operand_stack.pop();
            fmt::print("astore: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
        };
    }
    // iastore
    opcode_table[0x4f] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT value = cur_frame.operand_stack.pop();
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("iastore: arrayref={}, index={}, value={}\n", arrayref, index, value); // TODO: 实际应将 value 存入 arrayref 指向的数组的 index 位置
    };
    // lastore
    opcode_table[0x50] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT value = cur_frame.operand_stack.pop_long();
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("lastore: arrayref={}, index={}, value={}\n", arrayref, index, value);
        // TODO: 将 value 存入 arrayref 指向的 long 数组的 index 位置
    };
    // fastore
    opcode_table[0x51] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT value = cur_frame.operand_stack.pop_float();
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("fastore: arrayref={}, index={}, value={}\n", arrayref, index, value);
        // TODO: 将 value 存入 arrayref 指向的 float 数组的 index 位置
    };
    // dastore
    opcode_table[0x52] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT value = cur_frame.operand_stack.pop_double();
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("dastore: arrayref={}, index={}, value={}\n", arrayref, index, value);
        // TODO: 将 value 存入 arrayref 指向的 double 数组的 index 位置
    };
    // aastore
    opcode_table[0x53] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        RefT value = cur_frame.operand_stack.pop();
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("aastore: arrayref={}, index={}, value={}\n", arrayref, index, value);
        // TODO: 将 value 存入 arrayref 指向的引用类型数组的 index 位置
    };
    // bastore
    opcode_table[0x54] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        ByteT value = cur_frame.operand_stack.pop();
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("bastore: arrayref={}, index={}, value={}\n", arrayref, index, value);
        // TODO: 将 value 存入 arrayref 指向的 byte/boolean 数组的 index 位置
    };
    // castore
    opcode_table[0x55] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        CharT value = cur_frame.operand_stack.pop();
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("castore: arrayref={}, index={}, value={}\n", arrayref, index, value);
        // TODO: 将 value 存入 arrayref 指向的 char 数组的 index 位置
    };
    // sastore
    opcode_table[0x56] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        ShortT value = cur_frame.operand_stack.pop();
        IntT index = cur_frame.operand_stack.pop();
        RefT arrayref = cur_frame.operand_stack.pop();
        fmt::print("sastore: arrayref={}, index={}, value={}\n", arrayref, index, value);
        // TODO: 将 value 存入 arrayref 指向的 short 数组的 index 位置
    };
    // pop
    opcode_table[0x57] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT val = cur_frame.operand_stack.pop();
        fmt::print("pop {}\n", val);
    };
    // pop2
    opcode_table[0x58] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT val1 = cur_frame.operand_stack.pop();
        SlotT val2 = cur_frame.operand_stack.pop();
        fmt::print("pop2 {} {}\n", val1, val2);
    };
    // dup: Duplicate the top operand stack value
    opcode_table[0x59] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT val = cur_frame.operand_stack.stack.back();
        cur_frame.operand_stack.push(val);
        fmt::print("dup: dupicate the val {} on the stack\n", val);
    };
    // dup_x1: Duplicate the top operand stack value and insert two values down
    opcode_table[0x5a] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT v1 = cur_frame.operand_stack.pop();
        SlotT v2 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1);
        cur_frame.operand_stack.push(v2);
        cur_frame.operand_stack.push(v1);
        fmt::print("dup_x1\n");
    };
    // dup_x2: Duplicate the top operand stack value and insert two or three values down
    opcode_table[0x5b] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT v1 = cur_frame.operand_stack.pop();
        SlotT v2 = cur_frame.operand_stack.pop();
        SlotT v3 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1);
        cur_frame.operand_stack.push(v3);
        cur_frame.operand_stack.push(v2);
        cur_frame.operand_stack.push(v1);
        fmt::print("dup_x2\n");
    };
    // dup2
    opcode_table[0x5c] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT v1 = cur_frame.operand_stack.pop();
        SlotT v2 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v2);
        cur_frame.operand_stack.push(v1);
        cur_frame.operand_stack.push(v2);
        cur_frame.operand_stack.push(v1);
        fmt::print("dup2\n");
    };
    // dup2_x1: Duplicate the top one or two operand stack values and insert two or three values down
    opcode_table[0x5d] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT v1 = cur_frame.operand_stack.pop();
        SlotT v2 = cur_frame.operand_stack.pop();
        SlotT v3 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v2);
        cur_frame.operand_stack.push(v1);
        cur_frame.operand_stack.push(v3);
        cur_frame.operand_stack.push(v2);
        cur_frame.operand_stack.push(v1);
        fmt::print("dup2_x1\n");
    };
    // dup2_x2: Duplicate the top one or two operand stack values and insert two, three, or four values down
    opcode_table[0x5e] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT v1 = cur_frame.operand_stack.pop();
        SlotT v2 = cur_frame.operand_stack.pop();
        SlotT v3 = cur_frame.operand_stack.pop();
        SlotT v4 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v2);
        cur_frame.operand_stack.push(v1);
        cur_frame.operand_stack.push(v4);
        cur_frame.operand_stack.push(v3);
        cur_frame.operand_stack.push(v2);
        cur_frame.operand_stack.push(v1);
        fmt::print("dup2_x2\n");
    };
    // swap
    opcode_table[0x5f] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        SlotT v1 = cur_frame.operand_stack.pop();
        SlotT v2 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1);
        cur_frame.operand_stack.push(v2);
        fmt::print("swap\n");
    };
    // iadd
    opcode_table[0x60] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        fmt::print("iadd {} + {}\n", v1, v2);
        cur_frame.operand_stack.push(v1 + v2);
    };
    // ladd
    opcode_table[0x61] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 + v2);
        fmt::print("ladd {} + {}\n", v1, v2);
    };
    // fadd
    opcode_table[0x62] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v2 = cur_frame.operand_stack.pop_float();
        FloatT v1 = cur_frame.operand_stack.pop_float();
        cur_frame.operand_stack.push_float(v1 + v2);
        fmt::print("fadd {} + {}\n", v1, v2);
    };
    // dadd
    opcode_table[0x63] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v2 = cur_frame.operand_stack.pop_double();
        DoubleT v1 = cur_frame.operand_stack.pop_double();
        cur_frame.operand_stack.push_double(v1 + v2);
        fmt::print("dadd {} + {}\n", v1, v2);
    };
    // isub
    opcode_table[0x64] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 - v2);
        fmt::print("isub {} - {}\n", v1, v2);
    };
    // lsub
    opcode_table[0x65] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 - v2);
        fmt::print("lsub {} - {}\n", v1, v2);
    };
    // fsub
    opcode_table[0x66] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v2 = cur_frame.operand_stack.pop_float();
        FloatT v1 = cur_frame.operand_stack.pop_float();
        cur_frame.operand_stack.push_float(v1 - v2);
        fmt::print("fsub {} - {}\n", v1, v2);
    };
    // dsub
    opcode_table[0x67] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v2 = cur_frame.operand_stack.pop_double();
        DoubleT v1 = cur_frame.operand_stack.pop_double();
        cur_frame.operand_stack.push_double(v1 - v2);
        fmt::print("dsub {} - {}\n", v1, v2);
    };
    // imul
    opcode_table[0x68] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 * v2);
        fmt::print("imul {} * {}\n", v1, v2);
    };
    // lmul
    opcode_table[0x69] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 * v2);
        fmt::print("lmul {} * {}\n", v1, v2);
    };
    // fmul
    opcode_table[0x6a] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v2 = cur_frame.operand_stack.pop_float();
        FloatT v1 = cur_frame.operand_stack.pop_float();
        cur_frame.operand_stack.push_float(v1 * v2);
        fmt::print("fmul {} * {}\n", v1, v2);
    };
    // dmul
    opcode_table[0x6b] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v2 = cur_frame.operand_stack.pop_double();
        DoubleT v1 = cur_frame.operand_stack.pop_double();
        cur_frame.operand_stack.push_double(v1 * v2);
        fmt::print("dmul {} * {}\n", v1, v2);
    };
    // idiv
    opcode_table[0x6c] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 / v2);
        fmt::print("idiv {} / {}\n", v1, v2);
    };
    // ldiv
    opcode_table[0x6d] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 / v2);
        fmt::print("ldiv {} / {}\n", v1, v2);
    };
    // fdiv
    opcode_table[0x6e] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v2 = cur_frame.operand_stack.pop_float();
        FloatT v1 = cur_frame.operand_stack.pop_float();
        cur_frame.operand_stack.push_float(v1 / v2);
        fmt::print("fdiv {} / {}\n", v1, v2);
    };
    // ddiv
    opcode_table[0x6f] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v2 = cur_frame.operand_stack.pop_double();
        DoubleT v1 = cur_frame.operand_stack.pop_double();
        cur_frame.operand_stack.push_double(v1 / v2);
        fmt::print("ddiv {} / {}\n", v1, v2);
    };
    // irem
    opcode_table[0x70] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 % v2);
        fmt::print("irem {} % {}\n", v1, v2);
    };
    // lrem
    opcode_table[0x71] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 % v2);
        fmt::print("lrem {} % {}\n", v1, v2);
    };
    // frem
    opcode_table[0x72] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v2 = cur_frame.operand_stack.pop_float();
        FloatT v1 = cur_frame.operand_stack.pop_float();
        FloatT result;
        if (std::isnan(v1) || std::isnan(v2)) {
            result = std::numeric_limits<FloatT>::quiet_NaN();
        } else if (std::isinf(v1) || v2 == 0.0f || std::isinf(v2) && std::isinf(v1)) {
            result = std::numeric_limits<FloatT>::quiet_NaN();
        } else if (std::isfinite(v1) && std::isinf(v2)) {
            result = v1;
        } else if (v1 == 0.0f && std::isfinite(v2)) {
            result = v1;
        } else {
            result = v1 - v2 * std::trunc(v1 / v2);
        }
        cur_frame.operand_stack.push_float(result);
        fmt::print("frem {} % {} = {}\n", v1, v2, result);
    };
    // drem
    opcode_table[0x73] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v2 = cur_frame.operand_stack.pop_double();
        DoubleT v1 = cur_frame.operand_stack.pop_double();
        DoubleT result;
        if (std::isnan(v1) || std::isnan(v2)) {
            result = std::numeric_limits<DoubleT>::quiet_NaN();
        } else if (std::isinf(v1) || v2 == 0.0 || (std::isinf(v2) && std::isinf(v1))) {
            result = std::numeric_limits<DoubleT>::quiet_NaN();
        } else if (std::isfinite(v1) && std::isinf(v2)) {
            result = v1;
        } else if (v1 == 0.0 && std::isfinite(v2)) {
            result = v1;
        } else {
            result = v1 - v2 * std::trunc(v1 / v2);
        }
        cur_frame.operand_stack.push_double(result);
        fmt::print("drem {} % {} = {}\n", v1, v2, result);
    };
    // ineg
    opcode_table[0x74] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(-v);
        fmt::print("ineg {}\n", v);
    };
    // lneg
    opcode_table[0x75] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(-v);
        fmt::print("lneg {}\n", v);
    };
    // fneg
    opcode_table[0x76] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v = cur_frame.operand_stack.pop_float();
        cur_frame.operand_stack.push_float(-v);
        fmt::print("fneg {}\n", v);
    };
    // dneg
    opcode_table[0x77] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v = cur_frame.operand_stack.pop_double();
        cur_frame.operand_stack.push_double(-v);
        fmt::print("dneg {}\n", v);
    };
    // ishl: Shift left int
    opcode_table[0x78] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 << (v2 & 0x1F));
        fmt::print("ishl {} << {}\n", v1, v2);
    };
    // lshl: Shift left long
    opcode_table[0x79] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 << (v2 & 0x3F));
        fmt::print("lshl {} << {}\n", v1, v2);
    };
    // ishr: Shift right int
    opcode_table[0x7a] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 >> (v2 & 0x1F));
        fmt::print("ishr {} >> {}\n", v1, v2);
    };
    // lshr: Shift right long
    opcode_table[0x7b] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 >> (v2 & 0x3F));
        fmt::print("lshr {} >> {}\n", v1, v2);
    };
    // iushr: Logical shift right int
    opcode_table[0x7c] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push((UIntT)v1 >> (v2 & 0x1F));
        fmt::print("iushr {} >>> {}\n", v1, v2);
    };
    // lushr: Logical shift right long
    opcode_table[0x7d] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long((ULongT)v1 >> (v2 & 0x3F));
        fmt::print("lushr {} >>> {}\n", v1, v2);
    };
    // iand: Boolean AND int
    opcode_table[0x7e] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 & v2);
        fmt::print("iand {} & {}\n", v1, v2);
    };
    // land: Boolean AND long
    opcode_table[0x7f] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 & v2);
        fmt::print("land {} & {}\n", v1, v2);
    };
    // ior: Boolean OR int
    opcode_table[0x80] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 | v2);
        fmt::print("ior {} | {}\n", v1, v2);
    };
    // lor: Boolean OR long
    opcode_table[0x81] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 | v2);
        fmt::print("lor {} | {}\n", v1, v2);
    };
    // ixor: Boolean XOR int
    opcode_table[0x82] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push(v1 ^ v2);
        fmt::print("ixor {} ^ {}\n", v1, v2);
    };
    // lxor: Boolean XOR long
    opcode_table[0x83] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_long(v1 ^ v2);
        fmt::print("lxor {} ^ {}\n", v1, v2);
    };
    // iinc: Increment local variable by constant
    opcode_table[0x84] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>&code, const ClassInfo&, Interpreter&) {
        size_t idx = code[pc++];
        IntT inc = (int8_t)code[pc++];
        cur_frame.local_vars[idx] += inc;
    };
    // i2l: Convert int to long
    opcode_table[0x85] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push_long((LongT)(v));
        fmt::print("i2l {}\n", v);
    };
    // i2f: Convert int to float
    opcode_table[0x86] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push_float((FloatT)(v));
        fmt::print("i2f {}\n", v);
    };
    // i2d: Convert int to double
    opcode_table[0x87] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push_double((DoubleT)v);
        fmt::print("i2d {}\n", v);
    };
    // l2i: Convert long to int
    opcode_table[0x88] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push((IntT)v);
        fmt::print("l2i {}\n", v);
    };
    // l2f: Convert long to float
    opcode_table[0x89] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push((FloatT)v);
        fmt::print("l2f {}\n", v);
    };
    // l2d: Convert long to double
    opcode_table[0x8a] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v = cur_frame.operand_stack.pop_long();
        cur_frame.operand_stack.push_double((DoubleT)v);
        fmt::print("l2d {}\n", v);
    };
    // f2i: Convert float to int
    opcode_table[0x8b] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v = cur_frame.operand_stack.pop_float();
        cur_frame.operand_stack.push((IntT)v);
        fmt::print("f2i {}\n", v);
    };
    // f2l: Convert float to long
    opcode_table[0x8c] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v = cur_frame.operand_stack.pop_float();
        cur_frame.operand_stack.push((LongT)v);
        fmt::print("f2l {}\n", v);
    };
    // f2d: Convert float to double
    opcode_table[0x8d] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v = cur_frame.operand_stack.pop_float();
        cur_frame.operand_stack.push_double((DoubleT)v);
        fmt::print("f2d {}\n", v);
    };
    // d2i: Convert double to int
    opcode_table[0x8e] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v = cur_frame.operand_stack.pop_double();
        cur_frame.operand_stack.push((IntT)v);
        fmt::print("d2i {}\n", v);
    };
    // d2l: Convert double to long
    opcode_table[0x8f] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v = cur_frame.operand_stack.pop_double();
        cur_frame.operand_stack.push((LongT)v);
        fmt::print("d2l {}\n", v);
    };
    // d2f: Convert double to float
    opcode_table[0x90] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v = cur_frame.operand_stack.pop_double();
        cur_frame.operand_stack.push_float((FloatT)v);
        fmt::print("d2f {}\n", v);
    };
    // i2b: Convert int to byte
    opcode_table[0x91] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push((ByteT)v);
        fmt::print("i2b {}\n", v);
    };
    // i2c: Convert int to char
    opcode_table[0x92] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push((CharT)v);
        fmt::print("i2c {}\n", v);
    };
    // i2s: Convert int to short
    opcode_table[0x93] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        cur_frame.operand_stack.push((ShortT)v);
        fmt::print("i2s {}\n", v);
    };
    // lcmp
    opcode_table[0x94] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        LongT v2 = cur_frame.operand_stack.pop_long();
        LongT v1 = cur_frame.operand_stack.pop_long();
        IntT result = (v1 == v2) ? 0 : (v1 < v2 ? -1 : 1);
        cur_frame.operand_stack.push_long(result);
        fmt::print("lcmp {} {} => {}\n", v1, v2, result);
    };
    // fcmpl
    opcode_table[0x95] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v2 = cur_frame.operand_stack.pop_float();
        FloatT v1 = cur_frame.operand_stack.pop_float();
        IntT result = (v1 == v2) ? 0 : (v1 < v2 ? -1 : 1);
        cur_frame.operand_stack.push(result);
        fmt::print("fcmpl {} {} => {}\n", v1, v2, result);
    };
    // fcmpg
    opcode_table[0x96] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        FloatT v2 = cur_frame.operand_stack.pop_float();
        FloatT v1 = cur_frame.operand_stack.pop_float();
        IntT result = (v1 == v2) ? 0 : (v1 > v2 ? 1 : -1);
        cur_frame.operand_stack.push(result);
        fmt::print("fcmpg {} {} => {}\n", v1, v2, result);
    };
    // dcmpl
    opcode_table[0x97] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v2 = cur_frame.operand_stack.pop();
        DoubleT v1 = cur_frame.operand_stack.pop();
        IntT result = (v1 == v2) ? 0 : (v1 < v2 ? -1 : 1);
        cur_frame.operand_stack.push(result);
        fmt::print("dcmpl {} {} => {}\n", v1, v2, result);
    };
    // dcmpg
    opcode_table[0x98] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        DoubleT v2 = cur_frame.operand_stack.pop_double();
        DoubleT v1 = cur_frame.operand_stack.pop_double();
        IntT result = (v1 == v2) ? 0 : (v1 > v2 ? 1 : -1);
        cur_frame.operand_stack.push(result);
        fmt::print("dcmpg {} {} => {}\n", v1, v2, result);
    };
    // ifeq
    opcode_table[0x99] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v == 0) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // ifne
    opcode_table[0x9a] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v != 0) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // iflt
    opcode_table[0x9b] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v < 0) pc = (size_t)((int)pc + offset - 3);
    };
    // ifge
    opcode_table[0x9c] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v >= 0) pc = (size_t)((int)pc + offset - 3);
    };
    // ifgt
    opcode_table[0x9d] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v > 0) pc = (size_t)((int)pc + offset - 3);
    };
    // ifle
    opcode_table[0x9e] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v <= 0) pc = (size_t)((int)pc + offset - 3);
    };
    // if_icmpeq
    opcode_table[0x9f] = [](Frame& cur_frame, size_t&pc, const std::vector<uint8_t>&code, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 == v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmpne
    opcode_table[0xa0] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 != v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmplt
    opcode_table[0xa1] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 < v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmpge
    opcode_table[0xa2] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 >= v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmpgt
    opcode_table[0xa3] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 > v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmple
    opcode_table[0xa4] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT v2 = cur_frame.operand_stack.pop();
        IntT v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 <= v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_acmpeq
    opcode_table[0xa5] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT ref2 = cur_frame.operand_stack.pop();
        IntT ref1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (ref1 == ref2) pc = (size_t)((int)pc + offset - 3);
    };
    // if_acmpne
    opcode_table[0xa6] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        IntT ref2 = cur_frame.operand_stack.pop();
        IntT ref1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (ref1 != ref2) pc = (size_t)((int)pc + offset - 3);
    };
    // goto
    opcode_table[0xa7] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        pc = (size_t)((int)pc + offset - 3); // -3: opcode+2字节已读
    };
    // jsr: Jump to SubRoutine
    opcode_table[0xa8] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        cur_frame.operand_stack.push(pc); // 压入返回地址
        pc = (size_t)((int)pc + offset - 3);
        fmt::print("jsr: jump to {}, return address {}\n", pc, pc); // todo: implement
    };
    // ret
    opcode_table[0xa9] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        uint8_t idx = code[pc++];
        pc = cur_frame.local_vars[idx];
        fmt::print("ret {}\n", idx); // todo: implement
    };
    // tableswitch
    opcode_table[0xaa] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        fmt::print("tableswitch not implemented\n");
        exit(1);
    };
    // lookupswitch
    opcode_table[0xab] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        fmt::print("lookupswitch not implemented\n");
        exit(1);
    };
    // ireturn
    opcode_table[0xac] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter& interp) {
        int32_t ret = cur_frame.operand_stack.pop();
        thread.pop_frame();
        if (!thread.empty()) {
            thread.current_frame().operand_stack.push(ret);
        }
    };
    // lreturn, freturn, dreturn, areturn
    opcode_table[0xad] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) { 
        int32_t ret = cur_frame.operand_stack.pop();
        thread.pop_frame();
        if (!thread.empty()) {
            thread.current_frame().operand_stack.push(ret);
        }
    };
    opcode_table[0xae] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) { 
        int32_t ret = cur_frame.operand_stack.pop();
        thread.pop_frame();
        if (!thread.empty()) {
            thread.current_frame().operand_stack.push(ret);
        }
    };
    opcode_table[0xaf] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        int32_t ret = cur_frame.operand_stack.pop();
        thread.pop_frame();
        if (!thread.empty()) {
            thread.current_frame().operand_stack.push(ret);
        }
    };
    opcode_table[0xb0] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        int32_t ret = cur_frame.operand_stack.pop();
        thread.pop_frame();
        if (!thread.empty()) {
            thread.current_frame().operand_stack.push(ret);
        }
    };
    // return
    opcode_table[0xb1] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter& interp) {
        thread.pop_frame();
    };
    // getstatic
    opcode_table[0xb2] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        uint16_t idx = (code[pc] << 8) | code[pc+1];
        pc += 2;
        interp.resolve_getstatic(cf, idx, cur_frame);
    };
    // putstatic
    opcode_table[0xb3] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        uint16_t idx = (code[pc] << 8) | code[pc+1];
        pc += 2;
        fmt::print("putstatic: idx={}\n", idx); // TODO: 实现静态字段赋值
    };
    // getfield
    opcode_table[0xb4] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        int16_t idx = (code[pc] << 8) | code[pc+1];
        pc += 2;
        // 解析字段名和类型
        const ConstantPoolInfo& fieldref = cf.constant_pool[idx];
        uint16_t name_type_idx = fieldref.fieldref_name_type_index;
        auto [field_name, field_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
        RefT obj_ref = cur_frame.operand_stack.pop();
        SlotT val = interp.get_field(obj_ref, field_name);
        cur_frame.operand_stack.push(val);
        fmt::print("getfield: get obj:{} field:{} val:{}\n", obj_ref, field_name, val);
    };
    // putfield
    opcode_table[0xb5] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        int16_t idx = (code[pc] << 8) | code[pc+1];
        pc += 2;
        // 解析字段名和类型
        const ConstantPoolInfo& fieldref = cf.constant_pool[idx];
        uint16_t name_type_idx = fieldref.fieldref_name_type_index;
        auto [field_name, field_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
        int32_t val = cur_frame.operand_stack.pop();
        RefT obj_ref = cur_frame.operand_stack.pop();
        interp.put_field(obj_ref, field_name, val);
        fmt::print("putfield: set obj:{} field:{} val:{}\n", obj_ref, field_name, val);
    };
    // invokevirtual
    opcode_table[0xb6] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        uint16_t idx = (code[pc] << 8) | code[pc+1];
        pc += 2;
        const ConstantPoolInfo& methodref = cf.constant_pool[idx];
        uint16_t class_idx = methodref.methodref_class_index;
        uint16_t name_type_idx = methodref.methodref_name_type_index;
        std::string class_name = cf.constant_pool.get_utf8_str(cf.constant_pool[class_idx].class_name_index);
        auto [method_name, method_desc] = cf.constant_pool.get_name_and_type(name_type_idx);

        // 特殊处理System.out.println
        if (class_name == "java/io/PrintStream" && method_name == "println") {
            SlotT val = cur_frame.operand_stack.pop(); // 要打印的值
            RefT obj_ref = cur_frame.operand_stack.pop(); // System.out对象引用
            fmt::print("[System.out.println] {}\n", val);
            return;
        }
        fmt::print("[invokevirtual] classname:{} method_name:{} method_desc:{}\n", class_name.c_str(), method_name.c_str(), method_desc.c_str());

        // 弹出参数
        size_t arg_count = count_method_args(method_desc);
        arg_count++; // obj ref
        std::vector<SlotT> args(arg_count);
        for (int i = arg_count - 1; i >= 0; --i) {
            args[i] = cur_frame.operand_stack.pop();
            fmt::print("setup args[{}]={}\n", i, args[i]);
        }
        fmt::print("objref {} \n", args[0]);
        ClassInfo& target_class = interp.load_class(class_name);
        MethodInfo* target_method = interp.find_method(target_class, method_name, method_desc);
        if (target_method) {
            // 设置新帧并压入被调用栈
            installFrame(target_class, *target_method, args);
        } else {
            fmt::print("invokevirtual invaid method");
            exit(1);
        }
    };
    // invokespecial
    opcode_table[0xb7] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        uint16_t idx = (code[pc] << 8) | code[pc+1];
        pc += 2;
        const ConstantPoolInfo& cp_entry = cf.constant_pool[idx];
        uint16_t methodref_class_index = cp_entry.methodref_class_index;
        uint16_t name_type_idx = cp_entry.methodref_name_type_index;
        auto class_name = cf.constant_pool.get_class_name(methodref_class_index);
        auto [method_name, method_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
        fmt::print("[invokespecial] idx{} classname:{} method_name:{} method_desc:{}\n", idx, class_name, method_name, method_desc);

        if (class_name=="java/lang/Object" && method_name=="<init>") {
            // todo: remove here if implement multilclass
            return;
        }

        // 弹出参数
        size_t arg_count = count_method_args(method_desc);
        arg_count++; // object ref
        std::vector<SlotT> args(arg_count);
        for (int i = arg_count - 1; i >= 0; --i) {
            args[i] = cur_frame.operand_stack.pop();
            fmt::print("setup args[{}]={}\n", i, args[i]);
        }
        if (args[0]==0) return;
        fmt::print("objref {} \n", args[0]);

        ClassInfo& target_class = interp.load_class(class_name);
        MethodInfo* target_method = interp.find_method(target_class, method_name, method_desc);
        if (target_method) {
            installFrame(target_class, *target_method, args);
        } else {
            fmt::print("invokespecial invaid method");
            exit(1);
        }
    };
    // invokestatic
    opcode_table[0xb8] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        uint16_t idx = (code[pc] << 8) | code[pc+1];
        pc += 2;
        const ConstantPoolInfo& methodref = cf.constant_pool[idx];
        uint16_t class_idx = methodref.methodref_class_index;
        uint16_t name_type_idx = methodref.methodref_name_type_index;
        std::string class_name = cf.constant_pool.get_utf8_str(cf.constant_pool[class_idx].class_name_index);
        auto [method_name, method_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
        fmt::print("[invokestatic] classname:{} method_name:{} method_desc:{}\n", class_name.c_str(), method_name.c_str(), method_desc.c_str());

        // 弹出参数
        size_t arg_count = count_method_args(method_desc);
        std::vector<SlotT> args(arg_count);
        for (int i = arg_count - 1; i >= 0; --i) {
            args[i] = cur_frame.operand_stack.pop();
            fmt::print("setup args[{}]={}\n", i, args[i]);
        }

        ClassInfo& target_class = interp.load_class(class_name);
        MethodInfo* target_method = interp.find_method(target_class, method_name, method_desc);
        if (target_method) {
            installFrame(target_class, *target_method, args);
        } else {
            fmt::print("invokestatic invaid method");
            exit(1);
        }
    };
    // invokeinterface
    opcode_table[0xb9] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        uint16_t idx = ((uint16_t)code[pc] << 8) | (uint16_t)code[pc+1];
        uint8_t count = code[pc+2];
        uint8_t zero = code[pc+3];
        pc += 4;
        fmt::print("invokeinterface: idx={}, count={}, zero={}\n", idx, count, zero);
        // TODO: 实现接口方法调用
    };
    // new
    opcode_table[0xbb] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter& interp) {
        pc += 2; // 跳过常量池索引
        int obj_ref = interp.new_object();
        cur_frame.operand_stack.push(obj_ref);
        fmt::print("alloc new object {}\n", obj_ref);
    };
    // invokedynamic
    opcode_table[0xba] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("invokedynamic not implemented\n");
        exit(1);
    };
    // newarray
    opcode_table[0xbc] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("newarray not implemented\n");
        exit(1);
    };
    // anewarray
    opcode_table[0xbd] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("anewarray not implemented\n");
        exit(1);
    };
    // arraylength
    opcode_table[0xbe] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("arraylength not implemented\n");
        exit(1);
    };
    // athrow
    opcode_table[0xbf] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("athrow not implemented\n");
        exit(1);
    };
    // checkcast
    opcode_table[0xc0] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter&) {
        size_t idx = (((uint16_t)code[pc]) << 8) | (uint16_t)code[pc+1];
        pc += 2;
        fmt::print("checkcast: idx={}\n", idx);
        // TODO: 实现类型检查
    };
    // instanceof
    opcode_table[0xc1] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("instanceof not implemented\n");
        exit(1);
    };
    // monitorenter
    opcode_table[0xc2] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("monitorenter not implemented\n");
        exit(1);
    };
    // monitorexit
    opcode_table[0xc3] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("monitorexit not implemented\n");
        exit(1);
    };
    // wide
    opcode_table[0xc4] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("wide not implemented\n");
        exit(1);
    };
    // multianewarray
    opcode_table[0xc5] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("multianewarray not implemented\n");
        exit(1);
    };
    // ifnull
    opcode_table[0xc6] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        RefT ref = cur_frame.operand_stack.pop_ref();
        int16_t offset = (int16_t)(((uint16_t)code[pc] << 8) | (uint16_t)code[pc+1]);
        pc += 2;
        if (ref == 0) pc = (size_t)((int)pc + offset - 3);
    };
    // ifnonnull
    opcode_table[0xc7] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        RefT ref = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)(((uint16_t)code[pc] << 8) | (uint16_t)code[pc+1]);
        pc += 2;
        if (ref != 0) pc = (size_t)((int)pc + offset - 3);
    };
    // goto_w
    opcode_table[0xc8] = [](Frame&, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int32_t offset = ((uint32_t)code[pc] << 24) | ((uint32_t)code[pc+1] << 16) | ((uint32_t)code[pc+2] << 8) | (uint32_t)code[pc+3];
        pc += 4;
        pc = (size_t)((int)pc + offset - 5); // -5: opcode+4字节已读
        fmt::print("goto_w\n");
    };
    // jsr_w
    opcode_table[0xc9] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int32_t offset = ((uint32_t)code[pc] << 24) | ((uint32_t)code[pc+1] << 16) | ((uint32_t)code[pc+2] << 8) | (uint32_t)code[pc+3];
        pc += 4;
        cur_frame.operand_stack.push(pc);
        pc = (size_t)((int)pc + offset - 5);
        fmt::print("jsr_w\n");
    };
    // breakpoint
    opcode_table[0xca] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("breakpoint (reserved)\n");
        exit(1);
    };
    // impdep1
    opcode_table[0xfe] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("impdep1 (reserved)\n");
        exit(1);
    };
    // impdep2
    opcode_table[0xff] = [](Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        fmt::print("impdep2 (reserved)\n");
        exit(1);
    };
}

std::optional<SlotT> Interpreter::_execute(ClassInfo& entry_class, const MethodInfo& entry_method, const std::vector<SlotT>& entry_args) {
    if (opcode_table.empty()) init_opcode_table();
    installFrame(entry_class, entry_method, entry_args);
    while (!thread.empty()) {
        Frame& cur_frame = thread.current_frame();
        auto &pc = cur_frame.pc;
        auto &code = cur_frame.method_info.code;
        auto &cf = cur_frame.class_info;
        auto &classname = cf.constant_pool.get_class_name(cf.this_class);
        if (pc >= code.size()) {
            fmt::print("pc reach code end but no return");
            exit(1);
        }
        uint8_t opcode = code[pc++];
        fmt::print("[execute] className:{} method: {}", classname, cur_frame.method_info.name);
        fmt::print(" pc 0x{:x} op 0x{:x} \n", pc, opcode);
        opcode_table[opcode](cur_frame, pc, code, cf, *this);
        cur_frame.pc = pc;
    }
    return {};
}

// 分配新对象，返回对象引用（索引）
int Interpreter::new_object() {
    JVMObject obj;
    object_pool.push_back(obj);
    return object_pool.size() - 1; // 返回对象在池中的索引
}
// 设置对象字段
void Interpreter::put_field(int obj_ref, const std::string& field, SlotT value) {
    if (obj_ref >= 0 && obj_ref < object_pool.size()) {
        object_pool[obj_ref].fields[field] = value;
    }
}
// 获取对象字段
SlotT Interpreter::get_field(int obj_ref, const std::string& field) {
    if (obj_ref >= 0 && obj_ref < object_pool.size()) {
        return object_pool[obj_ref].fields[field];
    }
    return 0;
}

// 处理getstatic
void Interpreter::resolve_getstatic(const ClassInfo& cf, uint16_t index, Frame& frame) {
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


