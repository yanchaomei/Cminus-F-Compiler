# Lab4 实验报告

小组成员：

闫超美 PB18061351

张永停 PB17111585

## 实验要求

1. 理解框架代码与 Cminusf 的语义，填写使用访问者模式的 `src/cminusf/cminusf_builder.cpp`，使编译出的 `cminusf` 可以编译 Cminusf 程序到 LLVM IR 或可执行文件。
2. 编写测试样例，确保在源程序符合 Cminusf 语义的情况下，`cminusf` 可以得到正确的结果。

## 实验难点

* 对访问者模式的理解，以及研究框架，要清楚流程就是访问AST来生成中间代码，在访问不同的节点时生成对应的代码，最终访问完后要能生成正确的LLVM IR。
* 全局变量的选择，不同的函数之间是有变量的传递的，这部分的传递一是可以在scope里做，当它们在同一个作用域里。也可以使用全局变量，保存访问前面的节点时需要传递的信息，比如当有多个函数的时候，会访问很多次Funcdeclaration节点，所以每次都要用一个全局变量来保存函数信息，表明是当前是在哪个函数里面，之后的选择和循环插入标签时就需要用到。
* 作用域问题，要搞清楚在哪里enter scope，在哪里退出scope。
* return的处理，比如在while和if里出现了return，那么这时候就不能再br了，不然可能会导致执行时寄存器编号出错。
* 类型转换，float当成int输入时需要自己对它做类型转换, 数组下标为float需要转成int,  比较结果如果需要赋值给int 或者 float需要作类型转换，返回值类型和函数签名中的返回类型不一致时，函数调用时实参和函数签名中的形参类型不一致时 都需要进行转换。
* 数组传参，数组传参要转化成指针传入。而且确定在什么时候转换比较难搞。
* 数组下标为负数时的检测。要在每一次遇到数组索引的情况都判断一次。

## 实验设计

### 全局变量设计

因为LightIR的指令也是Value*类型，故实际用于传递中间值的全局变量只有一个 `Value *current_value`，还有一些其他类型的数据需要传递，所以加入了更多的全局变量。

1. current_value：作用就是传递框架中的中间变量，比如在assign的逻辑中，先是访问左值，然后在左值中要将得到的值传给current_value，然后在assign赋值操作中通过current_value来获得左值，这里左值通常是alloca的地址。然后再访问右值子节点，同样通过current_value来获得右值，最后再做赋值
2. current_type：当前的类型值
3. current_func：当前所在的函数体内
4. current_bb：当前所在的block内
5. return_alloca：返回时为返回值所声明的空间，需要传递后获得值后赋值
6. var_mode：用于从外部告诉 `ASTVar` 本次访问 var 是想要读还是写
7. width： 当前期望的Int的位数，为了方便类型转换而设置
8. if_count, while_count, idx_count： 分别对应当前是第几个if, while, 数组下标检测语句， 为了方便block命名而设置

### 类型转换设计

#### 赋值时检测

本部分在`CminusfBuilder::visit(ASTAssignExpression &node) `中实现

这里强制将右值转换为左值。为了减少代码冗余，我们将类型转换部分单独抽出来作为一个函数。

```c++
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

```

注意到这里为了函数的普适性， 我们并没有将`int1 int 32`的转换放在该函数中， 宽度转换单独写在`CminusfBuilder::visit(ASTAssignExpression &node)`中

```c++
	right_value = current_value;
    right_value = CastRightValue(left_alloca->get_type()->get_pointer_element_type(), right_value, builder, module, this_width);
    builder->create_store(right_value, left_alloca); 
    if(this_width == 1)
    {
        if(current_value->get_type()->is_integer_type())
            current_value = builder->create_icmp_ne(current_value, CONST_INT(0));
        if(current_value->get_type()->is_float_type())
            current_value = builder->create_fcmp_ne(current_value, CONST_FP(0));
    }
```

#### 函数返回类型检测

这里使用全局变量`return_alloca`来保存该函数的返回类型， 在`void CminusfBuilder::visit(ASTReturnStmt &node)` 中强制将类型转为该返回类型。实质也是将右值类型转为左值类型的操作，因此可以调用上面的函数。

