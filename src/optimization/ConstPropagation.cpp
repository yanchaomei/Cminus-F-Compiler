#include "ConstPropagation.hpp"
#include "logging.hpp"

// 给出了返回整形值的常数折叠实现，大家可以参考，在此基础上拓展
// 当然如果同学们有更好的方式，不强求使用下面这种方式
ConstantInt *ConstFolder::compute(
    Instruction::OpID op,
    ConstantInt *value1,
    ConstantInt *value2)
{

    int c_value1 = value1->get_value();
    int c_value2 = value2->get_value();
    switch (op)
    {
    case Instruction::add:
        return ConstantInt::get(c_value1 + c_value2, module_);

        break;
    case Instruction::sub:
        return ConstantInt::get(c_value1 - c_value2, module_);
        break;
    case Instruction::mul:
        return ConstantInt::get(c_value1 * c_value2, module_);
        break;
    case Instruction::sdiv:
        return ConstantInt::get((int)(c_value1 / c_value2), module_);
        break;
    default:
        return nullptr;
        break;
    }
}

// 整型的cmp指令的常数折叠
ConstantInt *ConstFolder::compute(
    CmpInst::CmpOp op,
    ConstantInt *value1,
    ConstantInt *value2
)
{
    int c_value1 = value1->get_value();
    int c_value2 = value2->get_value();
    switch (op)
    {
    case CmpInst::EQ:
        return ConstantInt::get(c_value1 == c_value2, module_);

        break;
    case CmpInst::NE:
        return ConstantInt::get(c_value1 != c_value2, module_);
        break;
    case CmpInst::GT:
        return ConstantInt::get(c_value1 > c_value2, module_);
        break;
    case CmpInst::GE:
        return ConstantInt::get((int)(c_value1 >= c_value2), module_);
        break;
    case CmpInst::LT:
        return ConstantInt::get((int)(c_value1 <= c_value2), module_);;
        break;
    case CmpInst::LE:
        return ConstantInt::get((int)(c_value1 < c_value2), module_);;
        break;
    default:
        return nullptr;
        break;
    }
}

// 浮点数计算的常数折叠
ConstantFP *ConstFolder::compute(
    Instruction::OpID op,
    ConstantFP *value1,
    ConstantFP *value2)
{
    float lhs = value1->get_value();
    float rhs = value2->get_value();
    switch (op)
    {
    case Instruction::fadd:
        return ConstantFP::get(lhs + rhs, module_);
        break;
    case Instruction::fsub:
        return ConstantFP::get(lhs - rhs, module_);
        break;
    case Instruction::fmul:
        return ConstantFP::get(lhs * rhs, module_);
        break;
    case Instruction::fdiv:
        return ConstantFP::get((float)(lhs / rhs), module_);
        break;
    default:
        return nullptr;
        break;
    }
}

// 浮点数比较的常数折叠
ConstantInt *ConstFolder::compute(
    CmpInst::CmpOp op,
    ConstantFP *value1,
    ConstantFP *value2)
{
    float c_value1 = value1->get_value();
    float c_value2 = value2->get_value();
    switch (op)
    {
    case CmpInst::EQ:
        return ConstantInt::get(c_value1 == c_value2, module_);

        break;
    case CmpInst::NE:
        return ConstantInt::get(c_value1 != c_value2, module_);
        break;
    case CmpInst::GT:
        return ConstantInt::get(c_value1 > c_value2, module_);
        break;
    case CmpInst::GE:
        return ConstantInt::get((int)(c_value1 >= c_value2), module_);
        break;
    case CmpInst::LT:
        return ConstantInt::get((int)(c_value1 <= c_value2), module_);;
        break;
    case CmpInst::LE:
        return ConstantInt::get((int)(c_value1 < c_value2), module_);;
        break;
    default:
        return nullptr;
        break;
    }
}

// 用来判断value是否为ConstantFP，如果不是则会返回nullptr
ConstantFP *cast_constantfp(Value *value)
{
    auto constant_fp_ptr = dynamic_cast<ConstantFP *>(value);
    if (constant_fp_ptr)
    {
        return constant_fp_ptr;
    }
    else
    {
        return nullptr;
    }
}
ConstantInt *cast_constantint(Value *value)
{
    auto constant_int_ptr = dynamic_cast<ConstantInt *>(value);
    if (constant_int_ptr)
    {
        return constant_int_ptr;
    }
    else
    {
        return nullptr;
    }
}


