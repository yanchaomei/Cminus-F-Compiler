; callee 函数
define dso_local i32 @callee(i32 %0) #0 {
    ; 传入的参数%0与2相乘存入%2
    %2 = mul i32 %0,2
    ; 返回相乘的结果 
    ret i32 %2
}
; main 函数
define dso_local i32 @main() #0 {
    ; 调用callee函数，传入参数110，返回结果存入%1
    %1 = call i32 @callee(i32 110)
    ; 返回调用结果%1
    ret i32 %1
}