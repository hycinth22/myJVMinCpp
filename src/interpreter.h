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
    Interpreter() {}
    // 执行指定方法
    std::optional<SlotT> execute(const std::string& class_name, const std::string& method_name, const std::string& method_desc, const std::vector<SlotT>& args);
    // 根据方法名和描述符查找方法
    MethodInfo* find_method(ClassInfo& cf, const std::string& name, const std::string& descriptor, std::string* found_in_which_parent_class = nullptr);
    // 分配新对象，返回对象引用（索引）
    RefT new_object(const std::string &class_name);
    // 根据对象引用获取对象索引
    JVMObject& get_object(RefT ref) {
        // 检查索引合法性
        if (ref < 0 || ref >= object_pool.size()) {
            throw std::out_of_range("Invalid object reference");
        }
        return object_pool[ref];
    }
    // 设置对象字段
    void put_field(RefT obj_ref, const std::string& field, SlotT value);
    // 获取对象字段
    SlotT get_field(RefT obj_ref, const std::string& field);
    // 浅克隆
    RefT shallow_clone_object(RefT objref) {
        const JVMObject& obj = get_object(objref);
        RefT new_obj_ref = new_object(obj.class_name);
        JVMObject& new_obj = get_object(new_obj_ref);
        new_obj.fields.reserve(obj.fields.size());
        for (const auto& [key, value]: obj.fields) {
            new_obj.fields[key] = value;
        }
        return new_obj_ref;
    }
private:
    // 对象池，包含堆对象和static对象
    std::vector<JVMObject> object_pool;

    using OpcodeHandler = std::function<void(JVMContext&, Frame&, size_t&, const std::vector<uint8_t>&, const ClassInfo&, Interpreter&)>;
    std::vector<OpcodeHandler> opcode_table;
    void init_opcode_table();

    void resolve_getstatic(const ClassInfo& cf, ConstIdxT index, Frame& frame);
    void execute_instruction(const std::vector<ConstantPoolInfo>& constant_pool, const std::vector<uint8_t>& code, size_t& pc, std::vector<SlotT>& stack, std::vector<SlotT>& locals);
    std::optional<SlotT> _execute(JVMContext& context, ClassInfo& cf, const MethodInfo& method, const std::vector<SlotT>& args);

    ClassInfo& load_class(const std::string& class_name) {
        return class_loader.load_class(class_name, [this](const std::string& loaded_class_name){
            // 执行已加载类的<clinit>方法初始化类的类变量和静态块。
            // 注意，class_loader除了加载class_name还会加载基类，因此loaded_class_name!=class_name
            std::vector<SlotT> args;
            this->execute(loaded_class_name, "<clinit>", "()V", args);
        });
    }
};

#endif // INTERPRETER_H