```c++
void CminusfBuilder::visit(ASTReturnStmt &node) {
    if(node.expression){
        width = 32;
        node.expression->accept(*this);
        auto ret_var = current_value;
        ret_var = CastRightValue(return_alloca->get_type()->get_pointer_element_type(), ret_var, builder, module);
        builder->create_ret(ret_var);
    }else{ 
        builder->create_void_ret();
    }
 }
```

#### 函数调用时类型检测

函数调用的类型检测其实也和返回检测类似(其实都差不多)， 这里将传入参数作为右值转换为声明类型。

```c++
for (auto arg: node.args) {
            arg->accept(*this);
            args.push_back(CastRightValue((*iter),current_value,builder,module));
            iter++;
        }
```

#### 二元运算时类型检测

我们采用以下策略进行转换

- 只要不是两个变量都是`int`类型，我们一律转为`float`

  ```c++
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
  ```

  

- 根据全局变量`width`判断是否应该进行`int32/float -> int1`的转换

  ```c++
  if(this_width == 1)
          {
              if(current_value->get_type()->is_integer_type())
                  current_value = builder->create_icmp_ne(current_value, CONST_INT(0));
              if(current_value->get_type()->is_float_type())
                  current_value = builder->create_fcmp_ne(current_value, CONST_FP(0));
          }
  ```

- 根据`width`判断是否进行`zext`

  ```c++
  if(this_width == 32)
          {
              if(current_value->get_type()->is_integer_type())
                  current_value = builder->create_zext(current_value, Type::get_int32_type(module.get()));
              if(current_value->get_type()->is_float_type())
                  current_value = builder->create_zext(current_value, Type::get_float_type(module.get()));
          }
  ```

#### 下标计算时类型检测

在`CminusfBuilder::visit(ASTVar &node)`中实现，强制将下标作为右值转为`int32`

(为了不使报告太冗长，这里仅仅放关键代码)

```c++
current_value = CastRightValue(Type::get_int32_type(module.get()), current_value, builder, module);
```

```c++
index = CastRightValue(Type::get_int32_type(module.get()), index, builder, module);
```



### 难点的解决方案

#### 作用域问题

这里主要是看要在哪进入scope，一般来说在处理ASTCompoundStmt时，也就是{}时需要进入作用域，然后结束时要出作用域。还有就是在函数部分，也就是访问ASTFunDeclaration时，需要在定义好函数后进入scope，然后将函数的参数给push进去。在结束函数时出scope。

#### Var处理

文法是上下文无关的，因此我们只能从入口告诉 var 的处理函数：本次访问 var 是想读还是想写。这里有一个专门的全局变量，从 `factor` 进入前设成“读模式”，从 `assign_expression` 的左半部分进入前设成“写模式”，然后在 var 处理函数里获取该变量对应的 alloca；如果是读模式就额外创建一条 `load` 指令并返回；如果是写模式就直接返回该 alloca，以便外层创建 `store` 指令。

#### return处理

return 会产生 `ret` 指令，它是一个终结指令，因此当 return 出现在某些位置（例如 if 的分支中）时会产生额外的终结指令，导致 LLVM IR 的字段序号错误。

解决方案：在if和while处理完一个block后判断最后一条指令是否是ret指令，如果是就跳过br。主要是检查`trueBB->getTerminator()` 是不是 `nullptr`。

返回值的处理方式：先为返回值创建一个alloca，然后在得到返回值后填入这个地址。在函数体处理完毕之后将该块放到函数末尾。

#### 下标检测负数

- 为了减少IR冗余，我们一个函数仅有一个检测是否为负数下标的区块。

- 检测在`CminusfBuilder::visit(ASTVar &node)`中实现。

- 当我们检测到变量为数组时

  ```c++
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
  ```

- 当为指针时，检测部分代码其实和为数组一样

  ```c++
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
  ```

#### 数组的处理

该部分处理在`void CminusfBuilder::visit(ASTVar &node)`中是实现

- 判断什么时候转换为指针

  ```c++
  if(var_mode == LOAD && current_value->get_type()->is_pointer_type())
          {
              if(current_value->get_type()->get_pointer_element_type()->is_array_type())
              {
                  current_value = builder->create_gep(current_value, {ConstantInt::get(0, module.get()),ConstantInt::get(0, module.get())});
                  this_mode = STORE;
                  //skip funa((int[])b) load
              }
          }
  ```

