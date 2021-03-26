# Lab5 实验报告

闫超美 PB18061351

张永停 PB17111585

## 实验要求

开发基本pass：

1. 常量传播
2. 循环不变式外提
3. 活跃变量分析

## 实验难点

### 常量传播

1. 全局变量的处理，在处理全局变量时，一开始没有找到方法去处理，后来想了想，只有在遇到`load`语句时有可能是从全局变量取值，而一个全局变量在使用前会给它赋值，所以在load这个全局变量之前会先在之前`store`一个值进去，所以我们就可以通过维护一个`map`来在store的时候将全局变量与常值映射，然后在load的时候可以从这个map里把值查出来，然后把用到此变量的指令的操作数都替代为这个常值。
2. 判断是否是全局变量，一开始想找API，但是似乎没有，然后想到可以用`dynamic_cast<GlobalVariable *>`来判断是否是全局变量。
3. 遇到类型转换的处理，比如遇到fp2si指令和si2fp指令，需要手动做类型转换，并做常量传播，然后把这条指令给删除；一开始我忘记做这样的处理，导致优化效果没有那么理想。
4. 遇到零扩展指令`zext`的处理，这里是一个比较坑的点，因为在从i1到i32的扩展中，就有可能由于做扩展的变量是一个常量，从而被之前的常量传播所替代了，导致这条语句运行出错。如果不做这个处理的话是通不过测试的。由于一般zext是和cmp指令配套出现的，所以我也就在做cmp指令的常量传播时做了个判断，如果是zext指令，就要把zext也做一个常量传播，然后再将zext指令给删除掉。



### 循环不变式外提

1. 循环不变式外提后插入的位置要在br前，我之前尝试先把br删掉，然后插入后再生成br，可是这样会导致segmentation fault而且一直解决不了，索性换了个方法，直接用add_instr_begin 这个API将指令倒序插入到bb的开头，这样又发现是有大问题的，因为在提出循环不变式到的bb中如果对循环不变式用到的变量有做计算，这样提到bb开头计算结果就会不正确，所以我最后还是改回去了，这次查到了一个api：get_terminator()，而不是像之前那样用循环找，这样先删除br，再插入，再添加br，就不会出现错误了。
2. 保存循环不变式的容器选择，一开始是用的vector存pair，每个pair是bb*和ins\*对应，然后发现这样在之后外提的时候想要从bb*对应的instr列表里删除这个instr会比较困难，因为有可能会有重合的语句，从而我改成了每个pair时bb和指向ins*的迭代器相对应，这样一来就可以直接通过迭代器去删除对应的语句。



### 活跃变量分析

1. 活跃变量的分析主要复杂的地方在于在于数据流方程的改动， 由于phi函数的加入， 原数据流方程$OUT[B] =\cup_{s是B的后继}IN[S]$  需要根据控制流方向来进行相应的改变。这里选择在记录`use`时对于phi指令，记录其第二、四参数（也即控制流）
2. 一个较为复杂的地方是根据指令来得到`use, def`。由于使用SSA格式，从而我们可以同时研究use, def， 对一条指令，只需要先将左值存入`def`， 就可以直接判断右值是否定过值。在分析`use`时， 需要注意很多指令细节。比如`gep`的参数可为2也可为3, `phi`的参数个数也是两种，同时需要对参数进行判断，如果是常数，则不加入。
3. 另一需要注意的地方在于， `phi i32 [ %op3, %label10 ], [ undef, %label_entry ]`的第三个参数并不一定是`nullptr`， 因此当不是`nullptr`时，需要对所有左值(包括函数的参数)进行检查以判断phi的第三个参数时候存在

## 实验设计

