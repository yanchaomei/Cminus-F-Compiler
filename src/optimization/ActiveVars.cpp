#include "ActiveVars.hpp"
#include "ConstPropagation.hpp"

void ActiveVars::run()
{std::cout<<"hello"<<std::endl;
    std::ofstream output_active_vars;
    output_active_vars.open("active_vars.json", std::ios::out);
    output_active_vars << "[";
    int count = 0; 
    for (auto &func : this->m_->get_functions()) {
        if (func->get_basic_blocks().empty()) {
            continue;
        }
        else
        {
            count ++;
            func_ = func;  
            
            func_->set_instr_name();
            live_in.clear();
            live_out.clear();

            std::map<BasicBlock *, std::set<std::pair<Value*, Value*>>> useB, inTemp, outTemp;
            std::map<BasicBlock *, std::set< Value*>> defB;


            // 在此分析 func_ 的每个bb块的活跃变量，并存储在 live_in live_out 结构内
            for(auto bb : func->get_basic_blocks())
            {
                std::set<std::pair<Value*, Value*>> use;
                std::set<Value*> def;

                std::set<Value*> all_left_value;
                for(auto allbb: func->get_basic_blocks())
                {
                    for(auto allinst: allbb->get_instructions())
                    {
                        all_left_value.insert(allinst);
                    }
                    
                }

                for(auto param: func->get_args())
                {
                    all_left_value.insert(param);
                }

                for(auto instruction: bb->get_instructions())
                {
                    def.insert(instruction);
                    if(instruction->is_add() || instruction->is_sub() || instruction->is_mul() || instruction->is_div() || 
                    instruction-> is_fadd() || instruction->is_fsub() || instruction->is_fmul() || instruction->is_fdiv() ||
                    instruction -> is_cmp() || instruction -> is_fcmp())
                    {
                        if((!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
                            use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0), nullptr));
                        if((!cast_constantfp(instruction->get_operand(1))) && (!cast_constantint(instruction->get_operand(1))) && def.find(instruction->get_operand(1)) == def.end())
                            use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(1), nullptr));
                           
                    }
                    
                    if(instruction -> is_load())
                    {
                        if(def.find(instruction->get_operand(0)) == def.end())
                            use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0), nullptr));
                    }
        
                    if(instruction -> is_store())
                    {
                        if((!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
                            use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0), nullptr));
                        if((!cast_constantfp(instruction->get_operand(1))) && (!cast_constantint(instruction->get_operand(1))) && def.find(instruction->get_operand(1)) == def.end())
                            use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(1), nullptr));
                    }
                   
                    if(instruction -> is_phi())
                    {
                        if( (instruction->get_operand(0)!=nullptr)&& (!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
                            {
                                if(all_left_value.find( instruction->get_operand(0)) != all_left_value.end())
                                    use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0),instruction->get_operand(1)));
                                          
                            }
                        
                        if((instruction->get_operand(2)!=nullptr)&&(!cast_constantfp(instruction->get_operand(2))) && (!cast_constantint(instruction->get_operand(2))) && def.find(instruction->get_operand(2)) == def.end())
                            {
                                if(all_left_value.find( instruction->get_operand(2)) != all_left_value.end())
                                    use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(2),instruction->get_operand(3)));
                                      
                            }
                    }
                 
                    if(instruction -> is_call())
                    {
                        for(int i = 1; i < instruction->get_num_operand(); i++)
                        {
                            if((!cast_constantfp(instruction->get_operand(i))) && (!cast_constantint(instruction->get_operand(i))) && def.find(instruction->get_operand(i)) == def.end())
                                use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(i), nullptr));
                        }
                    }

                    if(instruction -> is_gep())
                    {
                        if(instruction->get_num_operand() == 2)
                        {
                            if((!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
                                use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0),nullptr));
                            if((!cast_constantfp(instruction->get_operand(1))) && (!cast_constantint(instruction->get_operand(1))) && def.find(instruction->get_operand(1)) == def.end())
                                use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0),nullptr));
                        }
                        else if(instruction->get_num_operand() == 3)
                        {
                            if((!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
                                use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0),nullptr));
                            if((!cast_constantfp(instruction->get_operand(2))) && (!cast_constantint(instruction->get_operand(2))) && def.find(instruction->get_operand(2)) == def.end())
                                use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(2),nullptr));
                        
                        }
                    }
                    if(instruction -> is_zext() || instruction->is_ret())
                    {
                        if((instruction->get_num_operand()>0) && (!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
                            use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0),nullptr));

                    }
                   
                    if(instruction -> is_br() && instruction -> get_num_operand() == 3)
                    {
                        if((!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
                            use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0),nullptr));
                    }
                    if(instruction -> is_fp2si() || instruction -> is_si2fp())
                    {
                         if((!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
                            use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0),nullptr));
                    }

                }
                useB.insert(std::make_pair(bb, use));
                defB.insert(std::make_pair(bb, def));
            }
            
            inTemp.clear();
            outTemp.clear();
            bool flag = false;
            
            while (!flag)
            {
                flag = true;
                for(auto bb : func->get_basic_blocks())
                {
                
                    for(auto succbb: bb->get_succ_basic_blocks())
                    {
                       
                        for(auto temp_in: inTemp[succbb])
                        {
                           
                            if(temp_in.second == nullptr)
                            {
                                
                                outTemp[bb].insert(temp_in);
                            }
                            else if(temp_in.second == bb)
                            {
                                outTemp[bb].insert(std::make_pair(temp_in.first, nullptr));
                            }
                            
                            
                        }
                        
                    }
                   
                    for(auto use_temp: useB[bb])
                    {
                        if(inTemp[bb].find(use_temp) == inTemp[bb].end())
                        {
                            flag = false;
                            
                            inTemp[bb].insert(use_temp);
                        }
                    }
                   
                    for(auto out_temp: outTemp[bb])
                    {
                        if(defB[bb].find(out_temp.first) == defB[bb].end())
                        {
                            if(inTemp[bb].find(out_temp) == inTemp[bb].end())
                            {
                                flag = false;
                                inTemp[bb].insert(out_temp);
                            }
                        }
                    }
                }
               
            }
            for(auto bb : func->get_basic_blocks())
                {
                    for (auto in_temp: inTemp[bb])
                    {
                        if(in_temp.first)
                        {
                                live_in[bb].insert(in_temp.first);    
                        }
                    }
                        
                            
                    for (auto out_temp: outTemp[bb])
                        if(out_temp.first)
                            live_out[bb].insert(out_temp.first);
                }

            output_active_vars << print();
            output_active_vars << ",";
            
        }
    }
    output_active_vars << "]";
    output_active_vars.close();
    return ;
}

