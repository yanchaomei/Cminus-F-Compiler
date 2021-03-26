#include "cminusf_builder.hpp"
#include "logging.hpp"
// use these macros to get constant value
#define CONST_FP(num) \
    ConstantFP::get((float)num, module.get())
#define CONST_ZERO(type) \
    ConstantZero::get(var_type, module.get())
#define CONST_INT(num) \
    ConstantInt::get((int)num, module.get())

// You can define global variables here
// to store state

// to be used in while, if and return statements
Value* current_value;

int current_number;
float current_float;
int width;
int if_count, while_count, idx_count;
CminusType current_type;

Function* current_func;
BasicBlock* current_bb, *negBB; 

Value* return_alloca;

enum VarMode {STORE, LOAD} var_mode;

//Finish cast array
Value* CastRightValue(Type* left, Value* right, IRBuilder* builder, std::unique_ptr<Module> &module,int w = 32)
{
    auto left_type = left->get_type_id();
    auto right_type = (right->get_type())->get_type_id();
    //std::cout<<left_type<<right_type<<std::endl;
    Value* right_value;
    right_value = right;

    if(left_type < right_type)  //deal with int x = float y
        right_value = builder->create_fptosi(right, left);
    else if(left_type > right_type)// float x = int y
        right_value = builder->create_sitofp(right, left);
    return right_value;
}

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

void CminusfBuilder::visit(ASTProgram &node) {
    //LOG(INFO) << "Program";
    for(auto decl: node.declarations){
        decl->accept(*this);
    }
    scope.exit();
    //std::cout<<"testglobal"<<std::endl;
}

void CminusfBuilder::visit(ASTNum &node) { 
    //LOG(INFO) << "NUM";
    current_type = node.type;
    if(current_type == TYPE_INT){
        current_value = ConstantInt::get(node.i_val, module.get());
    }
    else if(current_type == TYPE_FLOAT)
        current_value = ConstantFP::get(node.f_val, module.get());
}

void CminusfBuilder::visit(ASTVarDeclaration &node) { 
    //LOG(INFO) << "VarDeclaration";
    Type* node_type;
    Type* node_value_type;

    if(node.type == TYPE_INT)
        node_value_type = Type::get_int32_type(module.get());
    else if(node.type == TYPE_FLOAT)
        node_value_type = Type::get_float_type(module.get()); 

    if(node.num != nullptr)
    {
        // when the var is a array
        node.num->accept(*this);
        node_type = ArrayType::get(node_value_type, node.num->i_val);
    }
    else
    {
        // the var is not a array 
        node_type = node_value_type;
    }

    if(scope.in_global())
    {
        GlobalVariable *global_var;
        global_var = GlobalVariable::create(node.id, module.get(), node_type, false, ConstantZero::get(node_type, module.get()));
        scope.push(node.id, global_var);
    }
    else
    {
        auto node_alloca = builder->create_alloca(node_type);
        scope.push(node.id, node_alloca);
    }
}

