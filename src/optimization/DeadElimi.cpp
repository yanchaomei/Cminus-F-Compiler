#include "DeadElimi.hpp"

void DeadElimi::run()
{
	int del_inst = 0;

	get_non_effect_func();
	
    for (auto &func : this->m_->get_functions())
    {
        worklist.clear();
        for (auto bb : func->get_basic_blocks()) 
        {
            for (auto inst : bb->get_instructions()) 
				if (is_effect(inst)) 
                	mark(inst);
        }

        for (auto bb : func->get_basic_blocks()) 
        {
			removelist.clear();
			for (auto inst : bb->get_instructions()) 
            	if (worklist.find(inst) == worklist.end()) 
        			removelist.insert(inst);
			for (auto inst : removelist) 
			{
				bb->delete_instr(inst);
      			del_inst++;
    		}
        }
    }
}

bool DeadElimi::is_effect(Instruction *inst)
{
	switch ((inst->get_instr_type()))
	{
	case Instruction::ret:
	case Instruction::br:
	case Instruction::store:
		return true;
		break;
	case Instruction::call:
	{
		
		auto call = static_cast<CallInst *>(inst);
		//std::cout << non_effect_func_list.size()<<"size"<<std::endl;
		//std::cout << call->print()<<std::endl;
		//std::cout << call->get_operand(0)->get_name()  <<" "<<(non_effect_func_list.find(call->get_function()) == non_effect_func_list.end())<<"call"<<std::endl;
		return non_effect_func_list.find(static_cast<Function*>(call->get_operand(0))) == non_effect_func_list.end();
    	break;
	}
	default:
		return false;
		break;
	}
}

void DeadElimi::get_non_effect_func()
{
	for(auto &func: this->m_->get_functions())
	{
		//std::cout << non_effect_func_list.size()<<"size"<<std::endl;
		//std::cout << func->get_name()<<"name"<<std::endl;
		if(func->get_num_basic_blocks() == 0)
			continue;
		bool flag = false;
		for(auto bb: func->get_basic_blocks())
		{
			for(auto inst: bb->get_instructions())
			{
				if(inst->is_store() || inst->is_call())
				{
					flag = true;
					break;
				}
				
			}
		}
		if(!flag)
			non_effect_func_list.insert(func);
	}
	//std::cout << non_effect_func_list.size()<<"size"<<std::endl;
}

void DeadElimi::mark(Instruction *inst)
{
	if(worklist.find(inst) != worklist.end())
		return;
	worklist.insert(inst);
	for(auto op: inst->get_operands())
	{
		auto op_inst =  dynamic_cast<Instruction *>(op);
		if(op_inst)
			mark(op_inst);
	}
}