* 常量传播
  
    
  
    实现思路：
    
    ​	常量折叠部分的扩展，这部分扩展了四个compute重载，分别对应int值的两个数计算和两个数比较，float的两个数计算和两个数比较。
    
    ​	实现的主要处理分为两个部分，一是要对所有func的bb的所有语句进行处理，判断这个语句的类型是算术指令、load store指令、cmp指令、fcmp指令、si2fp、fp2si指令等等。然后做不同的处理，并做常量传播；第二部分是将一些冗余的分支进行删除。
    
    ​	第一部分在遇到算术指令时，有两个操作数，然后进行判断是否都为常量，如果是就进行常量折叠，然后再get_use_list()，并将用到这个变量的语句的操作数都替换为折叠后的值；遇到cmp指令和fcmp指令时，处理思路类似，也是进行折叠后传播；遇到load或store指令时，如果是store指令，要看看是否是store一个常量到变量中去，而且判断是否是全局变量，如果是全局变量就将全局变量和值的映射插入到globalMap中去。然后是数组的情况我就没有进行处理了。遇到load指令，这时候就要判断是不是全局变量，如果是，就从map里取出常量进行传播，并删除掉load指令。因为load指令一般只在有数组或者是全局变量的时候使用，而数组我们不考虑，所以就只在判断是全局变量时进行处理。由于load的值一定是最近store进去的值，而store的处理要比load早，所以这时map里的值已经更新过了，此时就能进行正确的常量传播。对于si2fp和fp2si指令，都是手动做一个类型转换，并将使用到变量的都进行常量传播，最后删掉类型转换的指令。
    
    ​	第二部分在处理时比较简单，对每个bb，如果最后一个br是con_br，然后判断con是否是常数，如果是，就根据这个值是1还是0来修改这个br指令为br trueBB或者 br FalseBB，也就是删除掉无用的分支。
    
    
    
    相应代码：
    
    第一部分的处理cmp的代码：
    
    ```c++
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
    ```
    
    第一部分处理load&store的代码：
    
    ```c++
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
    ```
    
    第二部分的部分代码：
    
    ```cpp
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
    
    ```
    
    
    
    
    
    优化前后的IR对比（举一个例子）并辅以简单说明：
    
    testcase-4 
    
    ```c
    int a;
    void main(void){
        int i;
    
        a=(1<2)+2;
        i=0;
        while(i<100000000)
        {
            if(a+(3.0*1.23456*5.73478*2.3333*4.3673/6.34636)/3-2*1.23456*5.73478*2.3333*4.3673*6.34636)
            {
                a=a+1;
            }
            i=i+1;
        }
        output(a*2+5);
        return ;
    }
    ```
    
    
    
    优化前：
    
    ```cpp
    ; ModuleID = 'cminus'
    source_filename = "../tests/lab5/testcases/ConstPropagation/testcase-4.cminus"
    
    @a = global i32 zeroinitializer
    declare i32 @input()
    
    declare void @output(i32)
    
    declare void @outputFloat(float)
    
    declare void @neg_idx_except()
    
    define void @main() {
    label_entry:
      %op1 = icmp slt i32 1, 2
      %op2 = zext i1 %op1 to i32
      %op3 = add i32 %op2, 2
      store i32 %op3, i32* @a
      br label %label4
    label4:                                                ; preds = %label_entry, %label35
      %op38 = phi i32 [ 0, %label_entry ], [ %op37, %label35 ]
      %op6 = icmp slt i32 %op38, 100000000
      %op7 = zext i1 %op6 to i32
      %op8 = icmp ne i32 %op7, 0
      br i1 %op8, label %label9, label %label28
    label9:                                                ; preds = %label4
      %op10 = load i32, i32* @a
      %op11 = fmul float 0x4008000000000000, 0x3ff3c0c200000000
      %op12 = fmul float %op11, 0x4016f06a20000000
      %op13 = fmul float %op12, 0x4002aa9940000000
      %op14 = fmul float %op13, 0x4011781d80000000
      %op15 = fdiv float %op14, 0x401962ac40000000
      %op16 = sitofp i32 3 to float
      %op17 = fdiv float %op15, %op16
      %op18 = sitofp i32 %op10 to float
      %op19 = fadd float %op18, %op17
      %op20 = sitofp i32 2 to float
      %op21 = fmul float %op20, 0x3ff3c0c200000000
      %op22 = fmul float %op21, 0x4016f06a20000000
      %op23 = fmul float %op22, 0x4002aa9940000000
      %op24 = fmul float %op23, 0x4011781d80000000
      %op25 = fmul float %op24, 0x401962ac40000000
      %op26 = fsub float %op19, %op25
      %op27 = fcmp une float %op26,0x0
      br i1 %op27, label %label32, label %label35
    label28:                                                ; preds = %label4
      %op29 = load i32, i32* @a
      %op30 = mul i32 %op29, 2
      %op31 = add i32 %op30, 5
      call void @output(i32 %op31)
      ret void
    label32:                                                ; preds = %label9
      %op33 = load i32, i32* @a
      %op34 = add i32 %op33, 1
      store i32 %op34, i32* @a
      br label %label35
    label35:                                                ; preds = %label9, %label32
      %op37 = add i32 %op38, 1
      br label %label4
    }
    ```
    
    优化后：
    
    ```cpp
    @a = global i32 zeroinitializer
    declare i32 @input()
    
    declare void @output(i32)
    
    declare void @outputFloat(float)
    
    declare void @neg_idx_except()
    
    define void @main() {
    label_entry:
      store i32 3, i32* @a
      br label %label4
    label4:                                                ; preds = %label_entry, %label35
      %op38 = phi i32 [ 0, %label_entry ], [ %op37, %label35 ]
      %op6 = icmp slt i32 %op38, 100000000
      %op7 = zext i1 %op6 to i32
      %op8 = icmp ne i32 %op7, 0
      br i1 %op8, label %label9, label %label28
    label9:                                                ; preds = %label4
      %op10 = load i32, i32* @a
      %op18 = sitofp i32 %op10 to float
      %op19 = fadd float %op18, 0x4026bc77a0000000
      %op26 = fsub float %op19, 0x408c9dd680000000
      %op27 = fcmp une float %op26,0x0
      br i1 %op27, label %label32, label %label35
    label28:                                                ; preds = %label4
      %op29 = load i32, i32* @a
      %op30 = mul i32 %op29, 2
      %op31 = add i32 %op30, 5
      call void @output(i32 %op31)
      ret void
    label32:                                                ; preds = %label9
      %op33 = load i32, i32* @a
      %op34 = add i32 %op33, 1
      store i32 %op34, i32* @a
      br label %label35
    label35:                                                ; preds = %label9, %label32
      %op37 = add i32 %op38, 1
      br label %label4
    }
    ```
    
    可以看到，这里把一开始对a的计算在编译期进行处理，直接将值存储到a中，然后是label9里的处理，这里一开始需要进行很多条乘法加法，优化后将这些常量进行折叠和传播，直接把值给计算了出来。进行了很大的优化。




