push 100
push 40
setscreensize

push 0
pop r0

;loop1_begin
push 40
push r0
sub 
jg :loop1_end

push 0
pop r1

;loop2_begin 
push 100
push r1
sub 
jg :loop2_end
;loop2_body 

push 20
push r0
sub 
# Ratio
push 2
mul
pop r3

push 50
push r1 
sub 
pop r4

push r3
push r3
mul 
push r4
push r4
mul 
add 
# Radius
push 900
sub 
pop r6

push 901
push r6
div
pop r6

# Mem index calculation
push r0
push 100
mul 
push r1
add 
pop r5

push r6
jle :skip_draw

push r0
push r1
push r2

push r6
push r6
mul
push 37
mul
sin
push 1 
add
push 0.5
mul
push 255
mul
pop r6
push r6
pop r0
push r6
pop r1
push r6
pop r2

call :rgb2double
push r0
pop [r5]

pop r2 
pop r1
pop r0

;skip_draw
push r1
push 1 
add 
pop r1
jmp :loop2_begin
;loop2_end

push r0
push 1
add 
pop r0
jmp :loop1_begin
;loop1_end 

push 1
draw
hlt

;rgb2double
push r0
floor
push 65536
mul
push r1
floor 
push 256
mul 
add 
push r2 
add 
pop r0
ret