void ConstPropagation::run()
{
    constfold_ = new ConstFolder(m_);
    builder_ = new IRBuilder(nullptr,m_);
    auto func_list = m_->get_functions();
    for(auto func : func_list){
        for(auto bb : func->get_basic_blocks()){
            // mantain a vector to store the instr which need to be deleted
            std::vector<Instruction *> Delete_instructions;    
            globalMap.clear();
            for(auto instruction : bb->get_instructions()){

                if(instruction->is_add() || instruction->is_sub() || instruction->is_mul() || instruction->is_div() || 
                    instruction-> is_fadd() || instruction->is_fsub() || instruction->is_fmul() || instruction->is_fdiv() ){
                        // operands are const int
                        if(cast_constantint(instruction->get_operand(0)) && cast_constantint(instruction->get_operand(1))){
                            auto left = cast_constantint(instruction->get_operand(0));
                            auto right = cast_constantint(instruction->get_operand(1));
                            
                            auto constant = constfold_->compute(instruction->get_instr_type(),left, right);
                            for(auto ins : instruction->get_use_list()){ // const propagation 
                                dynamic_cast<User *>(ins.val_)->set_operand(ins.arg_no_, constant);
                                if(dynamic_cast<Instruction *>(ins.val_)->is_si2fp()){
                                    for(auto x : dynamic_cast<Instruction *>(ins.val_)->get_use_list()){
                                        auto temp = ConstantFP::get((float)(constant->get_value()),m_);
                                        dynamic_cast<User *>(x.val_)->set_operand(x.arg_no_, temp);
                                    }
                                    Delete_instructions.push_back(dynamic_cast<Instruction *>(ins.val_));
                                }
                            }
                            Delete_instructions.push_back(instruction);
                        }else if(cast_constantfp(instruction->get_operand(0)) && cast_constantfp(instruction->get_operand(1))){
                            // operands are const float
                            auto left = cast_constantfp(instruction->get_operand(0));
                            auto right = cast_constantfp(instruction->get_operand(1));
                            
                            auto constant = constfold_->compute(instruction->get_instr_type(),left, right); 
                            for(auto ins : instruction->get_use_list()){ // const propagation 
                                dynamic_cast<User *>(ins.val_)->set_operand(ins.arg_no_, constant);
                                if(dynamic_cast<Instruction *>(ins.val_)->is_fp2si()){
                                    for(auto x : dynamic_cast<Instruction *>(ins.val_)->get_use_list()){
                                        auto temp = ConstantInt::get((int)(constant->get_value()),m_);
                                        dynamic_cast<User *>(x.val_)->set_operand(x.arg_no_, temp);
                                    }
                                    Delete_instructions.push_back(dynamic_cast<Instruction *>(ins.val_));
                                }
                            }
                            Delete_instructions.push_back(instruction);
                        }
                    }

                if(instruction->is_cmp()){
                        if(cast_constantint(instruction->get_operand(0)) && cast_constantint(instruction->get_operand(1))){
                            auto left = cast_constantint(instruction->get_operand(0));
                            auto right = cast_constantint(instruction->get_operand(1));
                            
                            auto constant = constfold_->compute(dynamic_cast<CmpInst *>(instruction)->get_cmp_op(),left, right);
                            for(auto ins : instruction->get_use_list()){ // const propagation 
                                dynamic_cast<User *>(ins.val_)->set_operand(ins.arg_no_, constant);
                                if(dynamic_cast<Instruction *>(ins.val_)->is_zext()){
                                    for(auto x : dynamic_cast<Instruction *>(ins.val_)->get_use_list()){
                                        dynamic_cast<User *>(x.val_)->set_operand(x.arg_no_, constant);
                                    }
                                    Delete_instructions.push_back(dynamic_cast<Instruction *>(ins.val_));
                                }
                            }
                            Delete_instructions.push_back(instruction);
                        }
                }

                if(instruction->is_fcmp()){
                        if(cast_constantfp(instruction->get_operand(0)) && cast_constantfp(instruction->get_operand(1))){
                            auto left = cast_constantfp(instruction->get_operand(0));
                            auto right = cast_constantfp(instruction->get_operand(1));
                            
                            auto constant = constfold_->compute(dynamic_cast<CmpInst *>(instruction)->get_cmp_op(),left, right);
                            for(auto ins : instruction->get_use_list()){ // const propagation 
                                dynamic_cast<User *>(ins.val_)->set_operand(ins.arg_no_, constant);
                                if(dynamic_cast<Instruction *>(ins.val_)->is_zext()){
                                    for(auto x : dynamic_cast<Instruction *>(ins.val_)->get_use_list()){
                                        dynamic_cast<User *>(x.val_)->set_operand(x.arg_no_, constant);
                                    }
                                    Delete_instructions.push_back(dynamic_cast<Instruction *>(ins.val_));
                                }
                            }
                            Delete_instructions.push_back(instruction);
                        }
                }

                if(instruction->is_load() || instruction->is_store()){
                    if(instruction->is_load()){
                        bool flag = false;
                        Constant * const_value;
                        Value* value = instruction->get_operand(0); // get the operand
                        // if the operand from a instr(GEN Alloca etc...)
                        auto instrctionValue = dynamic_cast<Instruction *>(value);
                        if(instrctionValue){ //TODO when is array
                            ;
                        }
                        // if the operand is a global varible
                        auto globalValue = dynamic_cast<GlobalVariable *>(value);
                        if(globalValue){
                            if(globalMap.find(value) != globalMap.end()){
                                const_value = (globalMap.find(value))->second;
                                flag = true;
                            }
                        }

                        if(flag){
                            for(auto ins:instruction->get_use_list()){
                                dynamic_cast<User *>(ins.val_)->set_operand(ins.arg_no_, const_value);
                            }
                            Delete_instructions.push_back(instruction);
                        }
                    }else{
                        // instr is store
                        auto Lvalue = dynamic_cast<StoreInst *>(instruction)->get_lval();
                        auto Rvalue = dynamic_cast<StoreInst *>(instruction)->get_rval();
                        auto const_rvalue = dynamic_cast<Constant *> (Rvalue);
                        if(const_rvalue){
                            //if store a const to the value
                            auto globalValue = dynamic_cast<GlobalVariable *>(Lvalue);
                            if(globalValue){
                                if(globalMap.find(globalValue) != globalMap.end()){
                                    // not empty
                                    globalMap.find(globalValue)->second = const_rvalue;

                                }else{
                                    // map is empty
                                    // then insert it
                                    globalMap.insert({globalValue, const_rvalue});
                                }
                            }

                            auto instrctionValue = dynamic_cast<Instruction *>(Lvalue);
                            if(instrctionValue){
                                //TODO when is array
                                ;
                            }
                        }
                    }
                }

                if(instruction->is_si2fp()){
                    if(cast_constantint(instruction->get_operand(0))){
                        for(auto x : instruction->get_use_list()){
                            auto temp = ConstantFP::get((float)(cast_constantint(instruction->get_operand(0))->get_value()),m_);
                           dynamic_cast<User *>(x.val_)->set_operand(x.arg_no_, temp);
                        }
                        Delete_instructions.push_back(instruction);
                    }
                }
                if(instruction->is_fp2si()){
                    if(cast_constantfp(instruction->get_operand(0))){
                        for(auto x : instruction->get_use_list()){
                            auto temp = ConstantInt::get((int)(cast_constantfp(instruction->get_operand(0))->get_value()),m_);
                            dynamic_cast<User *>(x.val_)->set_operand(x.arg_no_, temp);
                        }
                        Delete_instructions.push_back(instruction);
                    }
                }

            }
            // delete all the ins whose value is a const
            for(auto delete_ins : Delete_instructions){
                bb->delete_instr(delete_ins);
            }
        }
    }

    for(auto func:func_list){
        for(auto bb : func->get_basic_blocks()){
            builder_->set_insert_point(bb);
            bool is_BR = false;
            if(bb->get_terminator()->is_br()) is_BR = true;
            if(is_BR){
                auto br = bb->get_terminator();
                bool is_con_BR = false;
                if(dynamic_cast<BranchInst *>(br)->is_cond_br()) is_con_BR = true;
                if(is_con_BR){
                    // the bool value's type is ConstantInt
                    // judge the condition is const or not
                    auto conditon = dynamic_cast<ConstantInt *>(br->get_operand(0));
                    bool is_const = false;
                    auto true_bb = br->get_operand(1);
                    auto false_bb = br->get_operand(2);
                    if (conditon) is_const = true;
                    if(is_const){
                        if(conditon->get_value()){

                            bb->delete_instr(br);
                            for(auto succ : bb->get_succ_basic_blocks()){
                                succ->remove_pre_basic_block(bb);
                                if(succ!=true_bb){
                                    for(auto ins:succ->get_instructions()){
                                        if(ins->is_phi()){
                                            for(int i=0; i<ins->get_num_operand(); i++){
                                                if(i%2 == 1){
                                                    if(ins->get_operand(i) == bb){
                                                        ins->remove_operands(i-1, i);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            auto trueBB = dynamic_cast<BasicBlock *>(true_bb);
                            builder_->create_br(trueBB);
                            bb->get_succ_basic_blocks().clear();
                            bb->add_succ_basic_block(trueBB);                            

                        }else{

                            bb->delete_instr(br);
                            for(auto succ : bb->get_succ_basic_blocks()){
                                succ->remove_pre_basic_block(bb);
                                if(succ!=false_bb){
                                    for(auto ins:succ->get_instructions()){
                                        if(ins->is_phi()){
                                            for(int i=0; i<ins->get_num_operand(); i++){
                                                if(i%2 == 1){
                                                    if(ins->get_operand(i) == bb){
                                                        ins->remove_operands(i-1, i);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            auto falseBB = dynamic_cast<BasicBlock *>(false_bb);
                            builder_->create_br(falseBB);
                            bb->get_succ_basic_blocks().clear();
                            bb->add_succ_basic_block(falseBB);

                        }
                    }
                }
            }
        }
    }
}
