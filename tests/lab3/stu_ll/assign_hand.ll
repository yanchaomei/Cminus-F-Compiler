
define dso_local i32 @main() #0 {
    ; 声明一个10个元素的数组，得到指针放在%1里
    %1 = alloca [10 x i32], align 4
    ; 获得a[0]的地址放在%2
    %2 = getelementptr inbounds [10 x i32], [10 x i32]* %1, i64 0, i64 0
    ; 将10存储到%2指向的内存单元，也就是a[0]所在的位置
    store i32 10, i32* %2, align 4
    ; 将a[0]的值取出来放到%3
    %3 = load i32, i32* %2, align 4
    ; 获得a[1]的地址放在%4
    %4 = getelementptr inbounds [10 x i32], [10 x i32]* %1, i64 0, i64 1
    ; 将%3与2相乘存到%5,也就是a[0] * 2
    %5 = mul i32 %3,2
    ; 将%5的值也就是乘法结果放在%4指向的位置，也就是a[1]
    store i32 %5, i32* %4, align 4
    ; 结果返回a[1]，也就是%5的值
    ret i32 %5
}

