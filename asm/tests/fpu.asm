; koprocesor 8087..387 - instrukce opravene pri prevodu
        .386
        .387
code    segment
        assume  cs:code, ds:code
        fld     st(1)
        fld     dword ptr [bx]
        fst     dword ptr [bx]
        fstp    st(2)
        fstp    tbyte ptr [bx]
        fist    word ptr [bx]
        fistp   qword ptr [bx]
        fldz
        fld1
        fprem1
        fucom   st(2)
        fucomp  st(3)
        fxch    st(1)
        fadd    st, st(1)
        faddp   st(1), st
        fsub    dword ptr [bx]
        fdivr   st(1), st
        fstsw   ax
        fnstsw  word ptr [bx]
code    ends
        end
