#ifndef FUNCINLINE_HPP
#define FUNCINLINE_HPP

#include "PassManager.hpp"
#include "Instruction.h"
#include "Module.h"

#include "Value.h"
#include <vector>
#include <stack>
#include <set>
#include <map>

class FuncInline: public Pass
{
public:
    FuncInline(Module *m) : Pass(m) {}
    void run();
    void judge_recur(Function *entry);
    void inlining(Function *func);
    bool can_be_inlined(Instruction *instr);
    bool is_ignored(Function *func) { return ignore_func.find(func) != ignore_func.end(); }
    bool is_inlined(Function *func) { return inline_func.find(func) != inline_func.end(); }
    bool is_recursive(Function *func) { return recursive_func.find(func) != recursive_func.end(); }

private:
    std::list<BasicBlock *> copy_basicblocks(CallInst *call_inst,Function *old_func, Function *parent);
    void handle_phi_returns(BasicBlock *return_bb, PhiInst *ret_val, std::list<BasicBlock *> &func_bbs);
    void remove_inline_func();

private:
    std::set<Function *> inline_func;  
    std::set<Function *> has_inline;  
    std::set<Function *> ignore_func;
    std::set<Function *> recursive_func;
    std::map<Function *, std::set<Function *>> func_succ;
    std::set<Function *> visited;
};

#endif