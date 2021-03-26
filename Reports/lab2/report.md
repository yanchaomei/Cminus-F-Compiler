# lab2 实验报告

PB18061351 闫超美

## 实验要求

1. 了解 bison 基础知识和理解 Cminus-f 语法（重在了解如何将文法产生式转换为 bison 语句）
2. 阅读 /src/common/SyntaxTree.c，对应头文件 /include/SyntaxTree.h（重在理解分析树如何生成）
3. 了解 bison 与 flex 之间是如何协同工作，看懂pass_node函数并改写 Lab1 代码
4. 补全 src/parser/syntax_analyzer.y 文件和 lexical_analyzer.l 文件

## 实验难点

实验难点在于理解 bison 与 flex 协同工作的方式，以及看懂SyntaxTree.h提供的建立语法树的API，并调用API完成实验

## 实验设计

实验需要做的是修改.l文件使得能够协同工作，以及完善.y文件使得能够生成一个C-minus的语法分析器，并在语法分析的过程中生成语法分析树输出到文件中。

### 修改 lexical_analyzer.l 文件

首先因为提供的文法中没有对EOL COMMENT BLANK的处理，也就是说这些不应该返回给bison，虽然这些也需要进行解析，但是生成token stream时不需要把这些加入，所以把这些正则后的return给删掉了。

然后就是处理其他的正则，在识别到token时要先调用pass_node函数，来把yytext传入，其实也就传入yytext新建了一个node，

最后是把其他情况时返回ERROR改成了返回0。

### 完善 syntax_analyzer.y 文件

bison主要接受从flex处理完词法后传递的token，我们需要做的就是编写文法，用.l文件中返回的token作为终结符，来将c-minus文法中的终结符给替换，在规约每一个产生式后，都将当前产生式的左边作为父节点，右边作为子节点来挂载。这样当规约完成后就能得到语法分析树。

首先要定义非终结符和终结符的type：

``` c

%union { 
    syntax_tree_node * node;
}

%token <node> ADD SUB MUL DIV LT LTE GT GTE EQ NEQ ASSIN
%token <node> SEMICOLON COMMA
%token <node> LPARENTHESE RPARENTHESE LBRACKET RBRACKET LBRACE RBRACE
%token <node> ELSE IF INT FLOAT RETURN VOID WHILE
%token <node> IDENTIFIER INTEGER FLOATPOINT ARRAY
%token <node> EOL COMMENT BLANK
%type <node> program type-specifier relop addop mulop
%type <node> declaration-list declaration var-declaration fun-declaration local-declarations
%type <node> compound-stmt statement-list statement expression-stmt iteration-stmt selection-stmt return-stmt
%type <node> expression var additive-expression term factor integer float call simple-expression
%type <node> params param-list param args arg-list
```

这样对非终结符和终结符的操作就是对节点指针的操作，每次规约后可以直接将产生式左边和右边的节点指针进行连接，从而方便建立分析树。对于非终结符节点指针来自于之前的规约，对于终结符，节点指针来自于.l文件中pass_node函数生成并通过yylval进行传递。

接下来需要做的就是写文法，并写规约后的建立节点与连接的操作。

示例：

```c
program : declaration-list { $$ = node("program", 1, $1); gt->root = $$; }

declaration-list : declaration-list declaration
{ $$ = node("declaration-list", 2, $1, $2);}
| declaration
{$$ = node("declaration-list", 1, $1);}
;

declaration : var-declaration
{$$ = node("declaration", 1, $1);}
| fun-declaration
{$$ = node("declaration", 1, $1);}
;
```

## 实验结果验证

执行 shell 脚本自动验证：

简单测试集：
``` shell
cp@cp:~/labs/test_lab2/2020fall-compiler_cminus$ ./tests/lab2/test_syntax.sh easy yes
[info] Analyzing array.cminus
[info] Analyzing call.cminus
[info] Analyzing div_by_0.cminus
[info] Analyzing expr-assign.cminus
[info] Analyzing expr.cminus
[info] Analyzing FAIL_array-expr.cminus
error at line 2 column 17: syntax error
[info] Analyzing FAIL_decl.cminus
error at line 2 column 10: syntax error
[info] Analyzing FAIL_empty-param.cminus
error at line 1 column 10: syntax error
[info] Analyzing FAIL_func.cminus
error at line 1 column 18: syntax error
[info] Analyzing FAIL_id.cminus
error at line 1 column 6: syntax error
[info] Analyzing FAIL_local-decl.cminus
error at line 4 column 5: syntax error
[info] Analyzing FAIL_nested-func.cminus
error at line 3 column 13: syntax error
[info] Analyzing FAIL_var-init.cminus
error at line 2 column 8: syntax error
[info] Analyzing func.cminus
[info] Analyzing if.cminus
[info] Analyzing lex1.cminus
[info] Analyzing lex2.cminus
[info] Analyzing local-decl.cminus
[info] Analyzing math.cminus
[info] Analyzing relop.cminus
[info] Comparing...
[info] No difference! Congratulations!
```

normal测试集：

```shell
cp@cp:~/labs/test_lab2/2020fall-compiler_cminus$ ./tests/lab2/test_syntax.sh normal yes
[info] Analyzing array1.cminus
[info] Analyzing array2.cminus
[info] Analyzing func.cminus
[info] Analyzing gcd.cminus
[info] Analyzing if.cminus
[info] Analyzing selectionsort.cminus
[info] Analyzing tap.cminus
[info] Analyzing You_Should_Pass.cminus
[info] Comparing...
[info] No difference! Congratulations!
```

我自己构造的测试集：

```c

float aaaaa[12322]

/* adsfasdga */

void func(int a, int fasdf){
    return 1+1;
}

int main(){

    func(1,2);

    return 0;
}
```

## 实验反馈

1. 实验文档写的很棒，给的几个例子能很好让我理解实验的意图以及需要做的事情。
2. 测试集很详细
3. 学到了bisond的用法。
