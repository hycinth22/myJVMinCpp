#include "classFileParser_types.h"

// 解释器
class Interpreter {
    std::vector< std::vector<uint8_t> > object_pool;
public:
    void execute(const std::vector<ConstantPoolInfo>& constant_pool, const MethodInfo& method);
private:
    void resolve_ldc(const std::vector<ConstantPoolInfo>& constant_pool, uint8_t index, std::vector<int32_t>& stack);
    void resolve_invokevirtual(const std::vector<ConstantPoolInfo>& constant_pool, uint16_t index, std::vector<int32_t>& stack);
};