#ifndef INTERPRETER_H
#define INTERPRETER_H
#include "classFileParser_types.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <stack>
#include <map>
#include <string>
#include <optional>
#include <functional>
#include "runtime.h"
#include "ClassLoader.h"

class Interpreter {
public:
    ClassLoader class_loader; 
    Interpreter() {
        new_object();
        new_object();
    }
    // 执行指定方法
    std::optional<SlotT> execute(const std::string& class_name, const std::string& method_name, const std::string& method_desc, const std::vector<SlotT>& args);
    // 根据方法名和描述符查找方法
    MethodInfo* find_method(ClassInfo& cf, const std::string& name, const std::string& descriptor);
    // 分配新对象，返回对象引用（索引）
    int new_object();
    // 设置对象字段
    void put_field(int obj_ref, const std::string& field, SlotT value);
    // 获取对象字段
    SlotT get_field(int obj_ref, const std::string& field);

private:
    // 对象池，包含堆对象和static对象
    std::vector<JVMObject> object_pool;

    using OpcodeHandler = std::function<void(Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&)>;
    std::vector<OpcodeHandler> opcode_table;
    void init_opcode_table();

    void resolve_getstatic(const ClassInfo& cf, uint16_t index, Frame& frame);
    void execute_instruction(const std::vector<ConstantPoolInfo>& constant_pool, const std::vector<uint8_t>& code, size_t& pc, std::vector<SlotT>& stack, std::vector<SlotT>& locals);
    std::optional<SlotT> _execute(ClassInfo& cf, const MethodInfo& method, const std::vector<SlotT>& args);

    ClassInfo& load_class(const std::string& class_name) {
        return class_loader.load_class(class_name);
    }
};

#endif // INTERPRETER_H