; vystup inline pro Turbo Pascal (pasm -i)
code segment
 assume cs:code
 mov ax, [bp+4]
 add ax, [bp+6]
 mov sp, bp
code ends
 end
