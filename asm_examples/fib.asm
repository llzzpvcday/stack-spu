get
pop r1
call :fib 
push r0
out
hlt

;fib 
push r1
jz :fib_ret_one 
push 1
push r1
sub
jz :fib_ret_one 

push r1
push 1
push r1
sub 
pop r1
call :fib 
pop r1
push r0

push 2
push r1
sub 
pop r1

call :fib 
push r0
add
pop r0
ret

;fib_ret_one
push 1
pop r0
ret