- 从数组类型接受

  ```c++
  current_value = builder->create_load(x);
              index = CastRightValue(Type::get_int32_type(module.get()), index, builder, module);
  
  ```

  ```c++
  current_value = builder->create_gep(current_value, {index});
  ```

- 从函数参数接受

  ```
  current_value = CastRightValue(Type::get_int32_type(module.get()), current_value, builder, module);
                  
  current_value = builder->create_gep(x, {ConstantInt::get(0, module.get()), current_value});
  ```



### 其他细节&&test设计

#### 一些细节

- 全局变量的处理, 这个需要初始化

  ```c++
  if(scope.in_global())
      {
          GlobalVariable *global_var;
          global_var = GlobalVariable::create(node.id, module.get(), node_type, false, ConstantZero::get(node_type, module.get()));
          scope.push(node.id, global_var);
      }
  ```

- 区块命名。为了方便阅读，添加if_count, while_count, idx_count： 分别对应当前是第几个if, while, 数组下标检测语句

  ```c++
  if_count ++;
  auto trueBB = BasicBlock::create(module.get(), "trueBB"+std::to_string(if_count), current_func);
      auto falseBB = BasicBlock::create(module.get(), "falseBB"+std::to_string(if_count), current_func);
      auto retBB = BasicBlock::create(module.get(), "retBB"+std::to_string(if_count), current_func);
  ```

- 然后就是杂七杂八的函数返回，数组传递等问题。本次实验感觉细节还是很多的

#### 一些测试样例

- 类型检测与转换

  赋值、返回，参数检测

  ```c++
  float x[10];
  float foo(float a)
  {
  	a = a + 1;
  	a = a - a;
  	return a;
  }
  
  int fooo(float b)
  {
  	if(b)
  	{return b;}
  	else
  	{return b-1;}
  }
  
  void main(void)
  {
  	float a;
  	int b;
  	a = 1.5;
  	b = x[0];
  	a = a + b;
  	a = foo(a);
  	a = fooo(a);
  	outputFloat(a);
  }
  
  ```

  下标整数转换

  ```c++
  int foo(int u[])
  {
  	return u[3.5];
  }
  void main(void)
  {
  	int a[10];
  	a[3] = 3;
  	a[1.6] = foo(a);
  	output(a[1]);
  }
  ```

  int32/float <-- int1

  ```c++
  int main(void){
  	int c;
  	if(123.0)
  		c = 1;
  	
  	return c;
  }
  ```

  

- 下标负数检测

  ```c++
  void foo(int u[])
  {
  	int a;
  	int i;
  	i = 9;
  	i = 0 - i;
  	a = u[i];	
  }
  void main(void)
  {
  	int a[10];
  	int b;
  	b = 2;
  	b = 0 - b;
  	b = a[b];
  	foo(a);
  }
  ```

- return测试

  ```c++
  int fff(int x){
  	return x+x-x;
  }
  int x[10];
  void main(void){
  	int i;
  	i = 0-1;
  	while (i < 9)
  		output(x[i=i+1]);
  	i = 0-1;
  	while (i < 9)
  		x[i=i+1]=input();
  	while (i >= 0)
  		if (fff(x[i])==0){
  			output(i);
  			return;
  		}else i=i-1;
  	output(i);
  	return;
  }
  ```

  等共计30多个简单样例

## 实验总结

* 闫超美：学到了很多cpp的语法，以及对面向对象编程有了更加深入的理解，尤其是对访问者模式在生成中间代码的妙用有很大的触动，没有想到设计模式在写这种比较大的项目中带来的巨大的优势。同时对LightIR这个框架有了更多的了解，处理生成中间代码需要考虑到的边边角角的问题的能力有所提升。
* 张永停：对面向对象的编程有了更深的理解，对解耦，继承，理解更深了（嗯是这样），同时大大提高了自己DEBUG的能力以及测试的能力， 对各种C的语言特性有了更深的理解。以及爆肝一天一夜的我的不赶ddl的flag又倒了（最开始没有看到助教cminusf的文档，自己全靠看源文件和样例来研究函数的用法浪费了很多时间，下次一定记得把文档看完。



## 实验反馈 （可选 不会评分）

助教文档及其详细，赞！（就是我没看到，哭唧唧，导致全部类型转换那块的范例思考了好一会）

## 组间交流 （可选）

本次实验和哪些组（记录组长学号）交流了哪一部分信息

