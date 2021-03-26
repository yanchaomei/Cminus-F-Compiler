#include "BasicBlock.h"
#include "Constant.h"
#include "Function.h"
#include "IRBuilder.h"
#include "Module.h"
#include "Type.h"

#include <iostream>
#include <memory>

#ifdef DEBUG  // 用于调试信息,大家可以在编译过程中通过" -DDEBUG"来开启这一选项
#define DEBUG_OUTPUT std::cout << __LINE__ << std::endl;  // 输出行号的简单示例
#else
#define DEBUG_OUTPUT
#endif

#define CONST_INT(num) \
    ConstantInt::get(num, module)

#define CONST_FP(num) \
    ConstantFP::get(num, module) // 得到常数值的表示,方便后面多次用到

int main(){

    auto module = new Module("while");
    auto builder = new IRBuilder(nullptr, module);
    Type *Int32Type = Type::get_int32_type(module);

    // main函数
    auto mainFun = Function::create(FunctionType::get(Int32Type, {}),
                                  "main", module);
    auto bb = BasicBlock::create(module, "entry", mainFun);
    builder->set_insert_point(bb);

    auto a = builder->create_alloca(Int32Type);
    auto i = builder->create_alloca(Int32Type);

    builder->create_store(CONST_INT(10), a);
    builder->create_store(CONST_INT(0), i);

    auto label1 = BasicBlock::create(module, "label1", mainFun);
    auto while_body = BasicBlock::create(module, "while_body", mainFun);
    auto end = BasicBlock::create(module, "end", mainFun);

    builder->create_br(label1);

    // block label1
    builder->set_insert_point(label1);
    auto a_value = builder->create_load(a);
    auto i_value = builder->create_load(i);
    auto icmp = builder->create_icmp_lt(i_value, CONST_INT(10));
    builder->create_cond_br(icmp, while_body, end);

    // block while_body
    builder->set_insert_point(while_body);
    auto i_new = builder->create_iadd(i_value, CONST_INT(1));
    auto a_new = builder->create_iadd(a_value, i_new);
    builder->create_store(i_new, i);
    builder->create_store(a_new, a);
    builder->create_br(label1);

    // block end
    builder->set_insert_point(end);
    builder->create_ret(a_value);

    std::cout << module->print();
    delete module;
    return 0;


}