void CminusfBuilder::visit(ASTFunDeclaration &node) { 
    //LOG(INFO) << "FunDeclaration";
    std::vector<Type*> Params;
    for(auto param: node.params)
    {
        if(param->isarray)
        {
            //std::cout<<"type"<<param->type<<std::endl;
            if(param->type == CminusType::TYPE_INT)
                Params.push_back(PointerType::get_int32_ptr_type(module.get()));
            else
                Params.push_back(PointerType::get_float_ptr_type(module.get()));
        }
        else
        {
            if(param->type == CminusType::TYPE_INT)
                Params.push_back(Type::get_int32_type(module.get()));
            else if(param->type == CminusType::TYPE_FLOAT)
                Params.push_back(Type::get_float_type(module.get()));
            else
                Params.push_back(Type::get_void_type(module.get()));
        }
    }
    auto Int32Type = Type::get_int32_type(module.get());
    auto FloatType = Type::get_float_type(module.get());
    auto voidType = Type::get_void_type(module.get());
    FunctionType* functype;
    // choose the function type
    if(node.type == TYPE_INT){
        functype = FunctionType::get(Int32Type,Params);
    }else if(node.type == TYPE_FLOAT){
        functype = FunctionType::get(FloatType,Params);
    }else{
        functype = FunctionType::get(voidType,Params);
    }
    // get the function
    auto Func = Function::create(functype, node.id, module.get());
    // push the func's name
    scope.push(node.id, Func);

    current_func = Func;
    auto bb = BasicBlock::create(module.get(), "entry", Func);
    builder->set_insert_point(bb);

    current_bb = bb;

    // enter the scope of this function
    scope.enter();
    // allocate for params
    auto arg = Func->arg_begin();
    for(auto param: node.params)
    {
        if(arg == Func->arg_end())
        {
            // error
            std::cout<<"error"<<std::endl;
        }

        if(param->isarray){
            // is pointer
            if(param->type == TYPE_INT)
            {
                auto param_var = builder->create_alloca(PointerType::get(Int32Type));
                scope.push(param->id, param_var);
                builder->create_store(*arg, param_var);
            }
            if(param->type == TYPE_FLOAT)
            {
                auto param_var = builder->create_alloca(PointerType::get(FloatType));
                scope.push(param->id, param_var);
                builder->create_store(*arg, param_var);
            }
        }
        else
        {
            if(param->type == TYPE_INT)
            {
                auto param_var = builder->create_alloca(Int32Type);
                scope.push(param->id, param_var);
                builder->create_store(*arg, param_var);
            }
            if(param->type == TYPE_FLOAT){
                auto param_var = builder->create_alloca(FloatType);
                scope.push(param->id, param_var);
                builder->create_store(*arg, param_var);
            }
        }
        arg++;
    }

    // allocate return_alloca it's a global value
    if(node.type == TYPE_INT)
    {
        return_alloca = builder->create_alloca(Int32Type);
        //fun_return_type = return_alloca->get_type()->get_type_id();
    }
    else if(node.type == TYPE_FLOAT)
    {
        return_alloca = builder->create_alloca(FloatType);
        //fun_return_type = return_alloca->get_type()->get_type_id();
    }
    
    //auto backup_fun_type = fun_return_type;
    if_count = while_count = idx_count = 0;
    negBB = nullptr;
    // visit the compound_stmt and store the ret value in the return_alloca
    node.compound_stmt->accept(*this);
    //std::cout<<'1'<<std::endl;
    if(builder->get_insert_block()->get_terminator() == nullptr)
    {
        if(node.type == TYPE_VOID){
            builder->create_void_ret();
        }else{
            auto ret_load = builder->create_load(return_alloca);
            builder->create_ret(ret_load);
        }
    }
    
    // exit the scope of this function
    scope.exit();
    //fun_return_type = backup_fun_type;
}

void CminusfBuilder::visit(ASTParam &node) {
    //No Need put the code from funcdeclaration to here
 }

void CminusfBuilder::visit(ASTCompoundStmt &node) { 
    //LOG(INFO) << "Compoundstmt";
    // {}内的变量要有作用域
    scope.enter();
    
    for(auto decl: node.local_declarations){
        decl->accept(*this);
    }

    for(auto stmt: node.statement_list)
    {
        stmt->accept(*this);
        if(builder->get_insert_block()->get_terminator() != nullptr)
            break;
    }

    scope.exit();
}


void CminusfBuilder::visit(ASTExpressionStmt &node) {
    //LOG(INFO) << "ExpressionStmt";
    width = 0;
    if (node.expression != nullptr)
        node.expression->accept(*this);
    
 }

void CminusfBuilder::visit(ASTSelectionStmt &node) { 
    width = 1;
    if_count ++;
    //LOG(INFO) << "SelectionStmt";
    node.expression->accept(*this); // set global value current_value to transfer the value
    auto trueBB = BasicBlock::create(module.get(), "trueBB"+std::to_string(if_count), current_func);
    auto falseBB = BasicBlock::create(module.get(), "falseBB"+std::to_string(if_count), current_func);
    auto retBB = BasicBlock::create(module.get(), "retBB"+std::to_string(if_count), current_func);
    bool need_new_block = false;
    if(node.else_statement != nullptr)
        builder->create_cond_br(current_value, trueBB, falseBB);
    else
        builder->create_cond_br(current_value, trueBB, retBB);


    builder->set_insert_point(trueBB);
    node.if_statement->accept(*this);
     
    if(builder->get_insert_block()->get_terminator() == nullptr)
    {
        //std::cout <<"TEST"<<std::endl;
        builder->create_br(retBB);
    } 

    if (node.else_statement != nullptr)
    {
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if(builder->get_insert_block()->get_terminator() == nullptr)
            builder->create_br(retBB);
    }
    else
    {
        builder->set_insert_point(falseBB);
        builder->create_br(retBB);
    }
        builder->set_insert_point(retBB);

}


