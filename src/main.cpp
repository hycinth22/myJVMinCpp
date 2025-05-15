#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <cstdint>
#include <string>
#include <map>
#include <memory>
#include <fmt/core.h> 

// 常量池类型定义
struct ConstantPoolInfo {
    // 公共字段
    uint8_t tag;
    // tag=7: 类名常量
    uint16_t class_name_index;
    // tag=9: 字段引用 (Fieldref)
    uint16_t fieldref_class_index;
    uint16_t fieldref_name_type_index;
    // tag=10: 方法引用常量（tag=10）
    uint16_t methodref_class_index;
    uint16_t methodref_name_type_index;
    // tag=8: String
    uint16_t string_index;
    // tag=3/4: Integer、Float
    uint32_t integerOrFloat;
    // tag=5/6: Long/Double
    uint32_t longOrDouble_high_bytes;
    uint32_t longOrDouble_low_bytes;
    // tag=12: 名称与类型描述符 (NameAndType)
    uint16_t name_index;
    uint16_t descriptor_index;
    // tag=1: UTF-8字符串常量
    std::string utf8_str;
    // 其他类型可在此扩展
};

// 方法信息结构
struct MethodInfo {
    uint16_t access_flags;
    std::string name;
    std::string descriptor;
    std::vector<uint8_t> code;
    uint16_t max_stack;
    uint16_t max_locals;
};

struct ExceptionTable {
    uint16_t start_pc;
    uint16_t end_pc;
    uint16_t handler_pc;
    uint16_t catch_type; 
};

struct CodeAttribute;
struct AttributeInfo {
    AttributeInfo() : ty(TYPE::EMPTY) {}
    AttributeInfo(AttributeInfo&& rhs) {
        ty = rhs.ty;
        if (ty==TYPE::CODE) {
            code = std::move(rhs.code);
        } else if (ty==TYPE::UNKOWN) {
            unkown_info = std::move(rhs.unkown_info);
        }
    }
    // ~AttributeInfo() {
    //     if (ty==TYPE::CODE) {
    //         code.~unique_ptr();
    //     } else if (ty==TYPE::UNKOWN) {
    //         unkown_info.~vector<uint8_t>();
    //     }
    // }
    AttributeInfo& operator=(AttributeInfo&& rhs) {
        this->~AttributeInfo();
        ty = rhs.ty;
        if (ty==TYPE::CODE) {
            code = std::move(rhs.code);
        } else if (ty==TYPE::UNKOWN) {
            unkown_info = std::move(rhs.unkown_info);
        }
        return *this;
    }
    AttributeInfo(std::string name, std::unique_ptr<CodeAttribute> code) : ty(TYPE::CODE), name(name), code(move(code)) {}
    AttributeInfo(std::string name, std::vector<uint8_t> unkown_info)  : ty(TYPE::UNKOWN), name(name), unkown_info(unkown_info) {}
    std::string name;
    std::unique_ptr<CodeAttribute> code;
    std::vector<uint8_t> unkown_info;
private:
    enum class TYPE {EMPTY, CODE, UNKOWN};
    TYPE ty;
};

struct CodeAttribute {
    uint16_t max_stack;
    uint16_t max_locals;
    std::vector<uint8_t> code;
    std::vector<AttributeInfo> exception_table;
    std::vector<AttributeInfo> attributes;
};

// 类文件结构
class ClassFile {
public:
    std::vector<ConstantPoolInfo> constant_pool;
    std::vector<MethodInfo> methods;
    uint16_t majorVer, minorVer;
};

// 辅助函数：大端序读取
uint16_t read_u1(std::ifstream& in) {
    uint8_t bytes[1];
    in.read(reinterpret_cast<char*>(bytes), 1);
    return bytes[0];
}

uint16_t read_u2(std::ifstream& in) {
    uint8_t bytes[2];
    in.read(reinterpret_cast<char*>(bytes), 2);
    return (bytes[0] << 8) | bytes[1];
}