* 循环不变式外提
    实现思路：

    ​	循环不变式外提的思路也是分为两个部分，一是找到循环不变式，二是将循环不变式外提。

    ​	首先是要在遍历所有loop时判断当前是否是inner_loop，因为我们提循环不变式要从内向外提，所以要从最内层循环开始，如果当前loop里有bb块对应的inner_loop不是当前loop，说明当前loop有内层循环，跳过这个loop，如果是inner_loop，就由此loop开始做循环不变式的寻找和外提，然后再找到它的父loop，接着处理。

    ​	然后就是循环不变式的寻找部分，这部分的处理比较简单，首先是将循环的所有bb的所有指令都插入到一个set中，然后再循环判断如果当前指令涉及到的操作数在之前的set中，说明这条指令依赖之前计算得到的值，并非循环不变式，如果没有依赖，就将其添加到循环不变式，并从那个set中删除。循环这样做，就能够找到所有的循环不变式。

    ​	最后是循环不变式的外提，这部分我做的比较简陋，就只是找到loop_base的pre_base,由于内循环有两个pre_base，判断要提到那个bb里只需要看这两个bb是否在loop里，不在的就是我们要提到的bb，然后先删掉结束的br，再插入所有的ins，最后再把br加到结尾。

    相应代码：

    找到循环不变式

    ```C++
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
    ```

    循环不变式外提：

    ```cpp
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
    ```

    

    优化前后的IR对比（举一个例子）并辅以简单说明：

    ​	testcase-1

    ```c
    void main(void){
        int i;
        int j;
        int ret;
    
        i = 1;
        
        while(i<10000)
        {
            j = 0;
            while(j<10000)
            {
                ret = (i*i*i*i*i*i*i*i*i*i)/i/i/i/i/i/i/i/i/i/i;
                j=j+1;
            }
            i=i+1;
        }
    	output(ret);
        return ;
    }
    ```

    

    优化前：

    ```cpp
    ; ModuleID = 'cminus'
    source_filename = "../tests/lab5/testcases/LoopInvHoist/testcase-1.cminus"
    
    declare i32 @input()
    
    declare void @output(i32)
    
    declare void @outputFloat(float)
    
    declare void @neg_idx_except()
    
    define void @main() {
    label_entry:
      br label %label3
    label3:                                                ; preds = %label_entry, %label58
      %op61 = phi i32 [ %op64, %label58 ], [ undef, %label_entry ]
      %op62 = phi i32 [ 1, %label_entry ], [ %op60, %label58 ]
      %op63 = phi i32 [ %op65, %label58 ], [ undef, %label_entry ]
      %op5 = icmp slt i32 %op62, 10000
      %op6 = zext i1 %op5 to i32
      %op7 = icmp ne i32 %op6, 0
      br i1 %op7, label %label8, label %label9
    label8:                                                ; preds = %label3
      br label %label11
    label9:                                                ; preds = %label3
      call void @output(i32 %op61)
      ret void
    label11:                                                ; preds = %label8, %label16
      %op64 = phi i32 [ %op61, %label8 ], [ %op55, %label16 ]
      %op65 = phi i32 [ 0, %label8 ], [ %op57, %label16 ]
      %op13 = icmp slt i32 %op65, 10000
      %op14 = zext i1 %op13 to i32
      %op15 = icmp ne i32 %op14, 0
      br i1 %op15, label %label16, label %label58
    label16:                                                ; preds = %label11
      %op19 = mul i32 %op62, %op62
      %op21 = mul i32 %op19, %op62
      %op23 = mul i32 %op21, %op62
      %op25 = mul i32 %op23, %op62
      %op27 = mul i32 %op25, %op62
      %op29 = mul i32 %op27, %op62
      %op31 = mul i32 %op29, %op62
      %op33 = mul i32 %op31, %op62
      %op35 = mul i32 %op33, %op62
      %op37 = sdiv i32 %op35, %op62
      %op39 = sdiv i32 %op37, %op62
      %op41 = sdiv i32 %op39, %op62
      %op43 = sdiv i32 %op41, %op62
      %op45 = sdiv i32 %op43, %op62
      %op47 = sdiv i32 %op45, %op62
      %op49 = sdiv i32 %op47, %op62
      %op51 = sdiv i32 %op49, %op62
      %op53 = sdiv i32 %op51, %op62
      %op55 = sdiv i32 %op53, %op62
      %op57 = add i32 %op65, 1
      br label %label11
    label58:                                                ; preds = %label11
      %op60 = add i32 %op62, 1
      br label %label3
    }
    
    ```

    优化后：

    ```cpp
    ; ModuleID = 'cminus'
    source_filename = "../tests/lab5/testcases/LoopInvHoist/testcase-1.cminus"
    
    declare i32 @input()
    
    declare void @output(i32)
    
    declare void @outputFloat(float)
    
    declare void @neg_idx_except()
    
    define void @main() {
    label_entry:
      br label %label3
    label3:                                                ; preds = %label_entry, %label58
      %op61 = phi i32 [ %op64, %label58 ], [ undef, %label_entry ]
      %op62 = phi i32 [ 1, %label_entry ], [ %op60, %label58 ]
      %op63 = phi i32 [ %op65, %label58 ], [ undef, %label_entry ]
      %op5 = icmp slt i32 %op62, 10000
      %op6 = zext i1 %op5 to i32
      %op7 = icmp ne i32 %op6, 0
      br i1 %op7, label %label8, label %label9
    label8:                                                ; preds = %label3
      %op19 = mul i32 %op62, %op62
      %op21 = mul i32 %op19, %op62
      %op23 = mul i32 %op21, %op62
      %op25 = mul i32 %op23, %op62
      %op27 = mul i32 %op25, %op62
      %op29 = mul i32 %op27, %op62
      %op31 = mul i32 %op29, %op62
      %op33 = mul i32 %op31, %op62
      %op35 = mul i32 %op33, %op62
      %op37 = sdiv i32 %op35, %op62
      %op39 = sdiv i32 %op37, %op62
      %op41 = sdiv i32 %op39, %op62
      %op43 = sdiv i32 %op41, %op62
      %op45 = sdiv i32 %op43, %op62
      %op47 = sdiv i32 %op45, %op62
      %op49 = sdiv i32 %op47, %op62
      %op51 = sdiv i32 %op49, %op62
      %op53 = sdiv i32 %op51, %op62
      %op55 = sdiv i32 %op53, %op62
      br label %label11
    label9:                                                ; preds = %label3
      call void @output(i32 %op61)
      ret void
    label11:                                                ; preds = %label8, %label16
      %op64 = phi i32 [ %op61, %label8 ], [ %op55, %label16 ]
      %op65 = phi i32 [ 0, %label8 ], [ %op57, %label16 ]
      %op13 = icmp slt i32 %op65, 10000
      %op14 = zext i1 %op13 to i32
      %op15 = icmp ne i32 %op14, 0
      br i1 %op15, label %label16, label %label58
    label16:                                                ; preds = %label11
      %op57 = add i32 %op65, 1
      br label %label11
    label58:                                                ; preds = %label11
      %op60 = add i32 %op62, 1
      br label %label3
    }
    
    ```

    可以看到把label16里的系列算术运算都提到了label8，从而避免了循环中的大量计算。