void CminusfBuilder::visit(ASTIterationStmt &node) 
{
    // the iteration-stmt
    while_count ++;
    //LOG(INFO) << "Iterationstmt";
    // create three bb: compare bb, while_body bb, end bb.
    auto cmp_bb = BasicBlock::create(module.get(), "cmp_bb"+std::to_string(while_count), current_func);
    auto while_body_bb = BasicBlock::create(module.get(), "while_body_bb" + std::to_string(while_count), current_func);
    auto end_bb = BasicBlock::create(module.get(), "end_bb" + std::to_string(while_count), current_func);

    // br to cmp_bb
    builder->create_br(cmp_bb);

    // cmp_bb
    builder->set_insert_point(cmp_bb);
    current_bb = cmp_bb;
    width = 1;
    // enter the expression node
    node.expression->accept(*this);
    // set the value of expression
    auto cmp = current_value;
    builder->create_cond_br(cmp, while_body_bb, end_bb);

    // while_body
    builder->set_insert_point(while_body_bb);
    current_bb = while_body_bb;
    // enter the statement node
    node.statement->accept(*this);
    // jump to the cmp bb, it's a loop
    if(builder -> get_insert_block()->get_terminator() == nullptr)
        builder->create_br(cmp_bb);
    //Finish consider the return-stmt inside the while bb Finish
    // end_bb
    builder->set_insert_point(end_bb);
    current_bb = end_bb;

 }

void CminusfBuilder::visit(ASTReturnStmt &node) {
    //LOG(INFO) << "returnstmt";
    
    if(node.expression){// return-stmt->return expression;
        width = 32;
        node.expression->accept(*this);
        // global value, seted in accept()
        auto ret_var = current_value;
        //std::cout <<"return"<<return_alloca->get_type()->get_type_id()<<std::endl;
        ret_var = CastRightValue(return_alloca->get_type()->get_pointer_element_type(), ret_var, builder, module);
        builder->create_ret(ret_var);
    }else{ // return-stmt->return;
        builder->create_void_ret();
    }
 }

void CminusfBuilder::visit(ASTVar &node) { 
    //LOG(INFO) << "Var";
    
    auto this_mode = var_mode;
    //std::cout<<"var_mode"<<var_mode<<std::endl;
    if (node.expression != nullptr) 
    {
        auto *x = scope.find(node.id); // find the al
        //std::cout << "[]" << std::endl;
        width = 32;
        node.expression->accept(*this);
        auto arrty = x->get_type();
        var_mode = LOAD;

        if(arrty->is_pointer_type() && 
           arrty->get_pointer_element_type()->is_array_type())
            {
                //std::cout<<current_value->get_type()->get_type_id();
                current_value = CastRightValue(Type::get_int32_type(module.get()), current_value, builder, module);
                
                if(negBB == nullptr)
                {
                    auto cur_insert = builder->get_insert_block();
                    negBB = BasicBlock::create(module.get(), "negBB", current_func);
                    builder->set_insert_point(negBB);
                    std::vector<Value*> arg;
                    builder->create_call(scope.find("neg_idx_except"),arg);
                    builder->create_void_ret();
                    builder -> set_insert_point(cur_insert);
                    
                }
                idx_count ++;
                auto afternegBB = BasicBlock::create(module.get(), "afternegBB"+std::to_string(idx_count),current_func);
                auto judge_neg_index_cmp = builder->create_icmp_lt(current_value, ConstantInt::get(0, module.get()));
                builder->create_cond_br(judge_neg_index_cmp, negBB, afternegBB);

                builder->set_insert_point(afternegBB);
                current_value = builder->create_gep(x, {ConstantInt::get(0, module.get()), current_value});
            } 
        else
        {
            auto index = current_value;
            auto Int32Type = Type::get_int32_type(module.get());
            auto FloatType = Type::get_float_type(module.get());
            
            
            current_value = builder->create_load(x);
            index = CastRightValue(Type::get_int32_type(module.get()), index, builder, module);

            if(negBB == nullptr)
                {
                    auto cur_insert = builder->get_insert_block();
                    negBB = BasicBlock::create(module.get(), "negBB", current_func);
                    builder->set_insert_point(negBB);
                    std::vector<Value*> arg;
                    builder->create_call(scope.find("neg_idx_except"),arg);
                    builder->create_void_ret();
                    builder -> set_insert_point(cur_insert);
                    
                }
            idx_count ++;
            auto afternegBB = BasicBlock::create(module.get(), "afternegBB"+std::to_string(idx_count),current_func);
            auto judge_neg_index_cmp = builder->create_icmp_lt(index, ConstantInt::get(0, module.get()));
            
            builder->create_cond_br(judge_neg_index_cmp, negBB, afternegBB);
            //std::cout<<current_value->get_type()->get_type_id()<<std::endl;
            builder->set_insert_point(afternegBB);
            
            current_value = builder->create_gep(current_value, {index});
            //std::cout<<current_value->get_type()->get_type_id()<<std::endl;
            //builder->create_store(current_value, current_value);
 
        }
    }
    else
    {
        current_value = scope.find(node.id); // find the alloca
        // not a array
        //current_value = builder->create_load(current_value);
        if(var_mode == LOAD && current_value->get_type()->is_pointer_type())
        {
            if(current_value->get_type()->get_pointer_element_type()->is_array_type())
            {
                current_value = builder->create_gep(current_value, {ConstantInt::get(0, module.get()),ConstantInt::get(0, module.get())});
                this_mode = STORE;
                //skip funa((int[])b) load
            }
        }
        
        //std::cout<<current_value->get_type()->get_type_id()<<std::endl;
    }
    if(this_mode == LOAD)
    {
        current_value = builder->create_load(current_value);
        //std::cout<<current_value->get_type()->get_type_id()<<std::endl;
    }
        
}

