#include "FuncInline.hpp"
#define MAX_INLINE_BB 4
#define MAX_INLINE_INST 5


void FuncInline::run()
{
    Function* main_func;

    for(auto func: this->m_->get_functions())
    {
        if(func->get_name() == "main") // Find main func
            main_func = func;

        if(func->get_num_basic_blocks() > MAX_INLINE_BB) // ignore large bb
            ignore_func.insert(func); 
        else
        {
            for(auto use: func->get_use_list()) //find parent
            {
                auto inst = static_cast<CallInst *>(use.val_);
                auto use_f = inst->get_parent()->get_parent();
                func_succ[use_f].insert(func);
            }
        }
    }
    
    judge_recur(main_func);
    inlining(main_func);
    //std::cout<<"/*****************************test!****************************/"<<std::endl;
    /*for(auto func: m_->get_functions())
    {
        if(!is_inlined(func) && func!= main_func)
            inlining(func);
    }*/
    remove_inline_func();
    //std::cout<<"/*****************************test!****************************/"<<std::endl;
    
}

void FuncInline::judge_recur(Function *entry)
{
    visited.insert(entry);
    for(auto succ: func_succ[entry])
    {
        if(visited.find(succ) != visited.end())
            recursive_func.insert(succ);
        else
            judge_recur(succ);
    }
    visited.erase(entry);
}

void FuncInline::inlining(Function *func)
{
    //std::cout<<"Before inline"<<func->get_num_basic_blocks()<<std::endl;
    auto &bbs = func->get_basic_blocks();
    for(auto iter = bbs.begin(); iter != bbs.end(); iter ++)
    {
        auto bb = *iter;
        
        auto &insts = bb->get_instructions();
        for(auto inst_iter = insts.begin(); inst_iter != insts.end(); inst_iter++)
        {
            auto inst = *inst_iter;
            //std::cout<<inst->print()<<"inst"<<std::endl;
            if(can_be_inlined(inst))
            {
                //std::cout<<inst->print()<<"can_be_inst"<<std::endl;
                
                /**************split inst after call**************/
                BasicBlock *new_bb = BasicBlock::create(m_, "", func);
                auto new_iter = inst_iter;
                
                for(new_iter++; new_iter != insts.end(); new_iter++)
                {
                    new_bb->add_instruction(*new_iter);
                    (*new_iter)->set_parent(new_bb);
                    for(auto use: ((*new_iter)->get_use_list()))
                    {
                        auto phi = dynamic_cast<Instruction *>(use.val_);
                        if(phi && phi->is_phi())
                        {
                            auto phi_bb = dynamic_cast<BasicBlock*>(phi->get_operand(use.arg_no_ + 1));
                            if(phi_bb == (*inst_iter)->get_parent())
                            {
                                phi_bb->remove_use(phi);
                                phi->set_operand(use.arg_no_+1, new_bb);
                            }
                        }
                    }
                    
                }
                insts.erase(inst_iter, insts.end());
                /*******************end split******************************/
                iter++;
                auto next_bb = iter;
                //auto next_bb = bbs.insert(iter, new_bb);
                //std::cout<<"end spilt"<<func->get_num_basic_blocks()<<std::endl;
                
                /*********************fix pre and succ blocks ********************************/
                for(auto succ_bb: bb->get_succ_basic_blocks())
                {
                    succ_bb->remove_pre_basic_block(bb);
                    succ_bb->add_pre_basic_block(new_bb);
                    new_bb->add_succ_basic_block(succ_bb);
                }
                
                bb->get_succ_basic_blocks().clear();
                /*********************end fix*********************************/          
    
                CallInst* call_inst = static_cast<CallInst*>(inst);
                auto func_old = static_cast<Function *>(call_inst->get_operand(0));
                //std::cout<<"Before copy"<<func->get_num_basic_blocks()<<std::endl;
                std::list<BasicBlock *> inlined_bbs = copy_basicblocks(call_inst, func_old, func);
                //std::cout<<"/*****************************test!****************************/"<<std::endl;
                //std::cout<<"After copy"<<inlined_bbs.front()->print()<<std::endl;
                for (auto bb : inlined_bbs) 
                {
                    bb->set_parent(func);
                    //std::cout<<bb->print()<<"bbp"<<std::endl;
                    //for(auto in: bb->get_instructions())
                        //if(static_cast<BinaryInst *> (in))
                            //std::cout<<"instop"<<in->get_name()<<std::endl;
                    //std::cout<<"bb"<<bb->get_num_of_instr()<<std::endl;
                }
                    
        
                //std::cout<<"/*****************************test!****************************/"<<std::endl;
                next_bb = bbs.insert(next_bb, inlined_bbs.begin(),inlined_bbs.end());

                BranchInst *br = BranchInst::create_br(*(inlined_bbs.begin()), bb);
                
//std::cout<<"/*****************************test!****************************/"<<std::endl;
                /************************alloca return value***************************/
                if(!call_inst->is_void())
                {
                    //std::cout<<"/*****************************test!****************************/"<<std::endl;
                    auto phi = PhiInst::create_phi(call_inst->get_type(), nullptr);
                     
                    handle_phi_returns(new_bb, phi, inlined_bbs);
                    //std::cout<<"/*****************************test!****************************/"<<std::endl;
                    if (phi->get_num_operand() == 2)
                        call_inst->replace_all_use_with(phi->get_operand(0));
                    else 
                    {
                    
                        call_inst->replace_all_use_with(phi);
                        phi->set_parent(new_bb);
                        new_bb->add_instr_begin(phi);
                    }
                }
                else
                {
                    for (auto bb_in : inlined_bbs) {
                        Instruction *instr = bb_in->get_terminator();
                        if (instr->get_instr_type() == Instruction::ret) 
                        {
                            auto br = BranchInst::create_br(new_bb, bb_in);
                            bb_in->delete_instr(instr);
                        }
                    }
                }
                //std::cout<<"/*****************************test!****************************/"<<std::endl;
                inline_func.insert(func_old);
                has_inline.insert(func);
                iter = --next_bb;
                break;
            }
        }
    }
}