uint32_t read_u4(std::ifstream& in) {
    uint8_t bytes[4];
    in.read(reinterpret_cast<char*>(bytes), 4);
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

// Class文件解析器
class ClassFileParser {
public:
    ClassFile parse(const std::string& filename) {
        std::ifstream in(filename, std::ios::binary);
        ClassFile class_file;

        // 读取魔数
        uint32_t magic = read_u4(in);
        if (magic != 0xCAFEBABE) {
            throw std::runtime_error("Invalid class file magic number");
        }

        // 解析版本号
        class_file.minorVer = read_u2(in); // minor
        class_file.majorVer = read_u2(in); // major

        // 解析常量池
        uint16_t cp_count = read_u2(in);
        for (int i = 1; i < cp_count; ++i) { // 常量池索引从1开始
            uint8_t tag;
            in.read(reinterpret_cast<char*>(&tag), 1);
            // fmt::print("parsing tag type: {}\n", tag);
            ConstantPoolInfo cp_info;
            cp_info.tag = tag;
            switch (tag) {
                case 7: { // CONSTANT_Class
                    cp_info.class_name_index = read_u2(in);
                    break;
                }
                case 9: { // CONSTANT_Fieldref
                    cp_info.fieldref_class_index = read_u2(in);
                    cp_info.fieldref_name_type_index = read_u2(in);
                    break;
                }            
                case 10: { // CONSTANT_Methodref
                    cp_info.methodref_class_index = read_u2(in);
                    cp_info.methodref_name_type_index = read_u2(in);
                    break;
                }
                case 8: { // CONSTANT_String
                    cp_info.string_index = read_u2(in);
                    break;
                }
                case 3: // CONSTANT_Integer
                case 4: // CONSTANT_Float
                {
                    cp_info.integerOrFloat = read_u4(in);
                    break;
                }
                case 5: // CONSTANT_Long
                case 6: // CONSTANT_Double
                {
                    cp_info.longOrDouble_high_bytes = read_u4(in);
                    cp_info.longOrDouble_low_bytes = read_u4(in);
                    break;
                }
                case 12: { // CONSTANT_NameAndType
                    cp_info.name_index = read_u2(in);
                    cp_info.descriptor_index = read_u2(in);
                    break;
                }
                case 1: { // CONSTANT_Utf8
                    uint16_t len = read_u2(in);
                    cp_info.utf8_str.resize(len);
                    in.read(&cp_info.utf8_str[0], len);
                    break;
                }
                // case 15: {

                // }
                // case 16: {

                // }
                // case 18: {

                // }
                // 其他已知类型处理...
                default: {
                    // 处理未知tag
                    throw std::runtime_error("Unsupported constant pool tag: " + std::to_string(tag));
                }
            }
            class_file.constant_pool.push_back(cp_info);
        }

        // 解析访问标志、类名、父类名等
        uint16_t access_flags= read_u2(in);
        uint16_t this_class= read_u2(in);
        uint16_t super_class= read_u2(in);

        // 解析接口
        uint16_t interfaces_count = read_u2(in);
        in.ignore(interfaces_count * 2);

        // 解析字段
        uint16_t fields_count = read_u2(in);
        for (int i = 0; i < fields_count; ++i) {
            uint16_t field_access_flags= read_u2(in);
            uint16_t field_name_index = read_u2(in);
            uint16_t field_descriptor_index = read_u2(in);
            uint16_t field_attributes_count = read_u2(in);
            for (int i = 0; i < field_attributes_count; ++i) {
                uint16_t attribute_name_index= read_u2(in);
                uint16_t attribute_length = read_u4(in);
                in.ignore(attribute_length);
            }
        }

        // 解析方法
        uint16_t methods_count = read_u2(in);
        for (int i = 0; i < methods_count; ++i) {
            MethodInfo method;

            uint16_t access_flags = read_u2(in);
            method.access_flags = access_flags;
            uint16_t name_idx = read_u2(in);
            uint16_t desc_idx = read_u2(in);
            method.name = get_utf8_str(class_file.constant_pool, name_idx);
            method.descriptor = get_utf8_str(class_file.constant_pool, desc_idx);

            fmt::print("parsing method {}\n", method.name);
        
            // 解析属性
            bool code_found = false;
            uint16_t attr_count = read_u2(in);
            for (int j = 0; j < attr_count; ++j) {
                AttributeInfo attr = parseAttributeInfo(in, class_file.constant_pool);
                if (attr.name == "Code") {
                    method.code = std::move(attr.code->code);
                    method.max_stack = attr.code->max_stack;
                    method.max_locals = attr.code->max_locals;
                    code_found = true;
                }
            }
            if (!code_found) {
                throw std::runtime_error("Found a method without Code attribute");
            }
            class_file.methods.push_back(method);
        }

        return class_file;
    }
private:
    AttributeInfo parseAttributeInfo(std::ifstream& in, const std::vector<ConstantPoolInfo>& cp) {
        AttributeInfo attr;
        uint16_t attr_name_idx = read_u2(in);
        uint32_t attr_len = read_u4(in);
        attr.name = get_utf8_str(cp, attr_name_idx);
        if (attr.name == "Code") {
            std::unique_ptr<CodeAttribute> code_attr = std::make_unique<CodeAttribute>();
            code_attr->max_stack = read_u2(in);
            code_attr->max_locals = read_u2(in);
            uint32_t code_len = read_u4(in);
            code_attr->code.resize(code_len);
            in.read(reinterpret_cast<char*>(code_attr->code.data()), code_len);
            // 异常表
            uint16_t exception_table_len = read_u2(in);
            for (int k = 0; k < exception_table_len; ++k) {
                uint16_t start_pc = read_u2(in);
                uint16_t end_pc = read_u2(in);
                uint16_t handler_pc = read_u2(in);
                uint16_t catch_type = read_u2(in);
            }
            // 属性
            uint16_t subattributes_count = read_u2(in);
            for (int i = 0; i < subattributes_count; ++i) {
                AttributeInfo attr = parseAttributeInfo(in, cp);
            }
            attr = AttributeInfo(attr.name, std::move(code_attr));
        } else {
            std::vector<uint8_t> unkown_info(attr_len);
            in.read(reinterpret_cast<char*>(unkown_info.data()), attr_len);
            attr = AttributeInfo(attr.name, unkown_info);
        }
        return attr;
    }
    std::string get_utf8_str(const std::vector<ConstantPoolInfo>& cp, uint16_t index) const {
        if (index == 0 || index > cp.size()) return "";
        return cp[index - 1].utf8_str;
    }
};

// 解释器实现
class Interpreter {
    std::vector< std::vector<uint8_t> > object_pool;
public:
    void execute(const std::vector<ConstantPoolInfo>& constant_pool, const MethodInfo& method) {
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
    void resolve_ldc(const std::vector<ConstantPoolInfo>& constant_pool, uint8_t index, std::vector<int32_t>& stack) {
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
    void resolve_invokevirtual(const std::vector<ConstantPoolInfo>& constant_pool, uint16_t index, std::vector<int32_t>& stack) {
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
};

int main(int argc, char* argv[]) {
    std::string input_file = "Test.class";
    if (argc==2) {
        input_file = std::string(argv[1]);
    }
    try {
        fmt::print("ClassFileParser running on {}\n", input_file);
        ClassFileParser parser;
        ClassFile cf = parser.parse(input_file);
        fmt::print("Version: {}.{}\n", cf.majorVer, cf.minorVer);
        fmt::print("Method List:\n");
        for (const auto& method : cf.methods) {
            fmt::print("  Name: {}, Descriptor: {}, codesize:{}, max_stack:{}, max_locals:{}\n", method.name, method.descriptor, method.code.size(), method.max_stack, method.max_locals);

        }
        fmt::print("Interpreter running\n");
        for (const auto& method : cf.methods) {
            if (method.name == "main") {
                Interpreter interpreter;
                interpreter.execute(cf.constant_pool, method);
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}