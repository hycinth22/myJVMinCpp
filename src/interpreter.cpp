#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <fmt/core.h>
#include "interpreter.h"
#include "runtime.h"
#include <functional>

JVMThread thread;

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
std::optional<int32_t> Interpreter::execute(const std::string& class_name, const std::string& method_name, const std::string& method_desc, const std::vector<int32_t>& args) {
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
    // iconst_0 ~ iconst_5
    for (int opcode = 0x03; opcode <= 0x08; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            int constval = opcode - 0x03;
            cur_frame.operand_stack.push(constval);
            fmt::print("Put constant {} on the stack\n", constval);
        };
    }
    // bipush
    opcode_table[0x10] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int8_t val = (int8_t)code[pc++];
        cur_frame.operand_stack.push(val);
        fmt::print("bipush: Put imm {} on the stack\n", val);
    };
    // sipush
    opcode_table[0x11] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int16_t val = (code[pc] << 8) | code[pc+1];
        pc += 2;
        cur_frame.operand_stack.push(val);
        fmt::print("sipush: Put imm {} on the stack\n", val);
    };
    // ldc
    opcode_table[0x12] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter&) {
        uint8_t idx = code[pc++];
        const ConstantPoolInfo& cpe = cf.constant_pool[idx];
        if (cpe.tag == ConstantType::INTEGER || cpe.tag == ConstantType::FLOAT) {
            cur_frame.operand_stack.push(cpe.integerOrFloat);
        } else {
            fmt::print("[ldc] 暂不支持的常量类型 tag={}\n", (int)cpe.tag);
            exit(1);
        }
    };
    // iload_0 ~ iload_3
    for (int opcode = 0x1a; opcode <= 0x1d; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            int local_index = opcode - 0x1a;
            cur_frame.operand_stack.push(cur_frame.local_vars[local_index]);
            fmt::print("Load int from local variable. index {} val {}\n", local_index, cur_frame.local_vars[local_index]);
        };
    }
    // aload_0 ~ aload_3: Load reference from local variable
    for (int opcode = 0x2a; opcode <= 0x2d; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            int local_index = opcode - 0x2a;
            cur_frame.operand_stack.push(cur_frame.local_vars[local_index]);
            fmt::print("aload: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
        };
    }
    // istore
    opcode_table[0x36] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        uint8_t idx = code[pc++];
        cur_frame.local_vars[idx] = cur_frame.operand_stack.pop();
    };
    // istore_0 ~ istore_3: Store int into local variable
    for (int opcode = 0x3b; opcode <= 0x3e; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            int local_index = opcode - 0x3b;
            cur_frame.local_vars[local_index] = cur_frame.operand_stack.pop();
            fmt::print("istore: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
        };
    }
    // astore_0 ~ astore_3: Store reference into local variable
    for (int opcode = 0x4b; opcode <= 0x4e; ++opcode) {
        opcode_table[opcode] = [opcode](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
            int local_index = opcode - 0x4b;
            cur_frame.local_vars[local_index] = cur_frame.operand_stack.pop();
            fmt::print("astore: local{} = {}\n", local_index, cur_frame.local_vars[local_index]);
        };
    }
    // dup: Duplicate the top operand stack value
    opcode_table[0x59] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        int32_t val = cur_frame.operand_stack.stack.back();
        cur_frame.operand_stack.push(val);
        fmt::print("dup: dupicate the val {} on the stack\n", val);
    };
    // iadd
    opcode_table[0x60] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&) {
        int32_t v2 = cur_frame.operand_stack.pop();
        int32_t v1 = cur_frame.operand_stack.pop();
        fmt::print("iadd {} + {}\n", v1, v2);
        cur_frame.operand_stack.push(v1 + v2);
    };
    // iinc
    opcode_table[0x84] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>&code, const ClassInfo&, Interpreter&) {
        uint8_t idx = code[pc++];
        int8_t inc = (int8_t)code[pc++];
        cur_frame.local_vars[idx] += inc;
    };
    // if_icmpeq
    opcode_table[0x9f] = [](Frame& cur_frame, size_t&pc, const std::vector<uint8_t>&code, const ClassInfo&, Interpreter&) {
        int32_t v2 = cur_frame.operand_stack.pop();
        int32_t v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 == v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmpne
    opcode_table[0xa0] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int32_t v2 = cur_frame.operand_stack.pop();
        int32_t v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 != v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmplt
    opcode_table[0xa1] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int32_t v2 = cur_frame.operand_stack.pop();
        int32_t v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 < v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmpge
    opcode_table[0xa2] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int32_t v2 = cur_frame.operand_stack.pop();
        int32_t v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 >= v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmpgt
    opcode_table[0xa3] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int32_t v2 = cur_frame.operand_stack.pop();
        int32_t v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 > v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // if_icmple
    opcode_table[0xa4] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int32_t v2 = cur_frame.operand_stack.pop();
        int32_t v1 = cur_frame.operand_stack.pop();
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        if (v1 <= v2) {
            pc = (size_t)((int)pc + offset - 3);
        }
    };
    // goto
    opcode_table[0xa7] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter&) {
        int16_t offset = (int16_t)((code[pc] << 8) | code[pc+1]);
        pc += 2;
        pc = (size_t)((int)pc + offset - 3); // -3: opcode+2字节已读
    };
    // ireturn
    opcode_table[0xac] = [](Frame& cur_frame, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter& interp) {
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
    // getfield
    opcode_table[0xb4] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo& cf, Interpreter& interp) {
        int16_t idx = (code[pc] << 8) | code[pc+1];
        pc += 2;
        // 解析字段名和类型
        const ConstantPoolInfo& fieldref = cf.constant_pool[idx];
        uint16_t name_type_idx = fieldref.fieldref_name_type_index;
        auto [field_name, field_desc] = cf.constant_pool.get_name_and_type(name_type_idx);
        int obj_ref = cur_frame.operand_stack.pop();
        int32_t val = interp.get_field(obj_ref, field_name);
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
        int obj_ref = cur_frame.operand_stack.pop();
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
            int32_t val = cur_frame.operand_stack.pop(); // 要打印的值
            int obj_ref = cur_frame.operand_stack.pop(); // System.out对象引用
            fmt::print("[System.out.println] {}\n", val);
            return;
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
        ClassInfo& target_class = interp.load_class(class_name);
        MethodInfo* target_method = interp.find_method(target_class, method_name, method_desc);
        if (target_method) {
            // 设置新帧并压入被调用栈
            auto installFrame = [](const ClassInfo& _class, const MethodInfo& _method, const std::vector<int32_t>& _args) {
                Frame frame(_method.max_locals, _method.max_stack, _class, _method);
                for (int i=0; i<_args.size(); i++) {
                    frame.local_vars[i] = _args[i];
                }
                thread.push_frame(frame);
            };
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
        int arg_count = count_method_args(method_desc);
        arg_count++; // object ref
        std::vector<int32_t> args(arg_count);
        for (int i = arg_count - 1; i >= 0; --i) {
            args[i] = cur_frame.operand_stack.pop();
            fmt::print("setup args[{}]={}\n", i, args[i]);
        }
        if (args[0]==0) return;
        fmt::print("objref {} \n", args[0]);

        ClassInfo& target_class = interp.load_class(class_name);
        MethodInfo* target_method = interp.find_method(target_class, method_name, method_desc);
        if (target_method) {
            auto installFrame = [](const ClassInfo& _class, const MethodInfo& _method, const std::vector<int32_t>& _args) {
                Frame frame(_method.max_locals, _method.max_stack, _class, _method);
                for (int i=0; i<_args.size(); i++) {
                    frame.local_vars[i] = _args[i];
                }
                thread.push_frame(frame);
            };
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
        int arg_count = count_method_args(method_desc);
        std::vector<int32_t> args(arg_count);
        for (int i = arg_count - 1; i >= 0; --i) {
            args[i] = cur_frame.operand_stack.pop();
            fmt::print("setup args[{}]={}\n", i, args[i]);
        }

        ClassInfo& target_class = interp.load_class(class_name);
        MethodInfo* target_method = interp.find_method(target_class, method_name, method_desc);
        if (target_method) {
            auto installFrame = [](const ClassInfo& _class, const MethodInfo& _method, const std::vector<int32_t>& _args) {
                Frame frame(_method.max_locals, _method.max_stack, _class, _method);
                for (int i=0; i<_args.size(); i++) {
                    frame.local_vars[i] = _args[i];
                }
                thread.push_frame(frame);
            };
            installFrame(target_class, *target_method, args);
        } else {
            fmt::print("invokestatic invaid method");
            exit(1);
        }
    };
    // new
    opcode_table[0xbb] = [](Frame& cur_frame, size_t& pc, const std::vector<uint8_t>& code, const ClassInfo&, Interpreter& interp) {
        pc += 2; // 跳过常量池索引
        int obj_ref = interp.new_object();
        cur_frame.operand_stack.push(obj_ref);
        fmt::print("alloc new object {}\n", obj_ref);
    };
}

std::optional<int32_t> Interpreter::_execute(ClassInfo& entry_class, const MethodInfo& entry_method, const std::vector<int32_t>& entry_args) {
    if (opcode_table.empty()) init_opcode_table();
    auto installFrame = [](const ClassInfo& _class, const MethodInfo& _method, const std::vector<int32_t>& _args) {
        Frame frame(_method.max_locals, _method.max_stack, _class, _method);
        for (int i=0; i<_args.size(); i++) {
            frame.local_vars[i] = _args[i];
        }
        thread.push_frame(frame);
    };
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


