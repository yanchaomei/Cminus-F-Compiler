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

    auto module = new Module("if");
    auto builder = new IRBuilder(nullptr, module);
    Type *Int32Type = Type::get_int32_type(module);

    // main函数
    auto mainFun = Function::create(FunctionType::get(Int32Type, {}),
                                  "main", module);
    auto bb = BasicBlock::create(module, "entry", mainFun);
    builder->set_insert_point(bb);

    Type* Floatty = Type::get_float_type(module);
    auto a = builder->create_alloca(Floatty);
    builder->create_store(CONST_FP(5.555), a);
    auto a_value = builder->create_load(a);
    auto fcmp = builder->create_fcmp_gt(a_value, CONST_FP(1.0));

    auto TrueBB = BasicBlock::create(module, "TrueBB", mainFun);
    auto FalseBB = BasicBlock::create(module, "FalseBB", mainFun);
    builder->create_cond_br(fcmp, TrueBB, FalseBB);

    // block TrueBB
    builder->set_insert_point(TrueBB);
    builder->create_ret(CONST_INT(233));

    // block FalseBB
    builder->set_insert_point(FalseBB);
    builder->create_ret(CONST_INT(0));

    std::cout << module->print();
    delete module;
    return 0;


}