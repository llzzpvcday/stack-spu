push 5
pop r1
call :fact 
push r0
out 
hlt
;fact 
push r1
jnz :fact_body
push 1
pop r0
ret
;fact_body
push r1
push 1
push r1
sub
pop r1
call :fact 
push r0
mul
pop r0
ret

