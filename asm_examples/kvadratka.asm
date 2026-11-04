get
pop r1
get
pop r2
get
pop r3
call :all_zero_check1
call :check_only_c1
push r1 
jz :a_is_zero

# Discriminant
push 4 
push r1 
mul 
push r3 
mul 
push r2 
push r2 
mul
sub
pop r4

push r4 
jz :d_is_zero
push r4 
jl :d_is_less_zero

# Root 1
push r1 
push 2 
mul 
push r4 
sqrt 
push r2 
push -1 
mul 
add
div 
out
# Root 2
push r1 
push 2 
mul 
push r4 
sqrt 
push -1 
mul
push r2 
push -1 
mul 
add
div 
out

hlt
;all_zero_check1
push r1
jz :all_zero_check2 
ret 
;all_zero_check2
push r2 
jz :all_zero_check3 
ret 
;all_zero_check3 
push r3 
jz :all_zero_check4
ret
;all_zero_check4 
push 67
out
hlt

;check_only_c1
push r1
jz :check_only_c2 
ret
;check_only_c2 
push r2 
jz :check_only_c3 
ret 
;check_only_c3
push 1 
out 
hlt

;a_is_zero
push r2 
push r3
push -1 
mul 
div
out 
hlt

;d_is_zero
push r1 
push 2 
mul 
push r2 
push -1 
mul 
div 
out 
hlt

;d_is_less_zero
push 0
out 
hlt