std::string ActiveVars::print()
{
    std::string active_vars;
    active_vars +=  "{\n";
    active_vars +=  "\"function\": \"";
    active_vars +=  func_->get_name();
    active_vars +=  "\",\n";

    active_vars +=  "\"live_in\": {\n";
    for (auto &p : live_in) {
        if (p.second.size() == 0) {
            continue;
        } else {
            active_vars +=  "  \"";
            active_vars +=  p.first->get_name();
            active_vars +=  "\": [" ;
            for (auto &v : p.second) {
                active_vars +=  "\"%";
                active_vars +=  v->get_name();
                active_vars +=  "\",";
            }
            active_vars += "]" ;
            active_vars += ",\n";   
        }
    }
    active_vars += "\n";
    active_vars +=  "    },\n";
    
    active_vars +=  "\"live_out\": {\n";
    for (auto &p : live_out) {
        if (p.second.size() == 0) {
            continue;
        } else {
            active_vars +=  "  \"";
            active_vars +=  p.first->get_name();
            active_vars +=  "\": [" ;
            for (auto &v : p.second) {
                active_vars +=  "\"%";
                active_vars +=  v->get_name();
                active_vars +=  "\",";
            }
            active_vars += "]";
            active_vars += ",\n";
        }
    }
    active_vars += "\n";
    active_vars += "    }\n";

    active_vars += "}\n";
    active_vars += "\n";
    return active_vars;
}