#ifndef DEADELIMI_HPP
#define DEADELIMI_HPP
#include "PassManager.hpp"
#include "Instruction.h"
#include "Module.h"

#include "Value.h"
#include <vector>
#include <stack>
#include <unordered_set>

class DeadElimi: public Pass
{
public:
    DeadElimi(Module *m) : Pass(m) {}
    void run();
    bool is_effect(Instruction *inst);
    void get_non_effect_func();
    void mark(Instruction *inst);
private:
    std::unordered_set<Function *> non_effect_func_list;
    std::unordered_set<Instruction *> worklist;
    std::unordered_set<Instruction *> removelist;
};

#endif