bool FuncInline::can_be_inlined(Instruction *instr)
{
    if(instr->get_instr_type() == Instruction::call)
    {
        auto call = static_cast<CallInst *>(instr);
        auto func = static_cast<Function *>(call->get_operand(0));
        //std::cout<<func->get_num_basic_blocks()<<"num blo"<<std::endl;
        if(func->get_num_basic_blocks() <= MAX_INLINE_BB && func->get_num_basic_blocks() > 0)
        {
            for(auto bb: func->get_basic_blocks())
            {
                if(bb->get_num_of_instr() > MAX_INLINE_INST)
                    return false;
            }
            return !is_ignored(func) && !is_recursive(func);
        }
            
    }
    return false;
}

std::list<BasicBlock *> FuncInline::copy_basicblocks(CallInst *call_inst,Function *old_func, Function* parent)
{
    std::list<BasicBlock *> bbs;
    //std::cout<<old_func->get_name()<<std::endl;
    std::map<Value *, Value *> old2new;
    for (auto bb : old_func->get_basic_blocks()) 
    {
        //std::cout<<old_func->get_num_basic_blocks()<<"num"<<std::endl;
        //std::cout << bb->get_name()<<std::endl;
        //std::cout<<bb->print()<<std::endl;
        BasicBlock *new_bb = BasicBlock::create(m_, "", nullptr);
        //BasicBlock *new_bb = new BasicBlock()
        old2new[bb] = new_bb;
        bbs.push_back(new_bb);
        for (auto instr : bb->get_instructions()) 
        {
            Instruction *new_inst = instr->copy(new_bb);
            //new_inst->set_name(instr->get_name());
            //std::cout<<new_inst->get_name()<<"new i inst"<<std::endl;
            //Instruction *new_inst = new Instruction(instr->get_type(), instr->get_instr_type(), instr->get_num_operand(), new_bb);
            old2new[instr] = new_inst;
        }
    }
    int i = 1;
    for (auto arg : old_func->get_args()) 
    {
        //std::cout<< old2new[arg]
        old2new[arg] = call_inst->get_operand(i);
        //std::cout<<old2new[arg]->get_type()->get_type_id() <<"arg"<<std::endl;
        i++;
    }
    for (auto bb : old_func->get_basic_blocks()) 
    {
        BasicBlock *new_bb = dynamic_cast<BasicBlock *>(old2new[bb]);
        for (auto instr : bb->get_instructions()) 
        {
            Instruction *new_inst = dynamic_cast<Instruction *>(old2new[instr]);
            //std::cout<<instr->get_instr_op_name()<<std::endl;
            int i = 0;
            for (auto op : instr->get_operands()) 
            {
                Value *new_op;
                //std::cout<<op->get_name()<<std::endl;
                if (dynamic_cast<Constant *>(op) || dynamic_cast<GlobalVariable *>(op) || dynamic_cast<Function *>(op)) 
                {
                    new_op = op;
                } 
                else 
                {
                    //std::cout<<i<<old2new[op]->get_name() <<"new i inst"<<std::endl;
                
                    new_op = old2new[op];
                }
                
                new_inst->set_operand(i, new_op);
                //std::cout<<i<<new_inst->print() <<"new inst"<<std::endl;
                i++;
            }
            //std::cout<<instr->get_name() <<"new inst"<<std::endl;
        }
        //std::cout<<new_bb->print()<<"newbb"<<std::endl;
    }
    //std::cout<<"/*****************************test!****************************/"<<std::endl;
    for (auto bb : old_func->get_basic_blocks()) 
    {
        BasicBlock *new_bb = dynamic_cast<BasicBlock *>(old2new[bb]);
       //std::cout<<"/*****************************test!****************************/"<<std::endl;
        for (auto succ : bb->get_succ_basic_blocks()) 
        {
            BasicBlock *new_succ = dynamic_cast<BasicBlock *>(old2new[succ]);
            
            new_bb->add_succ_basic_block(new_succ);
            new_succ->add_pre_basic_block(new_bb);
        }
    }
    //std::cout<<"/*****************************test!****************************/"<<std::endl;
    return bbs;

}

