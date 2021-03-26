# lab1实验报告

学号:PB18061351  姓名:闫超美

## 实验要求

1. 根据`cminux-f`的词法补全`lexical_analyer.l`文件，完成词法分析器，能够输出识别出的`token`，`type` ,`line(刚出现的行数)`，`pos_start(该行开始位置)`，`pos_end(结束的位置,不包含)`。

## 实验难点

注释的处理，多行注释时计算行号和 `pos_start` 有一些坑,

比如：

```c
/* adsf

asdfadf*/  int main()
```

## 实验设计

### 1.运算符处理

```txt
\+ { return ADD;}
\- {return SUB;}
\* {return MUL;}
\/ {return DIV;}
\< {return LT;}
\<\= {return LTE;}
\> {return GT;}
\>\= {return GTE;}
\=\= {return EQ;}
\!\= {return NEQ;}
\= {return ASSIN;}
```



### 2. 符号处理

```txt
; {return SEMICOLON;}
, {return COMMA;}

\( {return LPARENTHESE;}
\) {return RPARENTHESE;}
\[ {return LBRACKET;}
\] {return RBRACKET;}
\{ {return LBRACE;}
\} {return RBRACE;}
```



### 3.关键字处理

```txt
else {return ELSE;}
if {return IF;}
int {return INT;}
float {return FLOAT;}
return {return RETURN;}
void {return VOID;}
while {return WHILE;}
```



### 4.ID & NUM

```txt
[a-zA-Z][a-zA-Z]* {return IDENTIFIER;}
[0-9][0-9]* {return INTEGER;}
[0-9][0-9]*[.]|[0-9]*[.][0-9][0-9]* {return FLOATPOINT;}
\[\] {return ARRAY;}
```



### 5.其他

```txt
[\n] {return EOL;}
"/*"[^\*]*(\*[\*]*[^\/\*][^\*]*)*[\*]*"*/" {return COMMENT;}
[ \r\t] {return BLANK;}
. {return ERROR;}
```



### lines 和 pos_start 和 pos_end的计算

这些计算我统一在处理正则返回后处理，分别是遇到注释，遇到空格等，遇到换行，其余情况下的处理。

遇到注释时，更新pos_start += yyleng，然后遍历整个注释串，统计换行符个数count并计算出最后一行注释的长度：为lenen-1， 然后如果注释中有换行，就将pos_start设置为lenen，同时lines要加count，否则仍为yyleng且行号不变。

遇到BLANK时，pos_start += yyleng;

遇到EOL时：lines++;  pos_start = pos_end = 1;

其他情况：更新token_stream前：pos_end = pos_start + yyleng;    然后更新token_stream，之后设置pos_start = pos_end;

```c
	int token;
	int index = 0;
	pos_end = pos_start = 1;
	int count = 0,lenen = 0,i=0;

	while(token = yylex()){
		switch(token){
			case COMMENT: 
				//STUDENT TO DO
				pos_start += yyleng;
				count = 0;
				lenen = 0;
				for(i=0; i < yyleng; i++){
					if(yytext[i] == '\n'){
						count++;
						lenen = yyleng - i;			
					}
				}
				if(count){
					lines +=count;
					pos_start = pos_end = lenen;
				}
				break;
			
			case BLANK:
				//STUDENT TO DO	
				pos_start += yyleng;
				break;
			case EOL:
				//STUDENT TO DO
				lines++;
				pos_start = pos_end = 1;
				break;
			case ERROR:
				printf("[ERR]: unable to analysize %s at %d line, from %d to %d\n", yytext, lines, pos_start, pos_end);
			default :
				if (token == ERROR){
					sprintf(token_stream[index].text, "[ERR]: unable to analysize %s at %d line, from %d to %d", yytext, lines, pos_start, pos_end);
				} else {
					strcpy(token_stream[index].text, yytext);
				}
				pos_end = pos_start + yyleng;
				token_stream[index].token = token;
				token_stream[index].lines = lines;
				token_stream[index].pos_start = pos_start;
				token_stream[index].pos_end = pos_end;
				index++;
				pos_start = pos_end;
				if (index >= MAX_NUM_TOKEN_NODE){
					printf("%d has too many tokens (> %d)", input_file, MAX_NUM_TOKEN_NODE);
					exit(1);
				}
		}
	}
```






## 实验结果验证

### shell 脚本验证六个测试

```shell
cd build;
make;
cd ../;
python3 tests/lab1/test_lexer.py;
diff tests/lab1/TA_token/1.tokens tests/lab1/token/1.tokens;
diff tests/lab1/TA_token/2.tokens tests/lab1/token/2.tokens;
diff tests/lab1/TA_token/3.tokens tests/lab1/token/3.tokens;
diff tests/lab1/TA_token/4.tokens tests/lab1/token/4.tokens;
diff tests/lab1/TA_token/5.tokens tests/lab1/token/5.tokens;
diff tests/lab1/TA_token/6.tokens tests/lab1/token/6.tokens;
echo "Finished";
```







### 我自己的一些测试样例

```c
/* * / * / * / ******/

000000.
    
000000.00

00004454984
    
.12314
    
****/*****/*****/
```





## 实验反馈

1. 实验设计比较好，锻炼了编写正则的能力
2. 明白了处理token所处位置的计算方法。