* 活跃变量分析
    实现思路：
    
    - def, use的设计
    
      由于涉及到phi函数需要记录控制流， 从而设计`useB`的类型为`std::map<BasicBlock *, std::set<std::pair<Value*, Value*>>>`， 其中第一个`Value*`存放值， 第二个`Value*`存放phi函数的2/4参数（即block）; 设计`defB`的类型为`std::map<BasicBlock *, std::set< Value*>>`
    
    - def, use的计算
    
      `def`的计算非常简单，只需要
    
      ```c++
       def.insert(instruction);
      ```
    
      而`use`的计算由于涉及到两个参数就要相对复杂一些， 其实也主要是细心
    
      对于`bineray`, 只需要将非常数值存入, 同时需要判断一下为定值, 第二个参数设置为`nullptr`
    
      ```c++
      if(instruction->is_add() || instruction->is_sub() || instruction->is_mul() || instruction->is_div() || instruction-> is_fadd() || instruction->is_fsub() || instruction->is_fmul() || instruction->is_fdiv() ||
         instruction -> is_cmp() || instruction -> is_fcmp())
      {
          if((!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
              use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0), nullptr));
          if((!cast_constantfp(instruction->get_operand(1))) && (!cast_constantint(instruction->get_operand(1))) && def.find(instruction->get_operand(1)) == def.end())
              use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(1), nullptr));
      
      }
      ```
    
      对于`load`， 其只涉及到一个参数，并且不可能是常数
    
      ```c++
      if(instruction -> is_load())
      {
          if(def.find(instruction->get_operand(0)) == def.end())
              use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0), nullptr));
      }
      
      ```
    
      对于`store`， 其实判断和`bineray`是一样的
    
      ```c++
      if(instruction -> is_store())
      {
          if((!cast_constantfp(instruction->get_operand(0))) && (!cast_constantint(instruction->get_operand(0))) && def.find(instruction->get_operand(0)) == def.end())
              use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(0), nullptr));
          if((!cast_constantfp(instruction->get_operand(1))) && (!cast_constantint(instruction->get_operand(1))) && def.find(instruction->get_operand(1)) == def.end())
              use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(1), nullptr));
      }
      ```
    
      `phi`是处理的重点: `phi`函数的var需要判断一下是否还存在， 即， 判断是否还有左值， 这里我们在fun刚开始的地方计算出所有左值存放在`set`里以方便查找
    
      ```c++
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
      ```
    
      如果var存在，那么判断是否定值， 若未定值， 则在插入时同时插入其控制流
    
      ```c++
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
      ```
    
      对于`call`， 其实判断是同上的
    
      ```c++
      if(instruction -> is_call())
      {
          for(int i = 1; i < instruction->get_num_operand(); i++)
          {
              if((!cast_constantfp(instruction->get_operand(i))) && (!cast_constantint(instruction->get_operand(i))) && def.find(instruction->get_operand(i)) == def.end())
                  use.insert(std::make_pair<Value*, Value*>(instruction->get_operand(i), nullptr));
          }
      }
      ```
    
      `gep`指令也需要注意一下参数个数与位置
    
      ```c++
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
      ```
    
      对于`ret`， 需要判断一下是否为`void`， 此外， 对于`br`， 也只需要判断一下是参数个数
    
      ```c++
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
      ```
    
    - live_in llive_out计算
    
      由于在记录use的时候记录了控制流，从而只需要在计算的时候判断一下succ的控制流
    
      ```c++
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
      ```
    
      由于此时结构与live_in, live_out并不一致， 故需要在最后取pair的第一个数值插入live_in
    
      ```c++
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
      ```
    
      

### 实验总结

**闫超美：**

​	在完成常量传播和循环不变式外提中，体会到了优化对程序性能的影响之巨大，以及学会了很多C++的容器的用法。优化是编译中最有趣也是技巧性最多的一个部分，这让我对此有了深深地理解和感悟。

**张永停**：

​	在完成活跃变量的时候， 对stl的容器有了更深的理解，同时由于一些赋值粘贴的bug导致debug了很久，这让我意识到细心的重要性。

### 实验反馈 （可选 不会评分）

对本次实验的建议

### 组间交流 （可选）

本次实验和哪些组（记录组长学号）交流了哪一部分信息