void FuncInline::handle_phi_returns(BasicBlock *return_bb, PhiInst *ret_phi, std::list<BasicBlock *> &func_bbs)
{
    for (auto bb : func_bbs) 
    {
        Instruction *instr = bb->get_terminator();
    
        if (instr->get_instr_type() == Instruction::ret) 
        {
        
        ret_phi->add_operand(instr->get_operand(0));
        ret_phi->add_operand(bb);
        auto br = BranchInst::create_br(return_bb, bb);
        bb->delete_instr(instr);
        
        }
    }
}

void FuncInline::remove_inline_func()
{
    Function* main_func;

    for(auto func: this->m_->get_functions())
    {
        if(func->get_name() == "main") // Find main func
            main_func = func;
    }
 
    for (auto f : inline_func) 
    {
        bool remove = true;
        for (auto use : f->get_use_list()) 
        {
            auto instr = static_cast<CallInst *>(use.val_);
            auto use_f = instr->get_parent()->get_parent();
            if ((has_inline.find(use_f) == has_inline.end()) && (!is_inlined(use_f))) 
            {
                remove = false;
            }
        }
        if (remove) 
        {
            m_->remove_function(f);
            for (auto bb : f->get_basic_blocks()) 
            {
                for (auto inst : bb->get_instructions()) 
                {
                    inst->remove_use_of_ops();
                }
            }
        }
    }
}