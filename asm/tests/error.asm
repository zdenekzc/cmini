; chybovy test: pasm musi skoncit s kodem 1 a hlasenim na radku 4
code segment
 assume cs:code
 mov ax, bx
 mov ax, blah
 add
code ends
 end
