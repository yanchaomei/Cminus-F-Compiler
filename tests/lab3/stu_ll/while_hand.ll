define dso_local i32 @main() #0 {
    ; 声明两个int变量 %1指向a %2指向i
    %1 = alloca i32
    %2 = alloca i32
    ; 将10存入a 将0存入i
    store i32 10, i32* %1
    store i32 0, i32* %2
    ; 跳转到3
    br label %3
3:
    ; load一下 %4的值为a的值 %5的值为i的值
    %4 = load i32, i32* %1
    %5 = load i32, i32* %2
    ; 比较i与10，若i<10 %6为1跳转到7，否则跳转到10
    %6 = icmp slt i32 %5, 10
    br i1 %6, label %7, label %10
     
7:
    ; 循环体部分，%5+1 也就是i+1存入%8
    %8 = add i32 %5, 1
    ; %4+%8也就是a+i存入%9
    %9 = add i32 %4, %8
    ; 将之前计算的值%8存入i %9存入a
    store i32 %8, i32* %2
    store i32 %9, i32* %1
    ; 跳转到标签3
    br label %3
10:
    ; 如果跳出循环体 就返回a的值
    ret i32 %4
}