//Finish var cast
//1. int a = float b; Finish
//2. (int) return (float)a; Finish
//3. fun(int) but use with fun(float) Finish
//4. (int) a op (float) 
//    step1 a +-*/ b; Finish
//    step2 a cmp b; Finish
//5. (int) = (float)fun()

//Finish test a[4] = a[3]; Finish

//Finih test scope

//Finish add init var

void CminusfBuilder::visit(ASTAssignExpression &node) {
    //LOG(INFO) << "assignexpression";
    var_mode = STORE;
    auto this_width = width;
    // expression->var=expression | simple-expression
    // assign or call
    Value* left_alloca;
    Value* right_value;
    
    Value* temp;
    width = 32;
    node.var->accept(*this); // know the alloca from current_var
    
    //std::cout <<"rightvalue"<<std::endl;
    left_alloca = current_value; // find the address of the value
    
    node.expression->accept(*this);
    right_value = current_value;
    //std::cout <<"rightvalue"<<right_value->get_type()->get_type_id()<<std::endl;
    right_value = CastRightValue(left_alloca->get_type()->get_pointer_element_type(), right_value, builder, module, this_width);
    //std::cout <<"rightvalue"<<std::endl;
    builder->create_store(right_value, left_alloca); // store the value in the addression
    if(this_width == 1)
    {
        if(current_value->get_type()->is_integer_type())
            current_value = builder->create_icmp_ne(current_value, CONST_INT(0));
        if(current_value->get_type()->is_float_type())
            current_value = builder->create_fcmp_ne(current_value, CONST_FP(0));
    }
 }

//Finish:while  return
void CminusfBuilder::visit(ASTSimpleExpression &node) {
    auto this_width = width;
    width = 32;
    //LOG(INFO) << "simpleExpression";
    // simple-expression→additive-expression relop additive-expression 
    // ∣ additive-expression
    node.additive_expression_l->accept(*this);
    Value* left = current_value;

    if (node.additive_expression_r == nullptr) {
        //LOG(DEBUG) << "simpleExpression->additive-expression";
        if(this_width == 1)
        {
            if(current_value->get_type()->is_integer_type())
                current_value = builder->create_icmp_ne(current_value, CONST_INT(0));
            if(current_value->get_type()->is_float_type())
                current_value = builder->create_fcmp_ne(current_value, CONST_FP(0));
        }
    }
    else
    {
        node.additive_expression_r->accept(*this);
        Value* right = current_value;

        auto left_ty = (left->get_type()->get_type_id());
        auto right_ty = (right->get_type()->get_type_id());

        if(node.op == OP_LT || node.op == OP_LE || node.op == OP_GE || 
            node.op == OP_GT || node.op == OP_EQ || node.op == OP_NEQ)
        {
            if(left_ty > right_ty)
                right = CastRightValue(left->get_type(), right, builder, module);
            else if(left_ty < right_ty)
                left = CastRightValue(right->get_type(), left, builder, module);
        }

        if (node.op == OP_LT) 
        {
            if((left_ty != right_ty) || left_ty == 6)
                current_value = builder->create_fcmp_lt(left, right);
            else
                current_value = builder->create_icmp_lt(left, right);
        } 
        else if (node.op == OP_LE) 
        {
            if(left_ty != right_ty|| left_ty == 6)
                current_value = builder->create_fcmp_le(left, right);
            else
                current_value = builder->create_icmp_le(left, right);
        } 
        else if (node.op == OP_GE) 
        {
            if(left_ty != right_ty|| left_ty == 6)
                current_value = builder->create_fcmp_ge(left, right);
            else
                current_value = builder->create_icmp_ge(left, right);
        } 
        else if (node.op == OP_GT) 
        {
            if(left_ty != right_ty|| left_ty == 6)
                current_value = builder->create_fcmp_gt(left, right);
            else
                current_value = builder->create_icmp_gt(left, right);
        } 
        else if (node.op == OP_EQ) 
        {
            if(left_ty != right_ty|| left_ty == 6)
                current_value = builder->create_fcmp_eq(left, right);
            else
                current_value = builder->create_icmp_eq(left, right);
        } 
        else if (node.op == OP_NEQ)
        {
            if(left_ty != right_ty|| left_ty == 6)
                current_value = builder->create_fcmp_lt(left, right);
            else
                current_value = builder->create_icmp_lt(left, right);
        } 
        else 
        {
            std::abort();
        }
        if(this_width == 32)
        {
            if(current_value->get_type()->is_integer_type())
                current_value = builder->create_zext(current_value, Type::get_int32_type(module.get()));
            if(current_value->get_type()->is_float_type())
                current_value = builder->create_zext(current_value, Type::get_float_type(module.get()));
        }
        //std::cout << std::endl;
    }
       
 }

