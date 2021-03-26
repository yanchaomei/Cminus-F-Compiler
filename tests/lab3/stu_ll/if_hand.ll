
define dso_local i32 @main() #0 {
    ; %1指向a
    %1 = alloca float
    ; 将浮点数5.55存入 a
    store float 0x40163851E0000000, float* %1
    ; %2存a的值然后将a与1.0比较 若a大于1.0 %3为1跳转到4否则跳转到5
    %2 = load float, float* %1
    %3 = fcmp ugt float %2, 1.0
    br i1 %3, label %4,label %5

4:  
    ; a>1 返回233
    ret i32 233
5:
    ; 返回0
    ret i32 0
}

