#include <algorithm>
#include "logging.hpp"
#include "LoopSearch.hpp"
#include "LoopInvHoist.hpp"

void LoopInvHoist::run()
{
    // 先通过LoopSearch获取循环的相关信息
    LoopSearch loop_searcher(m_, false);
    loop_searcher.run();

    // 接下来由你来补充啦！
    auto x=loop_searcher.begin();
    while(x != loop_searcher.end()){
        auto cur_loop = *x;
        bool is_inner_loop = true;
        for(auto bb:*cur_loop){
            if(loop_searcher.get_inner_loop(bb) != cur_loop){
                is_inner_loop = false;
            }
        }

        if(!is_inner_loop){ // when cur_loop has inner_loop just jump it
            x++;
            continue;
        }

        std::unordered_set<Value *> Can_not_be_moved;
        // from the inner_loop to parent_loop: find invariants and hoist them
        while(cur_loop != nullptr){
            Invs.clear();
            // find invariants
            bool will_iter = true;
            for(auto bb:*cur_loop){
                for(auto instr:bb->get_instructions()){
                    Can_not_be_moved.insert(instr);
                }
            }
            while(will_iter){
                will_iter = false;
                auto loop_set = *cur_loop;
                for(auto bb: loop_set){
                    auto instr_ptr = bb->get_instructions().begin();
                    auto e = bb->get_instructions().end();
                    for(; instr_ptr != e; instr_ptr++){
                        auto instr = *instr_ptr;
                        int cot = 0;
                        auto over = Can_not_be_moved.end();
                        if(Can_not_be_moved.find(instr) == over ||
                            instr->is_ret() || instr->is_call() || instr->is_br() || instr->is_phi() ||
                          instr->is_cmp() || instr->is_alloca() ) continue;

                        bool is_invariant = true;
                        for(auto operand : instr->get_operands()){
                            auto ins = Can_not_be_moved.find(operand);
                            if(ins != over){
                                is_invariant = false;
                            }
                        }

                        if(is_invariant){
                            will_iter = true;
                            cot = 1;
                            Can_not_be_moved.erase(instr);
                            Invs.push_back(std::make_pair(bb, instr_ptr));
                            
                        }
                    }
                }
            }
            
            // hoist the invatiants
            if(Invs.size()){
                auto base = loop_searcher.get_loop_base(cur_loop);
                auto pre_bases = base->get_pre_basic_blocks();
                BasicBlock * pre_base;
                for(auto Pre_B : pre_bases){
                    pre_base = Pre_B;
                    if(cur_loop->find(Pre_B) == cur_loop->end()){
                        break;
                    }
                }
                
                auto br = pre_base->get_terminator();
                pre_base->delete_instr(br);

                // for(auto x=Invs.end()-1;x>=Invs.begin();x--){
                //     pre_base->add_instr_begin(*(x->second)); // insert in prebb
                //     auto temp = x->first->get_instructions();
                //     temp.erase(x->second); // delete in oldbb
                // }

                for(auto x : Invs){
                    pre_base->add_instruction(*(x.second));
                    auto temp = x.first->get_instructions();
                    temp.erase(x.second); // delete in oldbb
                }

                pre_base->add_instruction(br);
            }

            cur_loop = loop_searcher.get_parent_loop(cur_loop);
        }

        x++;
    }
}