//Finish find some float examples!

void CminusfBuilder::visit(ASTAdditiveExpression &node) {
    //LOG(INFO) << "additiveexpression";
    // additive-expression→additive-expression addop term ∣ term
    if (node.additive_expression == nullptr) {
        node.term->accept(*this);    
    }
    else
    {
        node.additive_expression->accept(*this);
        Value* left = current_value;
        node.term->accept(*this); 
        Value* right = current_value;
        auto left_ty = (left->get_type()->get_type_id());
        auto right_ty = (right->get_type()->get_type_id());   
        if(node.op == OP_PLUS || node.op == OP_MINUS)
        {
            if(left_ty > right_ty)
                right = CastRightValue(left->get_type(), right, builder, module);
            else if(left_ty < right_ty)
                left = CastRightValue(right->get_type(), left, builder, module);
        }
        if (node.op == OP_PLUS) 
        {
            if(left_ty != right_ty|| left_ty == 6)
                current_value = builder->create_fadd(left, right);
            else
                current_value = builder->create_iadd(left, right);
        } 
        else if (node.op == OP_MINUS) 
        {
            if(left_ty != right_ty|| left_ty == 6)
                current_value = builder->create_fsub(left, right);
            else
                current_value = builder->create_isub(left, right);
        } 
        else {
            std::abort();
        }
    }
 }

void CminusfBuilder::visit(ASTTerm &node)
 {
    //LOG(INFO) << "term";
    // term→term mulop factor ∣ factor
    var_mode = LOAD;
    if (node.term == nullptr) {
        node.factor->accept(*this);
    } 
    else 
    {
        node.term->accept(*this);
        Value* left = current_value;
        node.factor->accept(*this);
        Value* right = current_value;
        auto left_ty = (left->get_type()->get_type_id());
        auto right_ty = (right->get_type()->get_type_id());   
        if(node.op == OP_MUL || node.op == OP_DIV)
        {
            if(left_ty > right_ty)
                right = CastRightValue(left->get_type(), right, builder, module);
            else if(left_ty < right_ty)
                left = CastRightValue(right->get_type(), left, builder, module);
        }
        if (node.op == OP_MUL) 
        {
            if(left_ty != right_ty|| left_ty == 6)
                current_value = builder->create_fmul(left, right);
            else
                current_value = builder->create_imul(left, right);
        } 
        else if (node.op == OP_DIV) 
        {
            if(left_ty != right_ty || left_ty == 6)
                current_value = builder->create_fdiv(left, right);
            else
                current_value = builder->create_isdiv(left, right);
        } 
        else {
            std::abort();
        }
    }

 }

void CminusfBuilder::visit(ASTCall &node) {
    //LOG(INFO) << "call";
    // call->ID ( args )
    auto func = scope.find(node.id);
    Type* t = func->get_type();
    auto func_type = static_cast<FunctionType *> (t);
    if(func == nullptr){
        ;
    }
    else
    {
        std::vector<Value*> args;
        std::vector<Type *>::iterator iter;
        if(func_type->get_num_of_args())
            iter = func_type->param_begin();
        //std::cout<<"kk"<<func_type->get_num_of_args()<<std::endl;
        
        for (auto arg: node.args) {
            arg->accept(*this);
            //std::cout<< (*iter)->get_pointer_element_type()->get_type_id() <<"type"<<std::endl;
            args.push_back(CastRightValue((*iter),current_value,builder,module));
            iter++;
        }
        current_value = builder->create_call(func, args);
    }
    
 }
