/* INSTR.C */

/* Generator kodu x86-64 prekladace c-mini - jediny generator bez asmjit
   (README.txt, kapitola 30; drive tri varianty instr.cc, instr0.cc,
   instr-list.cc - text pro assembler byl ve vsech bajtove shodny).

   - Ciste C (2026-11-16, README.txt kapitola 103; drive instr.cc v podmnozine
     c-mini a z nej cmini.bin -C vygenerovany instr.c): prelozi ho gcc, tcc
     (bootstrap) i sam cmini.bin (samopreklad) - proto jen konstrukce, ktere
     c-mini zna: struktury s polozkami long / ukazatel (polozka = 8 bajtu),
     bez ukazatelu na funkce v datech generatoru, bez struktur hodnotou.
   - Retezce char * (nikdy NULL, "" = prazdny; Cat, Str, IntStr ... alokuji
     a neuvolnuji - generator bezi jednou na preklad), Seq (dynamicke pole
     hodnot long) misto Sequence, Dict / SDict (hashovaci tabulka, klic cislo
     nebo ukazatel / retezec) misto Dictionary - pomocne funkce nize.
   - Bez trid: stav generatoru jsou globalni (static) promenne, funkce
     generatoru obycejne funkce (generator ma vzdy jen jeden stav -
     initState pred prekladem, freeState po nem).
   - Program se sklada jako spojovy seznam instrukci (asm/asmlist.h) -
     obdoba x86::Builder z AsmJit:
        ins2 (iMov, gp (RAX), qword_ptr (RBP, -16))
        note (ins2 (iAdd, gp (RAX), imm (8)), "x")     poznamka za ;
     instrukce = konstanty keyword z asm/key.h (iMov, iPush, iCall, ...),
     registry = vycet Register z asm/reg.h (RAX, RCX, R11, ...),
     operandy: gp (registr), xreg (XMM), imm (cislo), imm_hex (0...h),
     qword_ptr / byte_ptr / any_ptr (adresa [baze+posun]), any_ptr_idx
     ([baze+index*m]), qword_lab ([navesti]), alab (adresa navesti - lea),
     jlab (cil skoku / volani).  Operandy jsou ukazatele AsmOp * (rozhrani
     asmlist.h s ukazateli: c-mini ma vlastni model pameti struktur,
     struktury hodnotou nepredava).
   - jeden AsmProgram, tri kurzory (jako sekce v AsmJit): extrn / public
     (doplni se na konci), data a kod; behova knihovna RTL_... zustava
     textem (uzly AN_TEXT),
   - na konci AsmAssemble (knihovna pasm cte uzly primo - faze 2;
     CPARSER_ASMLIST=text: AsmAssembleText, text po radcich - faze 1);
     -o x: text pro assembler x.asm (AsmPrint) a vypis x.lst.

   Rozhrani instr.h: runInstructions, InstrLink, instructions,
   InstrOutputFile, InstrObjectFile.  Puvodni popis generatoru nasleduje.
*/


/*
   Generator kodu pro x86-64 v podobe textu v assembleru (syntaxe MASM)
   pro assembler pasm z adresare asm/ - obdoba instr1.cc, misto knihovny
   asmjit se vytvori zdrojovy text, ze ktereho pasm -e sestavi spustitelny
   soubor ELF64 (bez linkeru a bez knihovny C), a ten se spusti.

   Postup (run):
     1. vygeneruje se text programu (datovy a kodovy segment USE64),
     2. ulozi se do docasneho adresare jako prog.asm,
     3. vychozi (glibc): pasm -c -l prog.asm => prog.o, gcc -no-pie prog.o
        => prog.bin;  CPARSER_RTL=own (vlastni behova knihovna, bez glibc):
        pasm -e -l prog.asm  =>  prog.bin (ELF64);  vypis prog.lst,
     4. prog.bin se spusti jako samostatny proces; vystup programu jde
        primo na standardni vystup, vysledek funkce main (64 bitu) program
        zapise do deskriptoru 3 (roura z rodicovskeho procesu) - navratovy
        kod procesu ma jen 8 bitu.

   Zasobnikovy model (stejny jako instr1.cc):
     - vysledek kazdeho (pod)vyrazu je v registru rax,
     - mezivysledky se docasne ukladaji na zasobnik (push / pop),
     - rcx, rdx a r11 jsou pomocne registry.

   Pamet:
     - kazda promenna (int, bool, char, ukazatel, ...) zabira 8 bajtu,
       pole N prvku zabira N * 8 bajtu,
     - globalni promenne jsou v datovem segmentu: navesti G<n>
       (adresa: lea rax, G<n> - relativne k RIP),
     - parametry a lokalni promenne lezi v ramci funkce: [rbp - offset],
     - reference (int & x) je ulozena jako adresa.

   Volani funkci - registry jako System V AMD64:
     - parametry v rdi, rsi, rdx, rcx, r8, r9 (nejvyse 6), vysledek v rax,
     - funkce s telem maji navesti F<n>, ramec
          push rbp / mov rbp, rsp / sub rsp, N  ...  mov rsp, rbp / pop rbp / ret
     - funkce bez tela (printf, abs, ...) jsou v behove knihovne napsane
       v assembleru (RTL_DATA, RTL_CODE nize): printf, putchar, puts,
       abs, labs, rand, srand, time, exit.

   Navesti: L<n> skoky, S<n> retezcove konstanty.

   Struktury a ukazatele (jako instr5.cc):
     - kazda polozka zabira 8 bajtu, p->x, s.x, *p, &x, p [i],
     - new T  =>  calloc (1, velikost) z behove knihovny, potom konstruktor.

   Tridy:
     - class, public / private / protected (pristup se nekontroluje),
       jednoducha dedicnost  class B : public A,
     - rozlozeni objektu: polozky baze, ukazatel na tabulku virtualnich metod
       (zavede ho prvni polymorfni trida v hierarchii), vlastni polozky;
       baze je na zacatku objektu => B * lze pouzit jako A * bez posunu,
     - metody: skryty parametr this (rdi), dalsi parametry rsi .. r9,
       definice uvnitr tridy i mimo ni (int A::f () { ... }), staticke metody,
     - virtualni metody: tabulka VT<n> v datech (sloty baze, prekryte metody
       na stejnem miste, nove na konci), volani  call [[this + vptr] + 8 * slot],
       ciste virtualni metoda => RT_PURE, A::f () vola bez virtualniho mechanismu,
     - konstruktor (uzivatelsky nebo implicitni C<n>): konstruktor baze,
       ukazatel na tabulku virtualnich metod, konstruktory vnorenych objektu,
       potom inicializace polozek a telo; vraci this,
     - objekty se konstruuji pri new, pri deklaraci lokalni promenne
       a pred main (globalni objekty, funkce CINIT); i pole objektu,
     - destruktory se jen prelozi (delete zatim neni).

   Dalsi prostredky jazyka (ukazatele, float / double, switch, goto,
   std::string, Sequence, Dictionary, ...) - README.txt, kapitoly 15 - 24.

   Assembler pasm se vola jako knihovna (asm/libpasm.a: asmlist.h, pasmlib.h).
*/

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L   /* gcc -std=c99: mkdtemp, fileno, setenv */
#endif

#include "../asm/asmlist.h"  /* program jako seznam instrukci (pred decl.h) */
#include "instr.h"
#include "decl.h"
#include "../asm/pasmlib.h"   /* assembler pasm jako knihovna */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>       /* isalnum */
#include <unistd.h>      /* fork, pipe, dup2, execv */
#include <sys/wait.h>    /* waitpid */
#include <sys/stat.h>    /* chmod */
#include <stdio.h>
#include <stdint.h>

#include "util.h"   /* retezce, Seq, Dict (spolecne s translate.c) */

/* ---------------------------------------------------------------------- */

/* NewObject (decl.h) ma parametr IDENT_TYP (char [IDENT_SIZE]) - jmeno pres buffer */
static ObjectPtr newObj (const char * name, TypePtr desc)
{
    IDENT_TYP id;
    strncpy (id, name, IDENT_SIZE - 1);
    id [IDENT_SIZE - 1] = 0;
    return NewObject (id, desc);
}

static char * InstrContext = "";   /* prave prekladana funkce (chybova hlaseni) */

static void InstrError (const char * msg)
{
    if (! StrEmpty (InstrContext))
        msg = Cat4 (msg, " (in ", InstrContext, ")");
    Error ((char *) msg);
}


/* Behova knihovna v assembleru (pasm, x86-64, volani jadra pres syscall).
   Volaci konvence jako generovany kod: parametry rdi, rsi, rdx, rcx, r8, r9,
   vysledek rax; zachovavaji se rbx, rbp, r12 .. r15. */

static const char * RTL_DATA [] =
{
    "",
    "; ---- behova knihovna: data ----",
    "rt_obuf  db      4096 dup (0)            ; buffer standardniho vystupu",
    "rt_olen  dd      0",
    "rt_count dd      0                       ; pocet znaku vypsanych v printf",
    "rt_seed  dq      1                       ; rand",
    "rt_arg0  db      'program_name', 0",
    "rt_argv  dq      offset rt_arg0, 0       ; argv pro main",
    "rt_heap  dq      0                       ; prvni volna adresa haldy",
    "rt_hend  dq      0                       ; konec haldy (brk)",
    "rt_amsg  db      'Assertion failed in function ', 0",
    "rt_pmsg  db      'Pure virtual method called', 10, 0",
    "rt_nl    db      10, 0",
    NULL
};

/* std::string - spolecne pro vlastni knihovnu i glibc (alokace RT_CALLOC) */
static const char * RTL_STR_DATA [] =
{
    "",
    "; ---- std::string: data ----",
    "         dq      -1                      ; pocet referenci (-1 = trvaly)",
    "         dq      0                       ; delka prazdneho std::string",
    "rt_sempty db     0",
    "; zasobnik docasnych hodnot (retezce, kontejnery) - uvolni se na konci prikazu",
    "rt_tbuf  dq      0",
    "rt_tcnt  dq      0",
    "rt_tcap  dq      0",
    "rt_qmsg  db      'Sequence (index out of range)', 0",
    "rt_bcmsg db      'dynamic_cast (bad_cast)', 0",
    NULL
};

static const char * RTL_STR_CODE [] =
{
    "",
    "; ---- std::string ----",
    "; hodnota = ukazatel na znaky (ukoncene nulou), pred nimi [-8] delka a [-16]",
    "; pocet referenci (-1 = konstanta, trvala); 0 = prazdny retezec; retezce se",
    "; nemeni (+ a += vytvori novy); promenna, polozka, prvek vlastni 1 referenci,",
    "; nove retezce (vysledky +, substr, ...) jsou docasne hodnoty (RT_STEMP)",
    "",
    "; ---- delka:  rdi = retezec ----",
    "RT_SLEN proc    near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rsl1",
    "        mov     rax, qword ptr [rdi-8]",
    "rsl1:   ret",
    "RT_SLEN endp",
    "",
    "; ---- c_str:  rdi = retezec => ukazatel na znaky ----",
    "RT_SCSTR proc   near",
    "        mov     rax, rdi",
    "        test    rax, rax",
    "        jnz     rsc1",
    "        lea     rax, rt_sempty",
    "rsc1:   ret",
    "RT_SCSTR endp",
    "",
    "; ---- novy retezec:  rdi = delka => rax = znaky (vynulovane), 1 reference ----",
    "RT_SNEW proc    near",
    "        push    rdi",
    "        lea     rsi, [rdi+17]           ; pocet referenci + delka + koncova nula",
    "        mov     edi, 1",
    "        call    RT_CALLOC",
    "        pop     rdi",
    "        test    rax, rax",
    "        jz      rsn1",
    "        mov     qword ptr [rax], 1",
    "        mov     qword ptr [rax+8], rdi",
    "        add     rax, 16",
    "rsn1:   ret",
    "RT_SNEW endp",
    "",
    "; ---- reference navic:  rdi = retezec => rax = retezec ----",
    "RT_SINC proc    near",
    "        mov     rax, rdi",
    "        test    rdi, rdi",
    "        jz      rsi9",
    "        cmp     qword ptr [rdi-16], 0",
    "        jl      rsi9                    ; konstanta",
    "        inc     qword ptr [rdi-16]",
    "rsi9:   ret",
    "RT_SINC endp",
    "",
    "; ---- reference mene:  rdi = retezec (posledni => uvolnit) ----",
    "RT_SDEC proc    near",
    "        test    rdi, rdi",
    "        jz      rsx9",
    "        cmp     qword ptr [rdi-16], 0",
    "        jle     rsx9                    ; konstanta",
    "        dec     qword ptr [rdi-16]",
    "        jnz     rsx9",
    "        sub     rdi, 16",
    "        sub     rsp, 8                  ; zarovnani na 16 (glibc)",
    "        call    RT_FREE",
    "        add     rsp, 8",
    "rsx9:   ret",
    "RT_SDEC endp",
    "",
    "; ---- prirazeni:  rdi = adresa promenne, rsi = nova hodnota => rax ----",
    "RT_SASSIGN proc near",
    "        push    rbx",
    "        push    r12",
    "        sub     rsp, 8",
    "        mov     rbx, rdi",
    "        mov     r12, rsi",
    "        mov     rdi, rsi",
    "        call    RT_SINC                 ; nejdriv nova (i s = s)",
    "        mov     rdi, qword ptr [rbx]",
    "        mov     qword ptr [rbx], r12",
    "        call    RT_SDEC",
    "        mov     rax, r12",
    "        add     rsp, 8",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_SASSIGN endp",
    "",
    "; ---- docasna hodnota:  rdi = polozka (retezec, kontejner | 1, | 2), 0 => nic ----",
    "RT_TPUSH proc   near",
    "        test    rdi, rdi",
    "        jz      rtk9",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdi",
    "        mov     rax, qword ptr [rt_tcnt]",
    "        cmp     rax, qword ptr [rt_tcap]",
    "        jb      rtk2",
    "        mov     r12, qword ptr [rt_tcap]",
    "        add     r12, r12",
    "        jnz     rtk1",
    "        mov     r12d, 64",
    "rtk1:   mov     rdi, r12",
    "        mov     esi, 8",
    "        call    RT_CALLOC",
    "        mov     r13, rax",
    "        mov     rsi, qword ptr [rt_tbuf]",
    "        test    rsi, rsi",
    "        jz      rtk3",
    "        mov     rdi, r13",
    "        mov     rcx, qword ptr [rt_tcnt]",
    "        rep     movsq",
    "        mov     rdi, qword ptr [rt_tbuf]",
    "        call    RT_FREE",
    "rtk3:   mov     qword ptr [rt_tbuf], r13",
    "        mov     qword ptr [rt_tcap], r12",
    "rtk2:   mov     rax, qword ptr [rt_tcnt]",
    "        mov     rcx, qword ptr [rt_tbuf]",
    "        mov     qword ptr [rcx+rax*8], rbx",
    "        inc     qword ptr [rt_tcnt]",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "rtk9:   ret",
    "RT_TPUSH endp",
    "",
    "; ---- docasny retezec:  rdi = retezec => rax ----",
    "RT_STEMP proc   near",
    "        push    rdi",
    "        call    RT_TPUSH",
    "        pop     rax",
    "        ret",
    "RT_STEMP endp",
    "",
    "; ---- docasna posloupnost / slovnik:  rdi = hlavicka => rax ----",
    "RT_QTEMP proc   near",
    "        push    rdi",
    "        test    rdi, rdi",
    "        jz      rqt1",
    "        or      rdi, 1",
    "rqt1:   call    RT_TPUSH",
    "        pop     rax",
    "        ret",
    "RT_QTEMP endp",
    "",
    "RT_DTEMP proc   near",
    "        push    rdi",
    "        test    rdi, rdi",
    "        jz      rdtm1",
    "        or      rdi, 2",
    "rdtm1:  call    RT_TPUSH",
    "        pop     rax",
    "        ret",
    "RT_DTEMP endp",
    "",
    "; ---- docasna hodnota uz neni docasna (vlastni ji promenna):  rdi = polozka ----",
    "RT_TSTEAL proc  near",
    "        mov     rcx, qword ptr [rt_tcnt]",
    "        mov     rdx, qword ptr [rt_tbuf]",
    "rtz1:   test    rcx, rcx",
    "        jz      rtz9",
    "        dec     rcx",
    "        cmp     qword ptr [rdx+rcx*8], rdi",
    "        jne     rtz1",
    "        mov     qword ptr [rdx+rcx*8], 0",
    "rtz9:   ret",
    "RT_TSTEAL endp",
    "",
    "; ---- uvolneni docasnych hodnot nad znackou:  rdi = znacka (rax se zachova) ----",
    "RT_TREL proc    near",
    "        push    rax",
    "        push    rbx",
    "        push    r12",
    "        mov     rbx, rdi",
    "rtr1:   mov     rax, qword ptr [rt_tcnt]",
    "        cmp     rax, rbx",
    "        jbe     rtr9",
    "        dec     rax",
    "        mov     qword ptr [rt_tcnt], rax",
    "        mov     rcx, qword ptr [rt_tbuf]",
    "        mov     r12, qword ptr [rcx+rax*8]",
    "        mov     rdi, r12",
    "        and     rdi, -8",
    "        test    r12, 1",
    "        jnz     rtr2",
    "        test    r12, 2",
    "        jnz     rtr3",
    "        call    RT_SDEC",
    "        jmp     rtr1",
    "rtr2:   call    RT_QFREE",
    "        jmp     rtr1",
    "rtr3:   call    RT_DFREE",
    "        jmp     rtr1",
    "rtr9:   pop     r12",
    "        pop     rbx",
    "        pop     rax",
    "        ret",
    "RT_TREL endp",
    "",
    "; ---- spojeni:  rdi = a, rsi = b => a + b ----",
    "RT_SCAT proc    near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        push    r14",
    "        sub     rsp, 8                  ; zarovnani na 16 (glibc)",
    "        mov     r12, rdi",
    "        mov     r13, rsi",
    "        call    RT_SLEN",
    "        mov     rbx, rax                ; delka a",
    "        mov     rdi, r13",
    "        call    RT_SLEN",
    "        mov     r14, rax                ; delka b",
    "        mov     rax, r13",
    "        test    rbx, rbx                ; a prazdny => b",
    "        jz      rsa7",
    "        mov     rax, r12",
    "        test    r14, r14                ; b prazdny => a",
    "        jz      rsa7",
    "        lea     rdi, [rbx+r14]",
    "        call    RT_SNEW",
    "        test    rax, rax",
    "        jz      rsa9",
    "        mov     rdi, rax",
    "        mov     rsi, r12",
    "        mov     rcx, rbx",
    "        rep     movsb",
    "        mov     rsi, r13",
    "        mov     rcx, r14",
    "        rep     movsb",
    "        jmp     rsa9",
    "rsa7:   mov     rdi, rax                ; vysledek je jeden z operandu - reference navic",
    "        call    RT_SINC",
    "rsa9:   add     rsp, 8",
    "        pop     r14",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_SCAT endp",
    "",
    "; ---- retezec z jednoho znaku:  rdi = znak ----",
    "RT_SCHR proc    near",
    "        push    rdi",
    "        mov     edi, 1",
    "        call    RT_SNEW",
    "        pop     rcx",
    "        test    rax, rax",
    "        jz      rsh1",
    "        mov     byte ptr [rax], cl",
    "rsh1:   ret",
    "RT_SCHR endp",
    "",
    "; ---- retezec z char *:  rdi = znaky ukoncene nulou ----",
    "RT_SFROMC proc  near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rsf9",
    "        push    rbx",
    "        push    r12",
    "        sub     rsp, 8                  ; zarovnani na 16 (glibc)",
    "        mov     r12, rdi",
    "        mov     rbx, rdi",
    "rsf1:   cmp     byte ptr [rbx], 0",
    "        je      rsf2",
    "        inc     rbx",
    "        jmp     rsf1",
    "rsf2:   sub     rbx, r12                ; delka",
    "        xor     eax, eax",
    "        test    rbx, rbx",
    "        jz      rsf8",
    "        mov     rdi, rbx",
    "        call    RT_SNEW",
    "        test    rax, rax",
    "        jz      rsf8",
    "        mov     rdi, rax",
    "        mov     rsi, r12",
    "        mov     rcx, rbx",
    "        rep     movsb",
    "rsf8:   add     rsp, 8",
    "        pop     r12",
    "        pop     rbx",
    "rsf9:   ret",
    "RT_SFROMC endp",
    "",
    "; ---- string (p, n):  rdi = znaky, rsi = pocet => novy retezec ----",
    "RT_SFROMN proc  near",
    "        xor     eax, eax",
    "        test    rsi, rsi",
    "        jz      rsy9",
    "        push    rbx",
    "        push    r12",
    "        sub     rsp, 8                  ; zarovnani na 16 (glibc)",
    "        mov     r12, rdi",
    "        mov     rbx, rsi",
    "        mov     rdi, rsi",
    "        call    RT_SNEW",
    "        test    rax, rax",
    "        jz      rsy8",
    "        mov     rdi, rax",
    "        mov     rsi, r12",
    "        mov     rcx, rbx",
    "        rep     movsb",
    "rsy8:   add     rsp, 8",
    "        pop     r12",
    "        pop     rbx",
    "rsy9:   ret",
    "RT_SFROMN endp",
    "",
    "; ---- string (n, c):  rdi = pocet, rsi = znak => novy retezec ----",
    "RT_SREP proc    near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jle     rsw9",
    "        push    rbx",
    "        push    r12",
    "        sub     rsp, 8",
    "        mov     rbx, rdi",
    "        mov     r12, rsi",
    "        call    RT_SNEW",
    "        test    rax, rax",
    "        jz      rsw8",
    "        mov     rdx, rax",
    "        mov     rdi, rax",
    "        mov     rcx, rbx",
    "        mov     eax, r12d",
    "        rep     stosb",
    "        mov     rax, rdx",
    "rsw8:   add     rsp, 8",
    "        pop     r12",
    "        pop     rbx",
    "rsw9:   ret",
    "RT_SREP endp",
    "",
    "; ---- porovnani:  rdi = a, rsi = b => -1, 0, 1 (bajty bez znamenka, potom delka) ----",
    "RT_SCMP proc    near",
    "        push    rbx",
    "        push    r12",
    "        mov     r12, rsi",
    "        call    RT_SLEN",
    "        mov     r8, rax                 ; delka a",
    "        mov     rbx, rdi",
    "        mov     rdi, r12",
    "        call    RT_SLEN",
    "        mov     r9, rax                 ; delka b",
    "        mov     rdi, rbx",
    "        mov     rsi, r12",
    "        mov     rdx, r8                 ; mensi z delek",
    "        cmp     rdx, r9",
    "        jbe     rsm1",
    "        mov     rdx, r9",
    "rsm1:   xor     ecx, ecx",
    "rsm2:   cmp     rcx, rdx",
    "        jae     rsm4",
    "        movzx   eax, byte ptr [rdi+rcx]",
    "        movzx   r10d, byte ptr [rsi+rcx]",
    "        cmp     eax, r10d",
    "        jb      rsm7",
    "        ja      rsm8",
    "        inc     rcx",
    "        jmp     rsm2",
    "rsm4:   cmp     r8, r9",
    "        jb      rsm7",
    "        ja      rsm8",
    "        xor     eax, eax",
    "        jmp     rsm9",
    "rsm7:   mov     rax, -1",
    "        jmp     rsm9",
    "rsm8:   mov     eax, 1",
    "rsm9:   pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_SCMP endp",
    "",
    "; ---- podretezec:  rdi = s, rsi = pos, rdx = n => s.substr (pos, n) ----",
    "; (pos >= delka => prazdny retezec, n se zkrati na zbytek retezce)",
    "RT_SSUB proc    near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     r12, rdi",
    "        call    RT_SLEN                 ; rax = delka",
    "        cmp     rsi, rax",
    "        jae     rsu8                    ; pos >= delka => \"\"",
    "        sub     rax, rsi                ; zbytek",
    "        cmp     rdx, rax",
    "        jbe     rsu1",
    "        mov     rdx, rax",
    "rsu1:   test    rdx, rdx",
    "        jz      rsu8",
    "        lea     r13, [r12+rsi]          ; zdroj",
    "        mov     rbx, rdx                ; delka vysledku",
    "        mov     rdi, rdx",
    "        call    RT_SNEW",
    "        test    rax, rax",
    "        jz      rsu9",
    "        mov     rdi, rax",
    "        mov     rsi, r13",
    "        mov     rcx, rbx",
    "        rep     movsb",
    "        jmp     rsu9",
    "rsu8:   xor     eax, eax",
    "rsu9:   pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_SSUB endp",
    "",
    "; ---- zmena znaku:  rdi = s, rsi = i, rdx = c => kopie s se znakem c na pozici i ----",
    "; (retezce se sdileji - zapis s [i] = c vytvori novy retezec; i >= delka => beze zmeny)",
    "RT_SSET proc    near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        push    r14",
    "        sub     rsp, 8                  ; zarovnani na 16 (glibc)",
    "        mov     r12, rdi",
    "        mov     r13, rsi",
    "        mov     r14, rdx",
    "        call    RT_SLEN",
    "        mov     rbx, rax                ; delka",
    "        mov     rax, r12",
    "        cmp     r13, rbx",
    "        jae     rss7",
    "        mov     rdi, rbx",
    "        call    RT_SNEW",
    "        test    rax, rax",
    "        jz      rss9",
    "        mov     rdi, rax",
    "        mov     rsi, r12",
    "        mov     rcx, rbx",
    "        rep     movsb",
    "        mov     rcx, r14",
    "        mov     byte ptr [rax+r13], cl",
    "        jmp     rss9",
    "rss7:   mov     rdi, rax                ; beze zmeny - reference navic",
    "        call    RT_SINC",
    "rss9:   add     rsp, 8",
    "        pop     r14",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_SSET endp",
    "",
    "; ==== Sequence <T> ====",
    "; hodnota = ukazatel na hlavicku: [0] pocet, [8] kapacita, [16] prvky",
    "; (8bajtove sloty), [24] druh prvku (1 = std::string - reference);",
    "; 0 = prazdna posloupnost; zmeny dostavaji adresu promenne; posloupnost",
    "; vlastni jen jedna promenna (hodnotova semantika) - RT_QFREE",
    "",
    "; ---- hlavicka:  rdi = adresa promenne, rsi = druh prvku => rax = hlavicka ----",
    "RT_QHANDLE proc near",
    "        mov     rax, qword ptr [rdi]",
    "        test    rax, rax",
    "        jnz     rqh9",
    "        push    rdi",
    "        push    rsi",
    "        sub     rsp, 8",
    "        mov     edi, 1",
    "        mov     esi, 32",
    "        call    RT_CALLOC",
    "        add     rsp, 8",
    "        pop     rsi",
    "        pop     rdi",
    "        mov     qword ptr [rdi], rax",
    "        mov     qword ptr [rax+24], rsi",
    "rqh9:   ret",
    "RT_QHANDLE endp",
    "",
    "; ---- uvolneni posloupnosti:  rdi = hlavicka ----",
    "RT_QFREE proc   near",
    "        test    rdi, rdi",
    "        jz      rqx9",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdi",
    "        cmp     qword ptr [rbx+24], 1",
    "        jne     rqx2",
    "        xor     r12d, r12d",
    "rqx1:   cmp     r12, qword ptr [rbx]",
    "        jae     rqx2",
    "        mov     rax, qword ptr [rbx+16]",
    "        mov     rdi, qword ptr [rax+r12*8]",
    "        call    RT_SDEC",
    "        inc     r12",
    "        jmp     rqx1",
    "rqx2:   mov     rdi, qword ptr [rbx+16]",
    "        test    rdi, rdi",
    "        jz      rqx3",
    "        call    RT_FREE",
    "rqx3:   mov     rdi, rbx",
    "        call    RT_FREE",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "rqx9:   ret",
    "RT_QFREE endp",
    "",
    "; ---- zvetseni pole prvku:  rdi = hlavicka ----",
    "RT_QGROW proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdi",
    "        mov     r12, qword ptr [rbx+8]",
    "        add     r12, r12",
    "        jnz     rqg1",
    "        mov     r12d, 8",
    "rqg1:   mov     rdi, r12",
    "        mov     esi, 8",
    "        call    RT_CALLOC",
    "        mov     r13, rax",
    "        mov     rsi, qword ptr [rbx+16]",
    "        test    rsi, rsi",
    "        jz      rqg2",
    "        mov     rdi, r13",
    "        mov     rcx, qword ptr [rbx]",
    "        rep     movsq",
    "        mov     rdi, qword ptr [rbx+16]",
    "        call    RT_FREE",
    "rqg2:   mov     qword ptr [rbx+16], r13",
    "        mov     qword ptr [rbx+8], r12",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_QGROW endp",
    "",
    "; ---- add:  rdi = adresa promenne, rsi = hodnota (vlastnena), rdx = druh prvku ----",
    "RT_QADD proc    near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     r12, rsi",
    "        mov     rsi, rdx",
    "        call    RT_QHANDLE",
    "        mov     r13, rax",
    "        mov     rax, qword ptr [r13]",
    "        cmp     rax, qword ptr [r13+8]",
    "        jb      rqa1",
    "        mov     rdi, r13",
    "        call    RT_QGROW",
    "rqa1:   mov     rax, qword ptr [r13]",
    "        mov     rcx, qword ptr [r13+16]",
    "        mov     qword ptr [rcx+rax*8], r12",
    "        inc     qword ptr [r13]",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_QADD endp",
    "",
    "; ---- insertFirst:  rdi = adresa promenne, rsi = hodnota, rdx = druh prvku ----",
    "RT_QINSF proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     r12, rsi",
    "        mov     rsi, rdx",
    "        call    RT_QHANDLE",
    "        mov     r13, rax",
    "        mov     rax, qword ptr [r13]",
    "        cmp     rax, qword ptr [r13+8]",
    "        jb      rqi1",
    "        mov     rdi, r13",
    "        call    RT_QGROW",
    "rqi1:   mov     rcx, qword ptr [r13]",
    "        mov     r8, qword ptr [r13+16]",
    "rqi2:   test    rcx, rcx",
    "        jz      rqi3",
    "        mov     rax, qword ptr [r8+rcx*8-8]",
    "        mov     qword ptr [r8+rcx*8], rax",
    "        dec     rcx",
    "        jmp     rqi2",
    "rqi3:   mov     qword ptr [r8], r12",
    "        inc     qword ptr [r13]",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_QINSF endp",
    "",
    "; ---- adresa prvku:  rdi = hlavicka, rsi = index => rax (mimo rozsah => assert) ----",
    "RT_QREF proc    near",
    "        test    rdi, rdi",
    "        jz      rqr8",
    "        cmp     rsi, qword ptr [rdi]",
    "        jae     rqr8",
    "        mov     rax, qword ptr [rdi+16]",
    "        lea     rax, [rax+rsi*8]",
    "        ret",
    "rqr8:   lea     rdi, rt_qmsg",
    "        jmp     RT_ASSERT",
    "RT_QREF endp",
    "",
    "; ---- pocet prvku:  rdi = hlavicka ----",
    "RT_QSIZE proc   near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rqs9",
    "        mov     rax, qword ptr [rdi]",
    "rqs9:   ret",
    "RT_QSIZE endp",
    "",
    "; ---- clear:  rdi = adresa promenne ----",
    "RT_QCLEAR proc  near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, qword ptr [rdi]",
    "        test    rbx, rbx",
    "        jz      rqz9",
    "        cmp     qword ptr [rbx+24], 1",
    "        jne     rqz2",
    "        xor     r12d, r12d",
    "rqz1:   cmp     r12, qword ptr [rbx]",
    "        jae     rqz2",
    "        mov     rax, qword ptr [rbx+16]",
    "        mov     rdi, qword ptr [rax+r12*8]",
    "        mov     qword ptr [rax+r12*8], 0",
    "        call    RT_SDEC",
    "        inc     r12",
    "        jmp     rqz1",
    "rqz2:   mov     qword ptr [rbx], 0",
    "rqz9:   pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_QCLEAR endp",
    "",
    "; ---- pop:  rdi = adresa promenne (prazdna => assert) ----",
    "RT_QPOP proc    near",
    "        mov     rax, qword ptr [rdi]",
    "        test    rax, rax",
    "        jz      rqp8",
    "        cmp     qword ptr [rax], 0",
    "        je      rqp8",
    "        dec     qword ptr [rax]",
    "        cmp     qword ptr [rax+24], 1",
    "        jne     rqp7",
    "        mov     rcx, qword ptr [rax]",
    "        mov     rdx, qword ptr [rax+16]",
    "        mov     rdi, qword ptr [rdx+rcx*8]",
    "        mov     qword ptr [rdx+rcx*8], 0",
    "        sub     rsp, 8",
    "        call    RT_SDEC",
    "        add     rsp, 8",
    "rqp7:   ret",
    "rqp8:   lea     rdi, rt_qmsg",
    "        jmp     RT_ASSERT",
    "RT_QPOP endp",
    "",
    "; ---- kopie:  rdi = hlavicka => rax = nova hlavicka (hodnotova semantika) ----",
    "RT_QCOPY proc   near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rqc9",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdi",
    "        mov     edi, 1",
    "        mov     esi, 32",
    "        call    RT_CALLOC",
    "        mov     r12, rax",
    "        mov     rcx, qword ptr [rbx+24]",
    "        mov     qword ptr [r12+24], rcx ; druh prvku",
    "        mov     r13, qword ptr [rbx]",
    "        test    r13, r13",
    "        jz      rqc8",
    "        mov     qword ptr [r12], r13",
    "        mov     qword ptr [r12+8], r13",
    "        mov     rdi, r13",
    "        mov     esi, 8",
    "        call    RT_CALLOC",
    "        mov     qword ptr [r12+16], rax",
    "        mov     rdi, rax",
    "        mov     rsi, qword ptr [rbx+16]",
    "        mov     rcx, r13",
    "        rep     movsq",
    "        cmp     qword ptr [r12+24], 1   ; retezce - reference navic",
    "        jne     rqc8",
    "        mov     rbx, qword ptr [r12+16]",
    "rqc7:   test    r13, r13",
    "        jz      rqc8",
    "        dec     r13",
    "        mov     rdi, qword ptr [rbx+r13*8]",
    "        call    RT_SINC",
    "        jmp     rqc7",
    "rqc8:   mov     rax, r12",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "rqc9:   ret",
    "RT_QCOPY endp",
    "",
    "; ---- contains:  rdi = hlavicka, rsi = hodnota, rdx = druh (0 cislo / ukazatel,",
    ";      1 std::string, 2 double, 3 float) => rax = 0 / 1 ----",
    "RT_QFIND proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        push    r14",
    "        push    r15",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rqf9",
    "        mov     rbx, qword ptr [rdi+16]",
    "        mov     r12, qword ptr [rdi]",
    "        mov     r13, rsi",
    "        mov     r14, rdx",
    "        xor     r15d, r15d",
    "rqf1:   cmp     r15, r12",
    "        jae     rqf8",
    "        mov     rdi, qword ptr [rbx+r15*8]",
    "        cmp     r14, 1",
    "        je      rqf3",
    "        cmp     r14, 2",
    "        je      rqf4",
    "        cmp     r14, 3",
    "        je      rqf5",
    "        cmp     rdi, r13",
    "        je      rqf7",
    "        jmp     rqf6",
    "rqf3:   mov     rsi, r13",
    "        call    RT_SCMP",
    "        test    rax, rax",
    "        jz      rqf7",
    "        jmp     rqf6",
    "rqf4:   movq    xmm0, rdi",
    "        movq    xmm1, r13",
    "        ucomisd xmm0, xmm1",
    "        jp      rqf6",
    "        je      rqf7",
    "        jmp     rqf6",
    "rqf5:   movd    xmm0, edi",
    "        movd    xmm1, r13d",
    "        ucomiss xmm0, xmm1",
    "        jp      rqf6",
    "        je      rqf7",
    "rqf6:   inc     r15",
    "        jmp     rqf1",
    "rqf7:   mov     eax, 1",
    "        jmp     rqf9",
    "rqf8:   xor     eax, eax",
    "rqf9:   pop     r15",
    "        pop     r14",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_QFIND endp",
    "",
    "; ==== Dictionary <K, V> ====",
    "; hodnota = ukazatel na hlavicku: [0] pocet, [8] kapacita (2^n), [16] polozky,",
    "; [24] druh klice (0 cislo / ukazatel, 1 std::string), [32] druh hodnoty",
    "; (1 = std::string - reference); 0 = prazdny slovnik; RT_DFREE",
    "; polozka 24 bajtu: [0] obsazena (0 / 1), [8] klic, [16] hodnota",
    "; otevrene adresovani (linearni pruzkum), zvetseni pri zaplneni 3/4",
    "",
    "; ---- hash:  rdi = klic, rsi = druh => rax ----",
    "RT_DHASH proc   near",
    "        test    rsi, rsi",
    "        jnz     rdh2",
    "        mov     rax, rdi",
    "        mov     rcx, rax",
    "        shr     rcx, 33",
    "        xor     rax, rcx",
    "        mov     rcx, 0FF51AFD7ED558CCDh",
    "        imul    rax, rcx",
    "        mov     rcx, rax",
    "        shr     rcx, 33",
    "        xor     rax, rcx",
    "        ret",
    "rdh2:   mov     rax, 0CBF29CE484222325h ; FNV-1a",
    "        test    rdi, rdi",
    "        jz      rdh9",
    "        mov     rdx, qword ptr [rdi-8]  ; delka retezce",
    "        mov     r8, 100000001B3h",
    "        xor     ecx, ecx",
    "rdh3:   cmp     rcx, rdx",
    "        jae     rdh9",
    "        movzx   r9d, byte ptr [rdi+rcx]",
    "        xor     rax, r9",
    "        imul    rax, r8",
    "        inc     rcx",
    "        jmp     rdh3",
    "rdh9:   ret",
    "RT_DHASH endp",
    "",
    "; ---- hledani:  rdi = hlavicka, rsi = klic => rax = polozka nebo 0 ----",
    "RT_DFIND proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        push    r14",
    "        push    r15",
    "        mov     rbx, rdi",
    "        mov     r12, rsi",
    "        xor     eax, eax",
    "        cmp     qword ptr [rbx+8], 0",
    "        je      rdf9",
    "        mov     rdi, r12",
    "        mov     rsi, qword ptr [rbx+24]",
    "        call    RT_DHASH",
    "        mov     r13, qword ptr [rbx+8]",
    "        dec     r13                     ; maska",
    "        and     rax, r13",
    "        mov     r14, rax                ; index",
    "rdf1:   imul    rax, r14, 24",
    "        mov     r15, qword ptr [rbx+16]",
    "        add     r15, rax",
    "        cmp     qword ptr [r15], 0",
    "        je      rdf8                    ; prazdna polozka => klic neni",
    "        mov     rdi, qword ptr [r15+8]",
    "        cmp     qword ptr [rbx+24], 1",
    "        je      rdf3",
    "        cmp     rdi, r12",
    "        je      rdf7",
    "        jmp     rdf4",
    "rdf3:   mov     rsi, r12",
    "        call    RT_SCMP",
    "        test    rax, rax",
    "        jz      rdf7",
    "rdf4:   inc     r14",
    "        and     r14, r13",
    "        jmp     rdf1",
    "rdf7:   mov     rax, r15",
    "        jmp     rdf9",
    "rdf8:   xor     eax, eax",
    "rdf9:   pop     r15",
    "        pop     r14",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_DFIND endp",
    "",
    "; ---- volna polozka pro klic:  rdi = hlavicka, rsi = klic => rax ----",
    "RT_DSLOT proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdi",
    "        mov     rdi, rsi",
    "        mov     rsi, qword ptr [rbx+24]",
    "        call    RT_DHASH",
    "        mov     r13, qword ptr [rbx+8]",
    "        dec     r13",
    "        and     rax, r13",
    "        mov     rcx, rax",
    "rds1:   imul    rax, rcx, 24",
    "        add     rax, qword ptr [rbx+16]",
    "        cmp     qword ptr [rax], 0",
    "        je      rds9",
    "        inc     rcx",
    "        and     rcx, r13",
    "        jmp     rds1",
    "rds9:   pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_DSLOT endp",
    "",
    "; ---- zvetseni tabulky:  rdi = hlavicka ----",
    "RT_DGROW proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        push    r14",
    "        push    r15",
    "        mov     rbx, rdi",
    "        mov     r12, qword ptr [rbx+16] ; stare polozky",
    "        mov     r13, qword ptr [rbx+8]  ; stara kapacita",
    "        lea     r14, [r13+r13]",
    "        test    r14, r14",
    "        jnz     rdg1",
    "        mov     r14d, 16",
    "rdg1:   mov     rdi, r14",
    "        mov     esi, 24",
    "        call    RT_CALLOC",
    "        mov     qword ptr [rbx+16], rax",
    "        mov     qword ptr [rbx+8], r14",
    "        xor     r15d, r15d",
    "rdg2:   cmp     r15, r13",
    "        jae     rdg5",
    "        imul    rax, r15, 24",
    "        add     rax, r12",
    "        cmp     qword ptr [rax], 0",
    "        je      rdg4",
    "        mov     r14, rax",
    "        mov     rdi, rbx",
    "        mov     rsi, qword ptr [r14+8]",
    "        call    RT_DSLOT",
    "        mov     qword ptr [rax], 1",
    "        mov     rcx, qword ptr [r14+8]",
    "        mov     qword ptr [rax+8], rcx",
    "        mov     rcx, qword ptr [r14+16]",
    "        mov     qword ptr [rax+16], rcx",
    "rdg4:   inc     r15",
    "        jmp     rdg2",
    "rdg5:   test    r12, r12",
    "        jz      rdg9",
    "        mov     rdi, r12",
    "        call    RT_FREE",
    "rdg9:   pop     r15",
    "        pop     r14",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_DGROW endp",
    "",
    "; ---- adresa hodnoty:  rdi = adresa promenne, rsi = klic, rdx = druh klice,",
    ";      rcx = 1 vlozit chybejici klic (hodnota 0) / 0 jen hledat, r8 = druh",
    ";      hodnoty => rax (0 = neni) ----",
    "RT_DREF proc    near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        push    r14",
    "        push    r15",
    "        mov     rbx, rdi",
    "        mov     r12, rsi",
    "        mov     r13, rdx",
    "        mov     r14, rcx",
    "        mov     r15, qword ptr [rbx]",
    "        test    r15, r15",
    "        jnz     rdr2",
    "        xor     eax, eax",
    "        test    r14, r14",
    "        jz      rdr9",
    "        push    r8",
    "        sub     rsp, 8",
    "        mov     edi, 1",
    "        mov     esi, 40",
    "        call    RT_CALLOC",
    "        add     rsp, 8",
    "        pop     r8",
    "        mov     r15, rax",
    "        mov     qword ptr [rbx], r15",
    "        mov     qword ptr [r15+24], r13",
    "        mov     qword ptr [r15+32], r8",
    "rdr2:   mov     rdi, r15",
    "        mov     rsi, r12",
    "        call    RT_DFIND",
    "        test    rax, rax",
    "        jz      rdr3",
    "        add     rax, 16",
    "        jmp     rdr9",
    "rdr3:   test    r14, r14",
    "        jz      rdr9",
    "        mov     rax, qword ptr [r15]",
    "        inc     rax",
    "        shl     rax, 2                  ; (pocet + 1) * 4",
    "        mov     rcx, qword ptr [r15+8]",
    "        lea     rcx, [rcx+rcx*2]        ; kapacita * 3",
    "        cmp     rax, rcx",
    "        jbe     rdr4",
    "        mov     rdi, r15",
    "        call    RT_DGROW",
    "rdr4:   mov     rdi, r15",
    "        mov     rsi, r12",
    "        call    RT_DSLOT",
    "        mov     qword ptr [rax], 1",
    "        mov     qword ptr [rax+8], r12",
    "        mov     qword ptr [rax+16], 0",
    "        inc     qword ptr [r15]",
    "        cmp     qword ptr [r15+24], 1   ; klic std::string - reference",
    "        jne     rdr5",
    "        push    rax",
    "        sub     rsp, 8",
    "        mov     rdi, r12",
    "        call    RT_SINC",
    "        add     rsp, 8",
    "        pop     rax",
    "rdr5:   add     rax, 16",
    "rdr9:   pop     r15",
    "        pop     r14",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_DREF endp",
    "",
    "; ---- get:  rdi = hlavicka, rsi = klic => hodnota (chybejici klic => 0) ----",
    "RT_DGET proc    near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rdt9",
    "        call    RT_DFIND",
    "        test    rax, rax",
    "        jz      rdt9",
    "        mov     rax, qword ptr [rax+16]",
    "rdt9:   ret",
    "RT_DGET endp",
    "",
    "; ---- contains:  rdi = hlavicka, rsi = klic => 0 / 1 ----",
    "RT_DHAS proc    near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rdk9",
    "        call    RT_DFIND",
    "        test    rax, rax",
    "        setne   al",
    "        movzx   eax, al",
    "rdk9:   ret",
    "RT_DHAS endp",
    "",
    "; ---- size:  rdi = hlavicka ----",
    "RT_DSIZE proc   near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rdz9",
    "        mov     rax, qword ptr [rdi]",
    "rdz9:   ret",
    "RT_DSIZE endp",
    "",
    "; ---- odebrani referenci klicu a hodnot (std::string):  rdi = hlavicka ----",
    "RT_DREFS proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdi",
    "        xor     r12d, r12d",
    "rdq1:   cmp     r12, qword ptr [rbx+8]",
    "        jae     rdq9",
    "        imul    r13, r12, 24",
    "        add     r13, qword ptr [rbx+16]",
    "        cmp     qword ptr [r13], 0",
    "        je      rdq3",
    "        cmp     qword ptr [rbx+24], 1",
    "        jne     rdq2",
    "        mov     rdi, qword ptr [r13+8]",
    "        call    RT_SDEC",
    "rdq2:   cmp     qword ptr [rbx+32], 1",
    "        jne     rdq3",
    "        mov     rdi, qword ptr [r13+16]",
    "        call    RT_SDEC",
    "rdq3:   inc     r12",
    "        jmp     rdq1",
    "rdq9:   pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_DREFS endp",
    "",
    "; ---- uvolneni slovniku:  rdi = hlavicka ----",
    "RT_DFREE proc   near",
    "        test    rdi, rdi",
    "        jz      rdfr9",
    "        push    rbx",
    "        mov     rbx, rdi",
    "        call    RT_DREFS",
    "        mov     rdi, qword ptr [rbx+16]",
    "        test    rdi, rdi",
    "        jz      rdfr2",
    "        call    RT_FREE",
    "rdfr2:  mov     rdi, rbx",
    "        call    RT_FREE",
    "        pop     rbx",
    "rdfr9:  ret",
    "RT_DFREE endp",
    "",
    "; ---- clear:  rdi = adresa promenne ----",
    "RT_DCLEAR proc  near",
    "        mov     rdx, qword ptr [rdi]",
    "        test    rdx, rdx",
    "        jz      rdc9",
    "        push    rdx",
    "        mov     rdi, rdx",
    "        call    RT_DREFS",
    "        pop     rdx",
    "        mov     qword ptr [rdx], 0",
    "        mov     rdi, qword ptr [rdx+16]",
    "        test    rdi, rdi",
    "        jz      rdc9",
    "        mov     rcx, qword ptr [rdx+8]",
    "        lea     rcx, [rcx+rcx*2]",
    "        xor     eax, eax",
    "        rep     stosq",
    "rdc9:   ret",
    "RT_DCLEAR endp",
    "",
    "; ---- reference navic pro klice a hodnoty (std::string):  rdi = hlavicka ----",
    "RT_DINCS proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdi",
    "        xor     r12d, r12d",
    "rdn_1:  cmp     r12, qword ptr [rbx+8]",
    "        jae     rdn_9",
    "        imul    r13, r12, 24",
    "        add     r13, qword ptr [rbx+16]",
    "        cmp     qword ptr [r13], 0",
    "        je      rdn_3",
    "        cmp     qword ptr [rbx+24], 1",
    "        jne     rdn_2",
    "        mov     rdi, qword ptr [r13+8]",
    "        call    RT_SINC",
    "rdn_2:  cmp     qword ptr [rbx+32], 1",
    "        jne     rdn_3",
    "        mov     rdi, qword ptr [r13+16]",
    "        call    RT_SINC",
    "rdn_3:  inc     r12",
    "        jmp     rdn_1",
    "rdn_9:  pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_DINCS endp",
    "",
    "; ---- kopie:  rdi = hlavicka => rax = nova hlavicka ----",
    "RT_DCOPY proc   near",
    "        xor     eax, eax",
    "        test    rdi, rdi",
    "        jz      rdy9",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdi",
    "        mov     edi, 1",
    "        mov     esi, 40",
    "        call    RT_CALLOC",
    "        mov     r12, rax",
    "        mov     rcx, qword ptr [rbx]",
    "        mov     qword ptr [r12], rcx",
    "        mov     rcx, qword ptr [rbx+8]",
    "        mov     qword ptr [r12+8], rcx",
    "        mov     rcx, qword ptr [rbx+24]",
    "        mov     qword ptr [r12+24], rcx",
    "        mov     rcx, qword ptr [rbx+32]",
    "        mov     qword ptr [r12+32], rcx",
    "        mov     r13, qword ptr [rbx+8]",
    "        test    r13, r13",
    "        jz      rdy8",
    "        mov     rdi, r13",
    "        mov     esi, 24",
    "        call    RT_CALLOC",
    "        mov     qword ptr [r12+16], rax",
    "        mov     rdi, rax",
    "        mov     rsi, qword ptr [rbx+16]",
    "        lea     rcx, [r13+r13*2]",
    "        rep     movsq",
    "        mov     rdi, r12",
    "        call    RT_DINCS                ; reference klicu a hodnot (std::string)",
    "rdy8:   mov     rax, r12",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "rdy9:   ret",
    "RT_DCOPY endp",
    "",
    "; ---- dynamic_cast:  rdi = tabulka objektu, rsi = tabulka cilove tridy => 0 / 1",
    "; retez [vt - 8]; shoda adresy, nebo jmena tridy [vt - 16] (tabulky jsou",
    "; v kazdem objektu zvlast) ----",
    "RT_DCAST proc   near",
    "        mov     r8, qword ptr [rsi-16]  ; jmeno cilove tridy",
    "rdn1:   test    rdi, rdi",
    "        jz      rdn8",
    "        cmp     rdi, rsi",
    "        je      rdn7",
    "        mov     r9, qword ptr [rdi-16]",
    "        xor     ecx, ecx",
    "rdn2:   movzx   eax, byte ptr [r8+rcx]",
    "        movzx   edx, byte ptr [r9+rcx]",
    "        cmp     eax, edx",
    "        jne     rdn3",
    "        test    eax, eax",
    "        jz      rdn7",
    "        inc     rcx",
    "        jmp     rdn2",
    "rdn3:   mov     rdi, qword ptr [rdi-8]",
    "        jmp     rdn1",
    "rdn7:   mov     eax, 1",
    "        ret",
    "rdn8:   xor     eax, eax",
    "        ret",
    "RT_DCAST endp",
    "",
    "; ---- spolecny zacatek find / rfind:  rdi = s, rsi = t ----",
    "; => r8 = delka s, r9 = delka t, r10 = znaky s, r11 = znaky t",
    "RT_SFPREP proc  near",
    "        push    rdi",
    "        push    rsi",
    "        call    RT_SLEN",
    "        mov     r8, rax",
    "        mov     rdi, qword ptr [rsp]",
    "        call    RT_SLEN",
    "        mov     r9, rax",
    "        mov     rdi, qword ptr [rsp+8]",
    "        call    RT_SCSTR",
    "        mov     r10, rax",
    "        mov     rdi, qword ptr [rsp]",
    "        call    RT_SCSTR",
    "        mov     r11, rax",
    "        pop     rsi",
    "        pop     rdi",
    "        ret",
    "RT_SFPREP endp",
    "",
    "; ---- hledani:  rdi = s, rsi = t, rdx = pos => s.find (t, pos), npos = -1 ----",
    "RT_SFIND proc   near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdx                ; pos",
    "        call    RT_SFPREP",
    "        cmp     rbx, r8",
    "        ja      rsd8                    ; pos > delka => npos",
    "        mov     rcx, rbx",
    "rsd1:   lea     rax, [rcx+r9]",
    "        cmp     rax, r8",
    "        ja      rsd8",
    "        lea     r12, [r10+rcx]",
    "        xor     edx, edx",
    "rsd2:   cmp     rdx, r9",
    "        jae     rsd7                    ; nalezeno",
    "        movzx   eax, byte ptr [r12+rdx]",
    "        movzx   r13d, byte ptr [r11+rdx]",
    "        cmp     eax, r13d",
    "        jne     rsd3",
    "        inc     rdx",
    "        jmp     rsd2",
    "rsd3:   inc     rcx",
    "        jmp     rsd1",
    "rsd7:   mov     rax, rcx",
    "        jmp     rsd9",
    "rsd8:   mov     rax, -1",
    "rsd9:   pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_SFIND endp",
    "",
    "; ---- hledani od konce:  rdi = s, rsi = t, rdx = pos => s.rfind (t, pos) ----",
    "RT_SRFIND proc  near",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        mov     rbx, rdx                ; pos",
    "        call    RT_SFPREP",
    "        cmp     r9, r8",
    "        ja      rsr8                    ; t delsi nez s => npos",
    "        mov     rcx, r8",
    "        sub     rcx, r9                 ; posledni mozny zacatek",
    "        cmp     rbx, rcx",
    "        jae     rsr1",
    "        mov     rcx, rbx",
    "rsr1:   lea     r12, [r10+rcx]",
    "        xor     edx, edx",
    "rsr2:   cmp     rdx, r9",
    "        jae     rsr7                    ; nalezeno",
    "        movzx   eax, byte ptr [r12+rdx]",
    "        movzx   r13d, byte ptr [r11+rdx]",
    "        cmp     eax, r13d",
    "        jne     rsr3",
    "        inc     rdx",
    "        jmp     rsr2",
    "rsr3:   test    rcx, rcx",
    "        jz      rsr8",
    "        dec     rcx",
    "        jmp     rsr1",
    "rsr7:   mov     rax, rcx",
    "        jmp     rsr9",
    "rsr8:   mov     rax, -1",
    "rsr9:   pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "RT_SRFIND endp",
    "",
    NULL
};

/* Behova knihovna nad glibc (vychozi): main je obal, ktery zavola konstruktory
   globalnich objektu a funkci main programu; program se slinkuje gcc. */
static const char * RTL_LIBC_DATA [] =
{
    "",
    "; ---- behova knihovna (glibc): data ----",
    "rt_afmt  db      'Assertion failed in function '     ; 29 znaku",
    "rt_lf    db      10",
    "rt_pmsg  db      'Pure virtual method called', 10, 0",
    "rt_envname db    'CPARSER_RESULT', 0",
    NULL
};

static const char * RTL_LIBC_CODE [] =
{
    "",
    "; ---- behova knihovna (glibc): kod ----",
    "",
    "; ---- new:  calloc (n, size) z glibc ----",
    "RT_CALLOC proc  near",
    "        jmp     LC_calloc",
    "RT_CALLOC endp",
    "",
    "; ---- delete:  free (p) z glibc ----",
    "RT_FREE proc    near",
    "        jmp     LC_free",
    "RT_FREE endp",
    "",
    "; ---- nesplneny assert:  rdi = jmeno funkce ----",
    "; fflush (stdout) a write - ne fflush (NULL) a dprintf: libc ToaruOS",
    "; dprintf nema a fflush (NULL) v ni pada",
    "RT_ASSERT proc  near",
    "        push    rbx                     ; rsp zarovnan na 16",
    "        mov     rbx, rdi",
    "        mov     rdi, qword ptr [LC_stdout]",
    "        call    LC_fflush               ; fflush (stdout)",
    "        mov     edi, 2                  ; write (2, 'Assertion ...', 29)",
    "        lea     rsi, rt_afmt",
    "        mov     edx, 29",
    "        call    LC_write",
    "        mov     rdi, rbx",
    "        call    LC_strlen",
    "        mov     rdx, rax                ; write (2, jmeno, strlen (jmeno))",
    "        mov     rsi, rbx",
    "        mov     edi, 2",
    "        call    LC_write",
    "        mov     edi, 2                  ; write (2, '\\n', 1)",
    "        lea     rsi, rt_lf",
    "        mov     edx, 1",
    "        call    LC_write",
    "        call    LC_abort",
    "RT_ASSERT endp",
    "",
    "; ---- volani ciste virtualni metody ----",
    "RT_PURE proc    near",
    "        sub     rsp, 8",
    "        mov     rdi, qword ptr [LC_stdout]",
    "        call    LC_fflush               ; fflush (stdout)",
    "        mov     edi, 2                  ; write (2, rt_pmsg, 27)",
    "        lea     rsi, rt_pmsg",
    "        mov     edx, 27",
    "        call    LC_write",
    "        call    LC_abort",
    "RT_PURE endp",
    NULL
};

/* obal main (jen v souboru, ktery main definuje) */
static const char * RTL_LIBC_MAIN [] =
{
    "",
    "; main (argc, argv): main programu (konstruktory a destruktory globalnich",
    "; objektu: .init_array kazdeho objektu - CINIT, ten registruje CFINI pres atexit);",
    "; je-li nastavena promenna prostredi CPARSER_RESULT (spousti cparser),",
    "; zapise se vysledek main (64 bitu) do deskriptoru 3",
    "CMAIN:  push    rbx",
    "        push    r12",
    "        push    r13                     ; rsp zarovnan na 16",
    "        mov     r12, rdi",
    "        mov     r13, rsi",
    "        mov     rdi, r12",
    "        mov     rsi, r13",
    "        call    FMAIN",
    "        mov     rbx, rax",
    "        lea     rdi, rt_envname",
    "        call    LC_getenv",
    "        test    rax, rax",
    "        jz      cm1",
    "        sub     rsp, 16",
    "        mov     qword ptr [rsp], rbx",
    "        mov     edi, 3                  ; write (3, &vysledek, 8)",
    "        mov     rsi, rsp",
    "        mov     edx, 8",
    "        call    LC_write",
    "        add     rsp, 16",
    "cm1:    mov     rax, rbx",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        ret",
    "",
    NULL
};

/* funkce glibc pouzite behovou knihovnou: jmeno -> navesti */
static const char * RTL_LIBC_EXTERNS [] =
{
    "getenv", "write", "calloc", "free", "fflush", "strlen", "stdout", "abort", "atexit", NULL
};

static const char * RTL_CODE [] =
{
    "",
    "; ---- behova knihovna: kod ----",
    "",
    "; vstupni bod: konstruktory globalnich objektu, main (1, argv),",
    "; vysledek do deskriptoru 3, exit (vysledek)",
    "start:  call    CINIT",
    "        mov     edi, 1",
    "        lea     rsi, rt_argv",
    "        call    FMAIN",
    "        push    rax",
    "        call    CFINI                   ; destruktory globalnich objektu",
    "        call    RT_FLUSH",
    "        mov     eax, 1                  ; write (3, [rsp], 8)",
    "        mov     edi, 3",
    "        mov     rsi, rsp",
    "        mov     edx, 8",
    "        syscall",
    "        pop     rdi",
    "        mov     eax, 60                 ; exit (main)",
    "        syscall",
    "",
    "; ---- vypis bufferu (zachova vsechny registry krome rax) ----",
    "RT_FLUSH proc   near",
    "        push    rdi",
    "        push    rsi",
    "        push    rdx",
    "        push    rcx",
    "        push    r11",
    "        mov     edx, dword ptr [rt_olen]",
    "        test    edx, edx",
    "        jz      rtf9",
    "        mov     eax, 1                  ; write (1, rt_obuf, rt_olen)",
    "        mov     edi, 1",
    "        lea     rsi, rt_obuf",
    "        syscall",
    "        mov     dword ptr [rt_olen], 0",
    "rtf9:   pop     r11",
    "        pop     rcx",
    "        pop     rdx",
    "        pop     rsi",
    "        pop     rdi",
    "        ret",
    "RT_FLUSH endp",
    "",
    "; ---- jeden znak (al) do bufferu, meni rax, rcx, r11 ----",
    "RT_PUTC proc    near",
    "        mov     ecx, dword ptr [rt_olen]",
    "        cmp     ecx, 4096",
    "        jb      rtp1",
    "        push    rax",
    "        call    RT_FLUSH",
    "        pop     rax",
    "        xor     ecx, ecx",
    "rtp1:   lea     r11, rt_obuf",
    "        mov     byte ptr [r11+rcx], al",
    "        inc     ecx",
    "        mov     dword ptr [rt_olen], ecx",
    "        inc     dword ptr [rt_count]",
    "        ret",
    "RT_PUTC endp",
    "",
    "; ---- int putchar (int c) ----",
    "RT_PUTCHAR proc near",
    "        mov     eax, edi",
    "        call    RT_PUTC",
    "        movzx   eax, dil",
    "        ret",
    "RT_PUTCHAR endp",
    "",
    "; ---- int puts (char * s) ----",
    "RT_PUTS proc    near",
    "rts1:   movzx   eax, byte ptr [rdi]",
    "        test    eax, eax",
    "        jz      rts2",
    "        call    RT_PUTC",
    "        inc     rdi",
    "        jmp     rts1",
    "rts2:   mov     eax, 10",
    "        call    RT_PUTC",
    "        xor     eax, eax",
    "        ret",
    "RT_PUTS endp",
    "",
    "; ---- int abs (int x),  long labs (long x) ----",
    "RT_ABS  proc    near",
    "        movsxd  rax, edi",
    "        test    rax, rax",
    "        jns     rta1",
    "        neg     rax",
    "rta1:   ret",
    "RT_ABS  endp",
    "",
    "RT_LABS proc    near",
    "        mov     rax, rdi",
    "        test    rax, rax",
    "        jns     rtb1",
    "        neg     rax",
    "rtb1:   ret",
    "RT_LABS endp",
    "",
    "; ---- int rand (void),  void srand (unsigned s) ----",
    "RT_RAND proc    near",
    "        mov     rax, qword ptr [rt_seed]",
    "        imul    rax, rax, 1103515245",
    "        add     rax, 12345",
    "        mov     qword ptr [rt_seed], rax",
    "        shr     rax, 16",
    "        and     eax, 7FFFh",
    "        ret",
    "RT_RAND endp",
    "",
    "RT_SRAND proc   near",
    "        mov     eax, edi",
    "        mov     qword ptr [rt_seed], rax",
    "        ret",
    "RT_SRAND endp",
    "",
    "; ---- long time (long * t) ----",
    "RT_TIME proc    near",
    "        mov     eax, 201",
    "        syscall",
    "        ret",
    "RT_TIME endp",
    "",
    "; ---- void exit (int code) ----",
    "RT_EXIT proc    near",
    "        push    rdi",
    "        call    RT_FLUSH",
    "        pop     rdi",
    "        mov     eax, 60",
    "        syscall",
    "RT_EXIT endp",
    "",
    "; ---- void * calloc (n, size),  malloc (size),  free (p) ----",
    "; jednoducha halda: pamet se jen pridava (brk), je vynulovana (nove stranky",
    "; od jadra), free nic nedela; nedostatek pameti => 0",
    "RT_MALLOC proc  near",
    "        mov     rsi, rdi",
    "        mov     edi, 1",
    "RT_MALLOC endp",
    "RT_CALLOC proc  near",
    "        imul    rsi, rdi                ; velikost",
    "        add     rsi, 15                 ; zarovnani na 16 bajtu",
    "        and     rsi, -16",
    "        mov     rax, qword ptr [rt_heap]",
    "        test    rax, rax",
    "        jnz     rtc1",
    "        push    rsi",
    "        mov     eax, 12                 ; brk (0) = soucasny konec dat",
    "        xor     edi, edi",
    "        syscall",
    "        pop     rsi",
    "        mov     qword ptr [rt_heap], rax",
    "        mov     qword ptr [rt_hend], rax",
    "rtc1:   mov     rax, qword ptr [rt_heap]",
    "        lea     rdx, [rax+rsi]          ; novy zacatek volne pameti",
    "        cmp     rdx, qword ptr [rt_hend]",
    "        jbe     rtc2",
    "        push    rax",
    "        push    rdx",
    "        lea     rdi, [rdx+65536]        ; brk (konec + 64 KB)",
    "        mov     eax, 12",
    "        syscall",
    "        pop     rdx",
    "        lea     rdi, [rdx+65536]",
    "        cmp     rax, rdi",
    "        jb      rtc9",
    "        mov     qword ptr [rt_hend], rax",
    "        pop     rax",
    "rtc2:   mov     qword ptr [rt_heap], rdx",
    "        ret",
    "rtc9:   pop     rax                     ; nedostatek pameti",
    "        xor     eax, eax",
    "        ret",
    "RT_CALLOC endp",
    "",
    "RT_FREE proc    near",
    "        ret",
    "RT_FREE endp",
    "",
    "; ---- memcpy:  rdi = cil, rsi = zdroj, rdx = pocet => rax = cil ----",
    "RT_MEMCPY proc  near",
    "        mov     rax, rdi",
    "        mov     rcx, rdx",
    "        rep     movsb",
    "        ret",
    "RT_MEMCPY endp",
    "",
    "; ---- memset:  rdi = cil, esi = bajt, rdx = pocet => rax = cil ----",
    "RT_MEMSET proc  near",
    "        mov     r8, rdi",
    "        mov     eax, esi",
    "        mov     rcx, rdx",
    "        rep     stosb",
    "        mov     rax, r8",
    "        ret",
    "RT_MEMSET endp",
    "",
    "; ---- memcmp:  rdi = a, rsi = b, rdx = pocet => rozdil prvnich ruznych bajtu ----",
    "RT_MEMCMP proc  near",
    "        xor     ecx, ecx",
    "rmc1:   cmp     rcx, rdx",
    "        jae     rmc8",
    "        movzx   eax, byte ptr [rdi+rcx]",
    "        movzx   r8d, byte ptr [rsi+rcx]",
    "        sub     eax, r8d",
    "        jnz     rmc9",
    "        inc     rcx",
    "        jmp     rmc1",
    "rmc8:   xor     eax, eax",
    "rmc9:   movsxd  rax, eax",
    "        ret",
    "RT_MEMCMP endp",
    "",
    "; ---- strlen:  rdi = znaky ukoncene nulou ----",
    "RT_STRLEN proc  near",
    "        xor     eax, eax",
    "rsn9:   cmp     byte ptr [rdi+rax], 0",
    "        je      rsn8",
    "        inc     rax",
    "        jmp     rsn9",
    "rsn8:   ret",
    "RT_STRLEN endp",
    "",
    "; ---- retezec (rsi, ukonceny nulou) na standardni chybovy vystup ----",
    "RT_ERRSTR proc  near",
    "        mov     rdx, rsi",
    "rte1:   cmp     byte ptr [rdx], 0",
    "        je      rte2",
    "        inc     rdx",
    "        jmp     rte1",
    "rte2:   sub     rdx, rsi                ; delka",
    "        mov     eax, 1                  ; write (2, rsi, rdx)",
    "        mov     edi, 2",
    "        syscall",
    "        ret",
    "RT_ERRSTR endp",
    "",
    "; ---- ukonceni signalem SIGABRT (jako abort) ----",
    "RT_ABORT proc   near",
    "        mov     eax, 39                 ; getpid",
    "        syscall",
    "        mov     edi, eax",
    "        mov     esi, 6                  ; kill (getpid (), SIGABRT)",
    "        mov     eax, 62",
    "        syscall",
    "        mov     edi, 134",
    "        mov     eax, 60                 ; exit (134)",
    "        syscall",
    "RT_ABORT endp",
    "",
    "; ---- nesplneny assert:  rdi = jmeno funkce ----",
    "RT_ASSERT proc  near",
    "        push    rdi",
    "        call    RT_FLUSH",
    "        lea     rsi, rt_amsg",
    "        call    RT_ERRSTR",
    "        pop     rsi",
    "        call    RT_ERRSTR",
    "        lea     rsi, rt_nl",
    "        call    RT_ERRSTR",
    "        call    RT_ABORT",
    "RT_ASSERT endp",
    "",
    "; ---- volani ciste virtualni metody ----",
    "RT_PURE proc    near",
    "        call    RT_FLUSH",
    "        lea     rsi, rt_pmsg",
    "        call    RT_ERRSTR",
    "        call    RT_ABORT",
    "RT_PURE endp",
    "",
    "; ---- int printf (char * format, ...) ----",
    "; format v rdi, argumenty v rsi, rdx, rcx, r8, r9",
    "; podporovano: %d %i %u %x %c %s %%, priznaky - a 0, sirka, l (long)",
    "RT_PRINTF proc  near",
    "        push    rbp",
    "        mov     rbp, rsp",
    "        push    r9                      ; [rbp-8]  5. argument",
    "        push    r8",
    "        push    rcx",
    "        push    rdx",
    "        push    rsi                     ; [rbp-40] 1. argument",
    "        push    rbx",
    "        push    r12",
    "        push    r13",
    "        push    r14",
    "        push    r15                     ; [rbp-80]",
    "        sub     rsp, 48                 ; [rbp-128 .. rbp-81] cislice",
    "        mov     r12, rdi                ; formatovaci retezec",
    "        lea     r13, [rbp-40]           ; dalsi argument",
    "        mov     dword ptr [rt_count], 0",
    "",
    "rpf_next:",
    "        movzx   eax, byte ptr [r12]",
    "        inc     r12",
    "        test    eax, eax",
    "        jz      rpf_end",
    "        cmp     al, '%'",
    "        je      rpf_conv",
    "rpf_char:",
    "        call    RT_PUTC",
    "        jmp     rpf_next",
    "",
    "rpf_conv:",
    "        xor     r14d, r14d              ; priznaky: 1 = '-', 2 = '0'",
    "        xor     r15d, r15d              ; sirka",
    "        xor     ebx, ebx                ; bl = 'l'",
    "rpf_flag:",
    "        movzx   eax, byte ptr [r12]",
    "        cmp     al, '-'",
    "        jne     rpf_f2",
    "        or      r14d, 1",
    "        inc     r12",
    "        jmp     rpf_flag",
    "rpf_f2: cmp     al, '0'",
    "        jne     rpf_width",
    "        or      r14d, 2",
    "        inc     r12",
    "        jmp     rpf_flag",
    "rpf_width:",
    "        movzx   eax, byte ptr [r12]",
    "        sub     eax, '0'",
    "        cmp     eax, 9",
    "        ja      rpf_long",
    "        imul    r15d, r15d, 10",
    "        add     r15d, eax",
    "        inc     r12",
    "        jmp     rpf_width",
    "rpf_long:",
    "        movzx   eax, byte ptr [r12]",
    "        cmp     al, 'l'",
    "        jne     rpf_type",
    "        mov     bl, 1",
    "        inc     r12",
    "        jmp     rpf_long",
    "rpf_type:",
    "        movzx   eax, byte ptr [r12]",
    "        test    eax, eax",
    "        jz      rpf_end",
    "        inc     r12",
    "        cmp     al, 'd'",
    "        je      rpf_dec",
    "        cmp     al, 'i'",
    "        je      rpf_dec",
    "        cmp     al, 'u'",
    "        je      rpf_uns",
    "        cmp     al, 'x'",
    "        je      rpf_hex",
    "        cmp     al, 'c'",
    "        je      rpf_chr",
    "        cmp     al, 's'",
    "        je      rpf_str",
    "        jmp     rpf_char                ; %% a nezname konverze - znak",
    "",
    "rpf_dec:",
    "        mov     rax, qword ptr [r13]",
    "        add     r13, 8",
    "        test    bl, bl",
    "        jnz     rpf_d1",
    "        movsxd  rax, eax                ; int - 32 bitu",
    "rpf_d1: xor     esi, esi",
    "        test    rax, rax",
    "        jns     rpf_d2",
    "        neg     rax",
    "        mov     esi, '-'",
    "rpf_d2: mov     ecx, 10",
    "        jmp     rpf_num",
    "",
    "rpf_uns:",
    "        mov     ecx, 10",
    "        jmp     rpf_u0",
    "rpf_hex:",
    "        mov     ecx, 16",
    "rpf_u0: mov     rax, qword ptr [r13]",
    "        add     r13, 8",
    "        test    bl, bl",
    "        jnz     rpf_u1",
    "        mov     eax, eax                ; unsigned - 32 bitu",
    "rpf_u1: xor     esi, esi",
    "",
    "rpf_num:                                ; rax = cislo, rcx = zaklad, esi = znamenko",
    "        lea     rdi, [rbp-81]           ; konec cislic",
    "        mov     r8, rdi",
    "rpf_n1: xor     edx, edx",
    "        div     rcx",
    "        cmp     dl, 10",
    "        jb      rpf_n2",
    "        add     dl, 87                  ; 'a' - 10",
    "        jmp     rpf_n3",
    "rpf_n2: add     dl, '0'",
    "rpf_n3: dec     rdi",
    "        mov     byte ptr [rdi], dl",
    "        test    rax, rax",
    "        jnz     rpf_n1",
    "        mov     r9, r8",
    "        sub     r9, rdi                 ; pocet cislic",
    "        test    esi, esi",
    "        jz      rpf_out",
    "        inc     r9                      ; delka vcetne znamenka",
    "        test    r14d, 2",
    "        jz      rpf_n4",
    "        mov     eax, esi                ; doplneni nulami: znamenko hned",
    "        call    RT_PUTC",
    "        jmp     rpf_out",
    "rpf_n4: dec     rdi                     ; doplneni mezerami: znamenko pred cislice",
    "        mov     byte ptr [rdi], sil",
    "        jmp     rpf_out",
    "",
    "rpf_str:",
    "        mov     rdi, qword ptr [r13]",
    "        add     r13, 8",
    "        mov     r8, rdi",
    "rpf_s1: cmp     byte ptr [r8], 0",
    "        je      rpf_s2",
    "        inc     r8",
    "        jmp     rpf_s1",
    "rpf_s2: mov     r9, r8",
    "        sub     r9, rdi",
    "        and     r14d, 1",
    "        jmp     rpf_out",
    "",
    "rpf_chr:",
    "        mov     rax, qword ptr [r13]",
    "        add     r13, 8",
    "        lea     r8, [rbp-81]",
    "        lea     rdi, [rbp-82]",
    "        mov     byte ptr [rdi], al",
    "        mov     r9d, 1",
    "        and     r14d, 1",
    "",
    "rpf_out:                                ; text rdi .. r8, delka r9, sirka r15",
    "        test    r14d, 1",
    "        jnz     rpf_o2",
    "rpf_o1: cmp     r15, r9                 ; zarovnani vpravo",
    "        jle     rpf_o2",
    "        mov     eax, ' '",
    "        test    r14d, 2",
    "        jz      rpf_o1a",
    "        mov     eax, '0'",
    "rpf_o1a:",
    "        call    RT_PUTC",
    "        dec     r15",
    "        jmp     rpf_o1",
    "rpf_o2: cmp     rdi, r8",
    "        jae     rpf_o3",
    "        movzx   eax, byte ptr [rdi]",
    "        call    RT_PUTC",
    "        inc     rdi",
    "        jmp     rpf_o2",
    "rpf_o3: cmp     r15, r9                 ; zarovnani vlevo",
    "        jle     rpf_next",
    "        mov     eax, ' '",
    "        call    RT_PUTC",
    "        dec     r15",
    "        jmp     rpf_o3",
    "",
    "rpf_end:",
    "        mov     eax, dword ptr [rt_count]",
    "        lea     rsp, [rbp-80]",
    "        pop     r15",
    "        pop     r14",
    "        pop     r13",
    "        pop     r12",
    "        pop     rbx",
    "        mov     rsp, rbp",
    "        pop     rbp",
    "        ret",
    "RT_PRINTF endp",
    NULL
};

/* funkce behove knihovny: jmeno v C -> navesti */
/* funkce behove knihovny: jmeno v C -> navesti (dvojice, konec NULL) */
/* ToaruOS 2.3.2 (InstrToaru, PASM_TARGET=toaru): zmeny RTL_CODE - jine cisla
   syscallu (/usr/include/syscall_nums.h v hostovi: EXT 0, WRITE 4,
   GETTIMEOFDAY 6, GETPID 9, SBRK 10, KILL 37) a halda pres sbrk (prirustek
   po strankach, vraci STARY konec; Linux brk vraci novy; nove stranky
   nejsou vynulovane - zmereno v hostovi, kernel/sys/syscall.c je nenuluje).  Skupiny:
   radek RTL_CODE, pocet nahrazenych radku (od nej), nove radky, "@" */
static const char * RTL_TOARU [] =
{
    "        mov     eax, 1                  ; write (3, [rsp], 8)", "1",
    "        mov     eax, 4                  ; write (3, [rsp], 8) - ToaruOS",
    "@",
    "        mov     eax, 60                 ; exit (main)", "1",
    "        mov     eax, 0                  ; exit (main) - ToaruOS SYS_EXT",
    "@",
    "        mov     eax, 1                  ; write (1, rt_obuf, rt_olen)", "1",
    "        mov     eax, 4                  ; write (1, rt_obuf, rt_olen) - ToaruOS",
    "@",
    "        mov     eax, 201", "2",
    "        push    rdi                     ; ToaruOS: gettimeofday (&tv, 0)",
    "        sub     rsp, 16",
    "        mov     rdi, rsp",
    "        xor     esi, esi",
    "        mov     eax, 6",
    "        syscall",
    "        mov     rax, qword ptr [rsp]    ; tv_sec",
    "        add     rsp, 16",
    "        pop     rdi",
    "        test    rdi, rdi",
    "        jz      rtt1",
    "        mov     qword ptr [rdi], rax    ; *t = cas",
    "rtt1:   nop",
    "@",
    "        mov     eax, 60", "1",
    "        mov     eax, 0                  ; exit - ToaruOS",
    "@",
    "        mov     eax, 12                 ; brk (0) = soucasny konec dat", "1",
    "        mov     eax, 10                 ; sbrk (0) = soucasny konec haldy - ToaruOS",
    "@",
    "        lea     rdi, [rdx+65536]        ; brk (konec + 64 KB)", "8",
    "        lea     rdi, [rdx+65536]        ; ToaruOS: sbrk (prirustek) vraci stary konec",
    "        sub     rdi, qword ptr [rt_hend]",
    "        add     rdi, 4095",
    "        and     rdi, -4096              ; prirustek po strankach",
    "        push    rdi",
    "        mov     eax, 10                 ; sbrk",
    "        syscall",
    "        pop     rdi",
    "        pop     rdx",
    "        cmp     rax, qword ptr [rt_hend]",
    "        jne     rtc9                    ; chyba (-errno) nebo cizi konec",
    "        push    rdi                     ; ToaruOS stranky ze sbrk NENULUJE (Linux ano),",
    "        mov     rcx, rdi                ; calloc a new spolehaji na nuly",
    "        mov     rdi, rax",
    "        xor     eax, eax",
    "        rep     stosb",
    "        pop     rdi",
    "        add     qword ptr [rt_hend], rdi",
    "@",
    "        mov     eax, 1                  ; write (2, rsi, rdx)", "1",
    "        mov     eax, 4                  ; write (2, rsi, rdx) - ToaruOS",
    "@",
    "        mov     eax, 39                 ; getpid", "1",
    "        mov     eax, 9                  ; getpid - ToaruOS",
    "@",
    "        mov     eax, 62", "1",
    "        mov     eax, 37                 ; kill - ToaruOS",
    "@",
    "        mov     eax, 60                 ; exit (134)", "1",
    "        mov     eax, 0                  ; exit (134) - ToaruOS",
    "@",
    NULL
};

static const char * RTL_FUNCTIONS [] =
{
    "printf",   "RT_PRINTF",
    "putchar",  "RT_PUTCHAR",
    "puts",     "RT_PUTS",
    "abs",      "RT_ABS",
    "labs",     "RT_LABS",
    "rand",     "RT_RAND",
    "srand",    "RT_SRAND",
    "time",     "RT_TIME",
    "exit",     "RT_EXIT",
    "malloc",   "RT_MALLOC",
    "calloc",   "RT_CALLOC",
    "free",     "RT_FREE",
    "memcpy",   "RT_MEMCPY",
    "memset",   "RT_MEMSET",
    "memcmp",   "RT_MEMCMP",
    "strlen",   "RT_STRLEN",
    NULL
};


/* navesti funkce behove knihovny, "" = neni */
static char * rtlFunction (const char * name)
{
    int i;
    for (i = 0; RTL_FUNCTIONS [i] != NULL; i += 2)
        if (StrEq (name, RTL_FUNCTIONS [i]))
            return (char *) RTL_FUNCTIONS [i + 1];
    return (char *) "";
}

/* ---------------------------------------------------------------------- */

const char * InstrOutputFile = NULL; /* instr.h - ulozit program misto spusteni */
const char * InstrObjectFile = NULL; /* instr.h - jen objekt ELF64 (prekladat po souborech) */
const char * InstrAsmFile = NULL;    /* instr.h - ulozit vygenerovany .asm (-S) */
int InstrToaru = 0;                  /* instr.h - program pro ToaruOS (RTL_TOARU, pasm -T) */
const char * InstrToaruLib = NULL;   /* instr.h - libc ToaruOS, staticky linker pasm (cmini -L) */

/* ---------------------------------------------------------------------- */

/* umisteni promenne (Variable_new: global false, offset 0, ref false) */
typedef struct Variable
{
    bool   global;
    char * label;              /* globalni promenna: navesti v datech */
    long   offset;             /* lokalni promenna: [rbp + offset] */
    bool   ref;                /* reference - v pameti je adresa */
}
    Variable;

static Variable * Variable_new (void)
{
    Variable * v = (Variable *) MemAlloc (sizeof (Variable));
    v->global = false;
    v->label = (char *) "";
    v->offset = 0;
    v->ref = false;
    return v;
}

typedef struct LoopLabels
{
    char * break_label;
    char * continue_label;
    long   break_depth;                /* uroven ruseni lokalnich objektu pro break */
    long   continue_depth;             /* ... pro continue */
}
    LoopLabels;

static LoopLabels * LoopLabels_new (const char * b, const char * c, long bd, long cd)
{
    LoopLabels * l = (LoopLabels *) MemAlloc (sizeof (LoopLabels));
    l->break_label = (char *) b;
    l->continue_label = (char *) c;
    l->break_depth = bd;
    l->continue_depth = cd;
    return l;
}

/* umisteni argumentu podle System V (jednotne pro volajiciho i volany): kind 0 = registry INTEGER
   (nq qwordu od regno), 1 = XMM (regno), 2 = zasobnik (od qwordu stack; nq qwordu) */
typedef struct ArgSlot
{
    long kind;
    long nq;
    long regno;
    long stack;
}
    ArgSlot;

/* jak vyhodnotit this pri volani metody / konstruktoru (misto lambda funkce) */
enum ThisKind
{
    THIS_STACK,    /* konstruktor: this je na zasobniku, [rsp + 8 * (depth - this_depth)] */
    THIS_LOAD,     /* f () v metode: this teto metody */
    THIS_ADDR,     /* obj.f ():  &obj */
    THIS_EXPR      /* p->f ():  p */
};

typedef struct ThisCode
{
    long     kind;            /* enum ThisKind */
    long     this_depth;      /* THIS_STACK */
    ExprPtr  object;          /* THIS_ADDR, THIS_EXPR */
}
    ThisCode;

/* polozka typu kontejner v objektu: posun a typ */
typedef struct OwnedMember
{
    long    offset;
    TypePtr type;
}
    OwnedMember;

/* seznam objektu (lokalni objekty s destruktorem jednoho bloku) */
typedef struct ObjectList
{
    Seq items;             /* ObjectPtr */
}
    ObjectList;

/* docasne objekty struktur hodnotou (vysledky volani s vlastnenymi polozkami) cekajici na
   uvolneni - v konci vyrazu / prikazu, po vyhodnoceni podminky (viz releaseTemps) */
typedef struct PendingTemp
{
    long    off;
    TypePtr type;
}
    PendingTemp;

#define VAR_SIZE 8 /* velikost jedne promenne v bajtech */

/* ---------------------------------------------------------------------- */

/* docasny adresar, na konci (TempDir_free) se smaze i se soubory */
typedef struct TempDir
{
    char * path;
    Seq    files;          /* char * */
}
    TempDir;

static TempDir * TempDir_new (void)
{
    TempDir * d = (TempDir *) MemAlloc (sizeof (TempDir));
    const char * tmp = getenv ("TMPDIR");
    char * templ = Cat (tmp != NULL && tmp [0] != 0 ? tmp : "/tmp", "/cparser-XXXXXX");
    char buf [4096];
    if (strlen (templ) >= sizeof (buf))
        InstrError ("Temporary directory name too long");
    strcpy (buf, templ);
    if (mkdtemp (buf) == NULL)
        InstrError ("Cannot create temporary directory");
    d->path = Str (buf);
    return d;
}

static char * TempDir_file (TempDir * d, const char * name)
{
    SeqAdd (&d->files, (long) Str (name));
    return Cat3 (d->path, "/", name);
}

static void TempDir_free (TempDir * d)
{
    long i;
    for (i = 0; i < SeqSize (&d->files); i++)
        unlink (Cat3 (d->path, "/", (char *) SeqGet (&d->files, i)));
    rmdir (d->path);
}

static void copyFile (const char * from, const char * to, bool executable)
{
    FILE * f = fopen (from, "rb");
    FILE * g = fopen (to, "wb");
    bool ok = f != NULL && g != NULL;
    if (ok)
    {
        char buf [65536];
        size_t n;
        while ((n = fread (buf, 1, sizeof (buf), f)) > 0)
            if (fwrite (buf, 1, n, g) != n)
                ok = false;
        if (ferror (f)) ok = false;
    }
    if (f != NULL) fclose (f);
    if (g != NULL && fclose (g) != 0) ok = false;
    if (! ok)
        InstrError (Cat ("Cannot write ", to));
    if (executable)
        chmod (to, 0755);
}

/* cely soubor jako retezec (chybejici soubor => "") */
static char * readFile (const char * name)
{
    char * text = ReadFile (name, NULL, false);
    return text != NULL ? text : (char *) "";
}

/* ---------------------------------------------------------------------- */

/* stav generatoru (drive polozky tridy AsmText - generator ma vzdy jen
   jednu instanci; nastavi initState, uvolni freeState); u Seq a Dict typ
   prvku / klice => hodnoty v poznamce */

static AsmProgram * prog = NULL;       /* cely program - seznam uzlu (asm/asmlist.h) */

/* ulozi vygenerovany text pro assembler (AsmPrint) do souboru path */
static void saveAsm (const char * path)
{
    FILE * f = fopen (path, "w");
    if (f == NULL)
        InstrError (Cat ("Cannot write ", path));
    AsmPrint (prog, f);
    if (fclose (f) != 0)
        InstrError (Cat ("Cannot write ", path));
}
static AsmNode *    ext_cur;              /* misto pro extrn / public (doplni se na konci) */
static AsmNode *    data_cur;             /* konec generovanych dat (kod: kurzor programu) */
static AsmNode *    code_tail;            /* misto pro RTL_LIBC_MAIN na konci kodu (odebere se) */
static Dict label_objs;                   /* SDict: jmeno => AsmLabel * */

static Dict vars;                         /* ObjectPtr => Variable * */
static Dict fce_labels;                   /* ObjectPtr => char * */
static Seq  fce_keys;                     /* ObjectPtr - klice fce_labels serazene (jako std::map) */
static Dict string_labels;                /* SDict: retezcove konstanty => char * */
static Dict sconst_labels;                /* SDict: konstanty std::string (s delkou) => char * */

static bool use_libc;                     /* glibc (vychozi), jinak vlastni knihovna */
static Dict extern_labels;                /* SDict: funkce glibc: jmeno => navesti LC_... (char *) */
static Seq  extern_keys;                  /* char * - klice extern_labels serazene */
static Dict extern_lower;                 /* SDict: navesti LC_... malymi pismeny (pasm nerozlisuje) => bool */

static long label_count;

static ObjectPtr current_fce;
static char *    epilogue_label = "";
static long      frame_size;              /* velikost lokalnich promennych */
static long      depth;                   /* pocet 8-bajtovych polozek na zasobniku (push) */
static Seq  loops;                        /* LoopLabels * - pro break a continue (i switch - jen break) */
static Dict user_labels;                  /* SDict: navesti goto v aktualni funkci => char * */
static Dict case_labels;                  /* StatPtr => char * (case / default => navesti) */
static Dict range_slots;                  /* StatPtr => long (for (T x : seq) - skryte promenne [rbp + n]) */
static bool fn_temps;                     /* funkce ma docasne retezce / kontejnery */
static long temp_mark;                    /* [rbp + temp_mark] = znacka zasobniku docasnych hodnot */
static bool objmode;                      /* preklad do objektu (-c, vice souboru) */
static bool has_main;
static Seq  public_labels;                /* char * - navesti pro linker (PUBLIC) */
static Seq  public_names;                 /* char * - ... a jejich jmena */
static Dict extern_data;                  /* SDict: externi promenne: jmeno => navesti (char *) */
static Seq  extern_data_keys;             /* char * - klice extern_data serazene */

/* struktury a tridy */
static Dict struct_sizes;                 /* TypePtr => long: velikost v bajtech (jako v C, zaokrouhlena na zarovnani) */
static Dict struct_ends;                  /* TypePtr => long: konec posledni polozky (odvozena trida pokracuje za ni - jako c2c, translate.c) */
static Dict struct_aligns;                /* TypePtr => long: zarovnani */
static Dict member_offsets;               /* ObjectPtr => long: posun polozky v bajtech */
static Dict vptr_offsets;                 /* TypePtr => long: posun ukazatele na tabulku virtualnich metod */
static Dict vtable_labels;                /* TypePtr => char *: pouzite tabulky VT<n> */
static Seq  vtable_keys;                  /* TypePtr - klice vtable_labels serazene */
static Dict ctor_labels;                  /* TypePtr => char *: konstruktory bez argumentu */
static Seq  implicit_ctors;               /* TypePtr - implicitni konstruktory k vygenerovani */
static Seq  implicit_dtors;               /* TypePtr - implicitni destruktory k vygenerovani */
static Dict dtor_labels;                  /* TypePtr => char * */
static Seq  global_dtor_objects;          /* ObjectPtr - globalni objekty s destruktorem (CFINI) */
static Seq  cleanup;                      /* ObjectList * - lokalni objekty s destruktorem po blocich */
static Seq  global_objects;               /* ObjectPtr - globalni objekty s konstruktorem */

static TypePtr   current_class;           /* metoda nebo konstruktor: trida objektu this */
static ObjectPtr this_obj;                /* skryty parametr this */
static ObjectPtr ret_obj;                 /* skryty parametr: adresa pro vysledek - struktura hodnotou */
static Dict inptr_offsets;                /* ObjectPtr => long: parametr - struktura hodnotou: [rbp + n] = adresa argumentu */

static Seq pending_temps;                 /* PendingTemp * */

/* ------------------------------------------------------------------ */

/* funkce generatoru (drive metody AsmText) - prototypy: funkce se volaji
   v libovolnem poradi.  Prevod z C++: string => char *, Sequence & =>
   Seq *, reference long & / bool & => long * / bool *, implicitni hodnoty
   parametru se predavaji vzdy (ins0 .. ins3 misto ins s nullptr) */

static void insertSortedStr (Seq * keys, const char * k);
static void insertSortedType (Seq * keys, TypePtr k);
static void insertSortedObj (Seq * keys, ObjectPtr k);
static void setFceLabel (ObjectPtr f, const char * label);
static void addPublic (const char * label, const char * name);
static long run (bool verbose);
static bool runTool (const char * dir, Seq * args, char * * msg);
static void assemble (const char * dir, const char * mode);
static void linkToaru (const char * dir);
static void linkProg (const char * dir);
static void linkFiles (const char * dir, Seq * inputs);
static long linkAndRun (Seq * inputs, const char * out);
static long execute (const char * bin_name);
static char * newLabel (const char * prefix);                  /* drive implicitne "L" */
static AsmLabel * labelObj (const char * name);
static AsmOp * gp (Register r);
static AsmOp * xreg (int n);
static AsmOp * imm (long v);
static AsmOp * imm_hex (unsigned long v);
static AsmOp * imm_hex_lit (unsigned long v);
static AsmOp * qword_ptr (Register b, long off);
static AsmOp * byte_ptr (Register b, long off);
static AsmOp * any_ptr (Register b, long off);
static AsmOp * any_ptr_idx (Register b, Register i, int scale, long off);
static AsmOp * qword_lab (const char * l);
static AsmOp * alab (const char * l);
static AsmOp * jlab (const char * l);
static AsmNode * note (AsmNode * n, const char * text);
static AsmNode * ins (keyword k, AsmOp * a, AsmOp * b, AsmOp * c);
static AsmNode * ins0 (keyword k);
static AsmNode * ins1 (keyword k, AsmOp * a);
static AsmNode * ins2 (keyword k, AsmOp * a, AsmOp * b);
static AsmNode * ins3 (keyword k, AsmOp * a, AsmOp * b, AsmOp * c);
static void insPrefix (keyword prefix, keyword k);
static void bind (const char * label);
static void comment (const char * text);
static void blank (void);
static void textLines (const char * * text);
static bool toaruTarget (void);
static void textLinesToaru (const char * * text);
static AsmNode * dataBegin (void);
static void dataEnd (AsmNode * code_cursor);
static void dataComment (const char * text);
static void dataQ (const char * label, AsmOp * item, const char * c);   /* c drive implicitne "" */
static AsmOp * slotOp (const char * v);
static void startProgram (void);
static void finishProgram (void);
static bool isByteType (TypePtr type);
static void callRt (const char * label, const char * text);            /* text drive implicitne "" */
static void push (Register r);
static void pop (Register r);
static void emitBytes (const char * label, const char * value);
static char * stringLabel (const char * value);
static char * externLabel (const char * name);
static char * stringConstLabel (const char * value);
static bool isValueRef (TypePtr t);
static long byteSize (TypePtr type);
static bool enumValue (ObjectPtr obj, long * value);
static bool constValue (ExprPtr expr, long * value);
static long elemSize (TypePtr type);
static long typeCount (TypePtr type);
static long typeBytes (TypePtr type);
static long typeAlign (TypePtr type);
static long structBytes (TypePtr type);
static long lvalueSize (ExprPtr e);
static void loadSized (long size, bool uns);
static void loadValue (ExprPtr lv);
static void storeSized (Register addr, Register val, long size);
static void normalizeTo (TypePtr t);
static void normalizeSize (long size, bool uns);
static void arithType (TypePtr t1, TypePtr t2, long * size, bool * uns);
static void copyBytes (long n);
static char * className (TypePtr cls);
static long structCount (TypePtr type);
static long memberOffset (ObjectPtr member, TypePtr cls);
static int vtableSlot (TypePtr cls, ObjectPtr m);
static char * vtableLabel (TypePtr cls);
static void codeVtables (void);
static char * ctorLabel (TypePtr cls);
static void addDefaultArgs (ObjectPtr fce, Seq * args);
static void addDefaultArgsT (TypePtr ftype, Seq * args, const char * name);
static char * symName (ObjectPtr f);
static bool exported (ObjectPtr f);
static char * methodTarget (ObjectPtr m, const char * what);
static char * fnLabel (ObjectPtr fce);
static void codeIndirectCall (ExprPtr expr, ExprPtr f);
static void callCtor0 (TypePtr cls);
static void callCtor (TypePtr cls, Seq * args);                        /* drive Sequence hodnotou - args se nemeni */
static void constructVar (TypePtr type);
static void codeStringValue (ExprPtr e);
static void zeroVar (TypePtr type);
static void codeCtorPrologue (TypePtr cls, ObjectPtr ctor);            /* ctor drive implicitne nullptr */
static void codeImplicitCtor (TypePtr cls);
static char * dtorLabel (TypePtr cls);
static void destructVar (TypePtr type);
static void codeDtorEpilogue (TypePtr cls);
static void codeImplicitDtor (TypePtr cls);
static void codeDelete (ExprPtr expr);
static void codeGlobalFini (void);
static void codeGlobalInit (void);
static void flattenList (ExprPtr e, Seq * items);
static void staticBytes (Seq * slots, long pos, unsigned long value, long size, const char * name);
static void staticOffset (Seq * slots, long pos, const char * label, const char * name);
static void staticInit (TypePtr type, ExprPtr init, Seq * slots, long pos, const char * name);
static void allocGlobal (ObjectPtr obj);
static void allocGlobals (void);
static void allocLocal (ObjectPtr obj);
static void collectLocalDecls (Queue * decl);
static void collectLocals (StatPtr stat);
static Variable * variable (ExprPtr expr);
static void codeDecl (ObjectPtr obj);
static void codeFunction (ObjectPtr decl);
static void loadThis (void);
static void normalizeBool (void);
static void codeVarAddress (ExprPtr expr);
static void codeAddress (ExprPtr expr);
static void checkPointee (TypePtr ptr);
static void codeLoad (ExprPtr expr);
static bool isUnsignedOp (TypePtr t1, TypePtr t2);
static void codeOperation (SYMBOL kind, bool uns);                    /* uns drive implicitne false */
static void codeBinary (ExprPtr expr);
static bool isPtrValue (TypePtr t);
static unsigned long fpBits (double v, int kind);
static bool fpConstValue (ExprPtr expr, double * value);
static void toXmm (int n, int kind);
static void fromXmm (int kind);
static void fpToBool (int kind);
static void convert (TypePtr from, TypePtr to);
static void codeExprAs (ExprPtr e, TypePtr to);
static void codeBoolValue (ExprPtr e);
static TypePtr fpCommon (TypePtr t1, TypePtr t2);
static void codeFpBinary (ExprPtr expr);
static void codeFpAssign (ExprPtr expr, SYMBOL op);
static void codeCast (ExprPtr expr);
static void codeDynamicCheck (TypePtr src, TypePtr target, bool ref, ExprPtr expr);
static void codePointerArith (ExprPtr expr);
static void codeAndOr (ExprPtr expr);
static void codeConditional (ExprPtr expr);
static SYMBOL assignOp (SYMBOL op);
static void codeAssign (ExprPtr expr);
static void codeIncDec (ExprPtr expr);
static void collectArgs (ExprPtr expr, Seq * args);
static void codeAssert (Seq * args);
static void codeNew (ExprPtr expr);
static void codeArg (ExprPtr arg, TypePtr ptype);
static bool isFpArg (TypePtr ptype, ExprPtr arg);
static void codeThis (ThisCode * t);
static long passArgs (ThisCode * code_this, Seq * args, Seq * ptypes, int * nxmm, long sret_off);   /* sret_off drive implicitne 0 */
static Seq * classifyArgs (Seq * types, Seq * fp);                    /* vysledek: ArgSlot * */
static bool isStructParam (ObjectPtr par);
static void checkStructVal (TypePtr t);
static void incOwned (TypePtr type);
static void decOwned (TypePtr type);
static void releaseTemps (long mark);
static void checkStructCopy (TypePtr t);
static long allocTemp (long count);
static void copySlots (long count);
static void codeStructAddr (ExprPtr e);
static void codeCall (ExprPtr expr);
static void codeExpr (ExprPtr expr);
static void codeCond (ExprPtr expr);
static void codeLocalDecls (Queue * decl);
static void strTemp (void);
static bool ownedTemp (TypePtr res);
static bool exprOwned (ExprPtr e);
static bool statOwned (StatPtr s);
static void codeContValue (ExprPtr e);
static void collectContMembers (TypePtr cls, long base, Seq * out);  /* OwnedMember * */
static int cmpKind (TypePtr t);
static void codeContMethod (ExprPtr expr, ExprPtr f, ObjectPtr fce, TypePtr cls);
static void contSlotAddr (ExprPtr f);
static void contHandle (ExprPtr f);
static void codeDictMethod (const char * name, Seq * args, TypePtr cls, ExprPtr f);
static int elemKind (TypePtr T);
static bool isBoxedElem (TypePtr T);
static void boxElem (TypePtr T);
static void unboxElem (TypePtr T, bool store_slot);
static void storeElem (TypePtr T);
static int dictKind (TypePtr K);
static void codeContElemAddress (ExprPtr expr);
static void codeRangeFor (StatPtr stat);
static void codeLocalInit (TypePtr type, ExprPtr init, long off, const char * name);
static void codeIf (StatPtr stat);
static char * userLabel (const char * id);
static void collectCases (StatPtr stat, Seq * cases);
static void codeSwitch (StatPtr stat);
static void codeWhile (StatPtr stat);
static void codeDo (StatPtr stat);
static void codeFor (StatPtr stat);
static void codeReturn (StatPtr stat);
static void codeCompoundStat (StatPtr stat);
static bool hasLocalsToDestroy (long depth);
static void destroyLocals (long depth);
static void codeStat (StatPtr stat);

/* ------------------------------------------------------------------ */

/* pocatecni stav (drive konstruktor AsmText) - pred kazdym prekladem;
   program z predchoziho behu (i po chybe) se uvolni */
static void initState (void)
{
    if (prog != NULL) AsmDestroy (prog);
    prog = AsmCreate ();
    ext_cur = NULL;
    data_cur = NULL;
    code_tail = NULL;
    use_libc = true;
    label_count = 0;
    current_fce = NULL;
    frame_size = 0;
    depth = 0;
    fn_temps = false;
    temp_mark = 0;
    objmode = false;
    has_main = false;
    current_class = NULL;
    this_obj = NULL;
    ret_obj = NULL;
    DictClear (&inptr_offsets);
    DictClear (&label_objs);
    DictClear (&vars);
    DictClear (&fce_labels);
    SeqClear (&fce_keys);
    DictClear (&string_labels);
    DictClear (&sconst_labels);
    DictClear (&extern_labels);
    SeqClear (&extern_keys);
    DictClear (&extern_lower);
    SeqClear (&loops);
    DictClear (&user_labels);
    DictClear (&case_labels);
    DictClear (&range_slots);
    SeqClear (&public_labels);
    SeqClear (&public_names);
    DictClear (&extern_data);
    SeqClear (&extern_data_keys);
    DictClear (&struct_sizes);
    DictClear (&struct_ends);
    DictClear (&struct_aligns);
    DictClear (&member_offsets);
    DictClear (&vptr_offsets);
    DictClear (&vtable_labels);
    SeqClear (&vtable_keys);
    DictClear (&ctor_labels);
    SeqClear (&implicit_ctors);
    SeqClear (&implicit_dtors);
    DictClear (&dtor_labels);
    SeqClear (&global_dtor_objects);
    SeqClear (&cleanup);
    SeqClear (&global_objects);
}

/* uvolneni (drive destruktor AsmText) */
static void freeState (void)
{
    AsmDestroy (prog);
    prog = NULL;
}

/* vlozeni do serazeneho seznamu klicu (poradi jako iterace std::map) */
static void insertSortedStr (Seq * keys, const char * k)
{
    long i = SeqSize (keys);
    SeqAdd (keys, (long) k);
    while (i > 0 && strcmp (k, (char *) SeqGet (keys, i - 1)) < 0)
    {
        SeqSet (keys, i, SeqGet (keys, i - 1));
        i --;
    }
    SeqSet (keys, i, (long) k);
}

/* ukazatele se porovnavaji jako adresy (jako std::map <T *, ...>) */
static void insertSortedType (Seq * keys, TypePtr k)
{
    long i = SeqSize (keys);
    SeqAdd (keys, (long) k);
    while (i > 0 && (unsigned long) k < (unsigned long) SeqGet (keys, i - 1))
    {
        SeqSet (keys, i, SeqGet (keys, i - 1));
        i --;
    }
    SeqSet (keys, i, (long) k);
}

static void insertSortedObj (Seq * keys, ObjectPtr k)
{
    insertSortedType (keys, (TypePtr) k);
}

static void setFceLabel (ObjectPtr f, const char * label)
{
    if (! DictContains (&fce_labels, (long) f))
        insertSortedObj (&fce_keys, f);
    DictStore (&fce_labels, (long) f, (long) label);
}

static void addPublic (const char * label, const char * name)
{
    SeqAdd (&public_labels, (long) label);
    SeqAdd (&public_names, (long) name);
}

static AsmNode * ins0 (keyword k) { return ins (k, NULL, NULL, NULL); }
static AsmNode * ins1 (keyword k, AsmOp * a) { return ins (k, a, NULL, NULL); }
static AsmNode * ins2 (keyword k, AsmOp * a, AsmOp * b) { return ins (k, a, b, NULL); }
static AsmNode * ins3 (keyword k, AsmOp * a, AsmOp * b, AsmOp * c) { return ins (k, a, b, c); }

static long run (bool verbose)
{
    /* CPARSER_RTL=own => vlastni behova knihovna (bez glibc, mensi systemy) */
    const char * rtl = getenv ("CPARSER_RTL");
    long i;
    NodePtr p;
    objmode = InstrObjectFile != NULL;
    /* ToaruOS (-T): samostatny program s vlastni knihovnou a syscally ToaruOS
       (objekt -c dal pro tamni gcc a libc) */
    use_libc = ! ((rtl != NULL && strcmp (rtl, "own") == 0) || (toaruTarget () && ! objmode));
    if (InstrToaruLib != NULL)
    {
        /* libc ToaruOS: rezim libc, slinkuje staticky linker pasm (linkToaru) */
        if (toaruTarget ())
            InstrError ("Options -T (own runtime) and -L (ToaruOS libc) cannot be combined");
        if (InstrOutputFile == NULL && ! objmode)
            InstrError ("Option -L (ToaruOS libc) requires -o (the program runs only in ToaruOS)");
        use_libc = true;
    }
    if (objmode && !use_libc)
        InstrError ("Object files (-c) require glibc (not CPARSER_RTL=own)");
    startProgram ();                   /* kostra programu (seznam uzlu), kurzor na kod */
    if (use_libc)
        for (i = 0; RTL_LIBC_EXTERNS [i] != NULL; i++)
            externLabel (RTL_LIBC_EXTERNS [i]);

    ObjectPtr main_fce = NULL;
    for (p = glist.first; p != NULL; p = p->next)
    {
        ObjectPtr obj = p->item;
        if (obj->fce && obj->body != NULL)
        {
            if (StrEq (obj->name, "main"))
            {
                main_fce = obj;
                setFceLabel (obj, "FMAIN");     /* vola start v RTL_CODE */
            }
            else
                setFceLabel (obj, newLabel ("F"));
        }
    }

    for (p = ctorlist.first; p != NULL; p = p->next)
        setFceLabel (p->item, newLabel ("F"));     /* konstruktory */
    for (p = methodlist.first; p != NULL; p = p->next)
        setFceLabel (p->item, newLabel ("F"));     /* metody */

    /* objekt: verejne funkce, metody a konstruktory definovane mimo tridu */
    if (objmode)
        for (i = 0; i < SeqSize (&fce_keys); i++)
        {
            ObjectPtr f = (ObjectPtr) SeqGet (&fce_keys, i);
            if (exported (f))
                addPublic (DictGetStr (&fce_labels, (long) f), symName (f));
        }

    allocGlobals ();             /* po navestich funkci (ukazatele na funkce v datech) */

    has_main = main_fce != NULL;
    if (main_fce == NULL && !objmode)
        InstrError ("Function main not found");

    for (p = glist.first; p != NULL; p = p->next)
        codeDecl (p->item);
    for (p = ctorlist.first; p != NULL; p = p->next)
        codeFunction (p->item);
    for (p = methodlist.first; p != NULL; p = p->next)
        codeFunction (p->item);

    codeGlobalInit ();                             /* CINIT */
    codeGlobalFini ();                             /* CFINI */
    for (i = 0; i < SeqSize (&implicit_ctors); i++)
        codeImplicitCtor ((TypePtr) SeqGet (&implicit_ctors, i));     /* seznam muze behem generovani rust */
    codeVtables ();
    for (i = 0; i < SeqSize (&implicit_dtors); i++)
        codeImplicitDtor ((TypePtr) SeqGet (&implicit_dtors, i));     /* i z tabulek virtualnich metod */

    finishProgram ();

    /* preklad assemblerem pasm (text z pameti) a spusteni */
    TempDir * dir = TempDir_new ();
    char * bin_name = TempDir_file (dir, "prog.bin");
    char * lst_name = TempDir_file (dir, "prog.lst");
    TempDir_file (dir, "prog.o");
    TempDir_file (dir, "tool.out");

    /* text pro assembler i pri chybe: -S soubor.asm a -o x => x.asm */
    if (InstrAsmFile != NULL)
        saveAsm (InstrAsmFile);
    if (InstrOutputFile != NULL)
        saveAsm (Cat (InstrOutputFile, ".asm"));

    if (objmode)
    {
        /* jen objekt ELF64 (dalsi soubory a knihovny se slinkuji pozdeji) */
        assemble (dir->path, "-c");
        copyFile (Cat (dir->path, "/prog.o"), InstrObjectFile, false);
        TempDir_free (dir);
        return 0;
    }

    if (use_libc)
    {
        /* objekt ELF64 + gcc (glibc) */
        assemble (dir->path, "-c");
        if (InstrToaruLib != NULL) linkToaru (dir->path);   /* libc ToaruOS, PasmLink */
        else linkProg (dir->path);
    }
    else
        assemble (dir->path, "-e");   /* samostatny ELF64, vlastni knihovna */

    if (verbose)
    {
        printf ("%s", readFile (lst_name));
        printf ("\n");
    }

    if (InstrOutputFile != NULL)
    {
        /* ulozit program (a text pro assembler a vypis), nespoustet */
        const char * out = InstrOutputFile;
        copyFile (bin_name, out, true);
        copyFile (lst_name, Cat (out, ".lst"), false);
        TempDir_free (dir);
        return 0;
    }

    long result = execute (bin_name);
    TempDir_free (dir);
    return result;
}

/* ------------------------------------------------------------------ */

/* spusteni nastroje v docasnem adresari, vystup do tool.out;
   vraci true pri uspechu, msg = vystup nastroje */
static bool runTool (const char * dir, Seq * args, char * * msg)
{
    fflush (stdout);       /* ne fflush (NULL): */
    fflush (stderr);       /* v libc ToaruOS 2.3.2 pada (Segmentation fault) */

    pid_t pid = fork ();
    if (pid < 0)
        InstrError ("fork failed");

    if (pid == 0)
    {
        char * out = Cat (dir, "/tool.out");
        FILE * f = fopen (out, "w");
        if (f != NULL)
        {
            dup2 (fileno (f), 1);
            dup2 (fileno (f), 2);
        }
        if (chdir (dir) != 0) _exit (126);
        long n = SeqSize (args);
        long i;
        char * * argv = (char * *) MemAlloc ((n + 1) * sizeof (char *));
        for (i = 0; i < n; i++) argv [i] = (char *) SeqGet (args, i);
        argv [n] = NULL;
        execvp (argv [0], argv);
        _exit (127);
    }

    int status = 0;
    waitpid (pid, &status, 0);

    char * m = readFile (Cat (dir, "/tool.out"));
    long len = strlen (m);
    while (len > 0 && (m [len - 1] == '\n' || m [len - 1] == '\r'))
        len --;
    m = StrN (m, len);
    if (WIFEXITED (status) && WEXITSTATUS (status) == 127)
        m = Cat ("cannot execute ", (char *) SeqGet (args, 0));
    * msg = m;
    return WIFEXITED (status) && WEXITSTATUS (status) == 0;
}

/* jako  pasm -e -l prog.asm  /  pasm -c -l prog.asm:  vystupy prog.o /
   prog.bin a prog.lst v docasnem adresari (knihovna asm/pasmlib.h - bez
   spousteni programu pasm a bez prog.asm);  pasm cte seznam uzlu primo
   (faze 2), CPARSER_ASMLIST=text - pres text (faze 1, pro porovnani) */
static void assemble (const char * dir, const char * mode)
{
    char msg [512];
    int flags = PASM_LST;
    if (StrEq (mode, "-e")) flags = flags | PASM_EXE;
    else flags = flags | PASM_ELFOBJ;
    if (StrEq (mode, "-e") && toaruTarget ()) flags = flags | PASM_TOARU;   /* od 0x3F000000 */
    const char * how = getenv ("CPARSER_ASMLIST");
    bool text = how != NULL && strcmp (how, "text") == 0;
    char * base = Cat (dir, "/prog");
    int rc;
    if (text) rc = AsmAssembleText (prog, base, flags, msg, sizeof (msg));
    else      rc = AsmAssemble (prog, base, flags, msg, sizeof (msg));
    if (rc != 0)
        InstrError (Cat ("pasm: ", msg));
}

/* staticky linker pasm (PasmLink):  crt0.o crti.o prog.o libc.a crtn.o  => prog.bin
   (ELF64 pro ToaruOS od 0x3F000000, libc ToaruOS z InstrToaruLib - /lib ToaruOS) */
static void linkToaru (const char * dir)
{
    const char * lib = InstrToaruLib;
    char * crt0 = Cat (lib, "/crt0.o");
    char * crti = Cat (lib, "/crti.o");
    char * obj = Cat (dir, "/prog.o");
    char * libc = Cat (lib, "/libc.a");
    char * crtn = Cat (lib, "/crtn.o");
    char * out = Cat (dir, "/prog.bin");
    const char * inputs [5];
    char msg [512];
    inputs [0] = crt0;
    inputs [1] = crti;
    inputs [2] = obj;
    inputs [3] = libc;
    inputs [4] = crtn;
    if (PasmLink (out, inputs, 5, msg, sizeof (msg)) != 0)
        InstrError (msg);
}

/* gcc -no-pie prog.o -o prog.bin  (glibc) */
static void linkProg (const char * dir)
{
    Seq * inputs = SeqNew ();
    SeqAdd (inputs, (long) "prog.o");
    linkFiles (dir, inputs);
}

/* gcc -no-pie objekty ... -o prog.bin -lm  (v adresari dir) */
static void linkFiles (const char * dir, Seq * inputs)
{
    const char * cc = getenv ("CPARSER_CC");
    const char * gcc = (cc != NULL && cc [0] != 0) ? cc : "gcc";
    char * msg = (char *) "";
    Seq * args = SeqNew ();
    long i;
    SeqAdd (args, (long) gcc);
    SeqAdd (args, (long) "-no-pie");
    for (i = 0; i < SeqSize (inputs); i++) SeqAdd (args, SeqGet (inputs, i));
    SeqAdd (args, (long) "-o");
    SeqAdd (args, (long) "prog.bin");
    SeqAdd (args, (long) "-lm");
    if (! runTool (dir, args, &msg))
    {
        /* nedefinovana funkce:  undefined reference to `foo' */
        long p = StrFind (msg, "undefined reference to `", 0);
        if (p >= 0)
        {
            p += strlen ("undefined reference to `");
            long q = StrFindChar (msg, '\'', p);
            InstrError (Cat ("Undefined function: ", SubStr (msg, p, q < 0 ? -1 : q - p)));
        }
        InstrError (Cat ("link: ", msg));
    }
}

/* slinkovani objektu (i knihoven, -l...); out != NULL => ulozit program,
   jinak spustit a vratit vysledek main */
static long linkAndRun (Seq * inputs, const char * out)
{
    TempDir * dir = TempDir_new ();
    char * bin_name = TempDir_file (dir, "prog.bin");
    TempDir_file (dir, "tool.out");
    linkFiles (dir->path, inputs);
    if (out != NULL)
    {
        copyFile (bin_name, out, true);
        TempDir_free (dir);
        return 0;
    }
    long result = execute (bin_name);
    TempDir_free (dir);
    return result;
}

/* spusteni prog.bin, vysledek main se precte z roury (deskriptor 3) */
static long execute (const char * bin_name)
{
    int fd [2];
    if (pipe (fd) != 0)
        InstrError ("pipe failed");

    fflush (stdout);       /* ne fflush (NULL): */
    fflush (stderr);       /* v libc ToaruOS 2.3.2 pada (Segmentation fault) */

    pid_t pid = fork ();
    if (pid < 0)
        InstrError ("fork failed");

    if (pid == 0)
    {
        setenv ("CPARSER_RESULT", "3", 1);     /* main zapise vysledek do deskriptoru 3 */
        close (fd [0]);
        if (fd [1] != 3)
        {
            dup2 (fd [1], 3);
            close (fd [1]);
        }
        execl (bin_name, bin_name, (char *) NULL);
        _exit (127);
    }

    close (fd [1]);
    int64_t result = 0;
    long got = 0;
    while (got < (long) sizeof (result))
    {
        long n = read (fd [0], (char *) &result + got, sizeof (result) - got);
        if (n <= 0) break;
        got += n;
    }
    close (fd [0]);

    int status = 0;
    waitpid (pid, &status, 0);

    if (WIFSIGNALED (status))
        InstrError (Cat ("Program terminated by signal ", IntStr (WTERMSIG (status))));
    if (got != (long) sizeof (result))
    {
        if (WIFEXITED (status))
            return WEXITSTATUS (status);    /* exit () z programu */
        InstrError ("Program failed");
    }
    return result;
}

/* ------------------------------------------------------------------ */

/* vystup kodu */

static char * newLabel (const char * prefix)
{
    return Cat (prefix, IntStr (++ label_count));
}

/* ---- program jako seznam uzlu (asm/asmlist.h) ---- */

/* navesti podle jmena (generator pracuje se jmeny: L5, F3, S12, LC_printf) */
static AsmLabel * labelObj (const char * name)
{
    if (SDictContains (&label_objs, name))
        return (AsmLabel *) SDictGet (&label_objs, name);
    AsmLabel * l = AsmNamedLabel (prog, name);
    SDictStore (&label_objs, name, (long) l);
    return l;
}

/* operandy (jmena jako v AsmJit); patri programu prog */
static AsmOp * gp (Register r) { return AsmPReg (prog, r); }
static AsmOp * xreg (int n) { return AsmPXmm (prog, n); }
static AsmOp * imm (long v) { return AsmPImm (prog, v); }
static AsmOp * imm_hex (unsigned long v) { return AsmPImmHex (prog, (long) v); }      /* 0...h */
static AsmOp * imm_hex_lit (unsigned long v) { return AsmPImmHexLit (prog, (long) v); }   /* ...h */
static AsmOp * qword_ptr (Register b, long off) { return AsmPMem (prog, 8, b, off); }
static AsmOp * byte_ptr (Register b, long off) { return AsmPMem (prog, 1, b, off); }
static AsmOp * any_ptr (Register b, long off) { return AsmPMem (prog, 0, b, off); }
static AsmOp * any_ptr_idx (Register b, Register i, int scale, long off)
                                                { return AsmPMemIdx (prog, 0, b, i, scale, off); }
static AsmOp * qword_lab (const char * l) { return AsmPMemLab (prog, 8, labelObj (l), 0); }
static AsmOp * alab (const char * l) { return AsmPMemLab (prog, 0, labelObj (l), 0); }  /* lea rax, S5 */
static AsmOp * jlab (const char * l) { return AsmPLab (prog, labelObj (l)); }          /* jmp L5, call F3 */

/* poznamka za ; k uzlu:  note (note (ins1 (iMov, gp (RAX)), ...), "x") */
static AsmNode * note (AsmNode * n, const char * text)
{
    if (! StrEmpty (text)) AsmComment (n, text);
    return n;
}

/* instrukce s 0 .. 3 operandy na konec kodu (kurzor programu) */
static AsmNode * ins (keyword k, AsmOp * a, AsmOp * b, AsmOp * c)
{
    return AsmInsP (prog, k, a, b, c);
}

static void insPrefix (keyword prefix, keyword k)
{
    AsmPrefixed (prog, prefix, k);
}

static void bind (const char * label)
{
    AsmBind (prog, labelObj (label));
}

static void comment (const char * text)
{
    AsmTextLine (prog, Cat ("; ", text));
}

static void blank (void)
{
    AsmTextLine (prog, "");
}

/* pole radek ukoncene NULL (behova knihovna RTL_...) */
static void textLines (const char * * text)
{
    long i;
    for (i = 0; text [i] != NULL; i++)
        AsmTextLine (prog, text [i]);
}

/* program pro ToaruOS (instr.h InstrToaru nebo prostredi PASM_TARGET=toaru) */
static bool toaruTarget (void)
{
    const char * t = getenv ("PASM_TARGET");
    return InstrToaru != 0 || (t != NULL && strcmp (t, "toaru") == 0);
}

/* jako textLines, radky ze skupin RTL_TOARU nahradi */
static void textLinesToaru (const char * * text)
{
    long used = 0;
    long i = 0;
    while (text [i] != NULL)
    {
        long g = 0;
        bool found = false;
        while (RTL_TOARU [g] != NULL && ! found)
        {
            if (strcmp (RTL_TOARU [g], text [i]) == 0)
                found = true;
            else
            {
                while (strcmp (RTL_TOARU [g], "@") != 0) g ++;
                g ++;
            }
        }
        if (! found)
        {
            AsmTextLine (prog, text [i]);
            i ++;
            continue;
        }
        long n = atoi (RTL_TOARU [g + 1]);
        long k;
        for (k = g + 2; strcmp (RTL_TOARU [k], "@") != 0; k++)
            AsmTextLine (prog, RTL_TOARU [k]);
        i += n;
        used ++;
    }
    if (used != 11)                                /* vsechny skupiny prave jednou */
        InstrError ("RTL_TOARU does not match RTL_CODE");
}

/* data: uzly za data_cur (konec generovanych dat), potom zpet na konec kodu */
static AsmNode * dataBegin (void)
{
    return AsmSetCursor (prog, data_cur);
}

static void dataEnd (AsmNode * code_cursor)
{
    data_cur = AsmCursor (prog);
    AsmSetCursor (prog, code_cursor);
}

static void dataComment (const char * text)
{
    AsmNode * c = dataBegin ();
    AsmTextLine (prog, Cat ("; ", text));
    dataEnd (c);
}

/* [label] dq item  ; poznamka */
static void dataQ (const char * label, AsmOp * item, const char * c)
{
    AsmNode * save = dataBegin ();
    AsmLabel * l = NULL;
    if (! StrEmpty (label)) l = labelObj (label);
    note (AsmDataP (prog, l, iDq, item), c);
    dataEnd (save);
}

/* hodnota v datech z textu:  "offset S5", "-3", "4607182418800017408" */
static AsmOp * slotOp (const char * v)
{
    if (strlen (v) > 7 && strncmp (v, "offset ", 7) == 0)
        return AsmPOffset (prog, labelObj (v + 7));
    if (! StrEmpty (v) && v [0] == '-')
        return AsmPImm (prog, strtol (v, NULL, 10));
    return AsmPImmU (prog, strtoul (v, NULL, 10));
}

/* kostra programu: hlavicka, extrn / public (ext_cur), datovy segment
   (data_cur), behova knihovna, kodovy segment - kurzor zustane na
   zacatku kodu; volat v run () (use_libc je znamo) */
static void startProgram (void)
{
    if (use_libc)
        AsmTextLine (prog, "; vygenerovano prekladacem cparser (instr.cc) pro assembler pasm -c a glibc");
    else
        AsmTextLine (prog, "; vygenerovano prekladacem cparser (instr.cc) pro assembler pasm -e");
    AsmDir (prog, ipX64, NULL, NULL);
    ext_cur = AsmTextLine (prog, "");
    if (use_libc) AsmTextLine (prog, "");
    data_cur = AsmDir (prog, iSegment, labelObj ("data"), "use64 para");
    if (use_libc) textLines (RTL_LIBC_DATA);
    else textLines (RTL_DATA);
    textLines (RTL_STR_DATA);
    AsmDir (prog, iEnds, labelObj ("data"), NULL);
    AsmTextLine (prog, "");
    if (use_libc)
    {
        AsmTextLine (prog, "; konstruktory globalnich objektu (glibc); destruktory registruje CINIT pres atexit");
        AsmDir (prog, iSegment, labelObj ("init_array"), "use64 para");
        AsmDataP (prog, NULL, iDq, AsmPOffset (prog, labelObj ("CINIT")));
        AsmDir (prog, iEnds, labelObj ("init_array"), NULL);
        AsmTextLine (prog, "");
    }
    AsmNode * code_start = AsmDir (prog, iSegment, labelObj ("code"), "use64 para");
    code_tail = AsmTextLine (prog, "");     /* misto pro RTL_LIBC_MAIN (main) - odebere se */
    if (use_libc) textLines (RTL_LIBC_CODE);
    else if (toaruTarget ()) textLinesToaru (RTL_CODE);
    else textLines (RTL_CODE);
    textLines (RTL_STR_CODE);
    AsmDir (prog, iEnds, labelObj ("code"), NULL);
    if (use_libc) AsmDir (prog, iEnd, NULL, NULL);
    else AsmDir (prog, iEnd, labelObj ("start"), NULL);
    AsmSetCursor (prog, code_start);
}

/* konec programu: extrn / public (az ted jsou zname), behova knihovna main */
static void finishProgram (void)
{
    long i;
    AsmNode * code = AsmSetCursor (prog, code_tail);
    if (use_libc && has_main) textLines (RTL_LIBC_MAIN);
    AsmRemove (prog, code_tail);
    if (code == code_tail) code = AsmCursor (prog);

    AsmSetCursor (prog, ext_cur);
    if (use_libc)
    {
        for (i = 0; i < SeqSize (&extern_keys); i++)
        {
            char * e = (char *) SeqGet (&extern_keys, i);
            AsmDir (prog, iExtrn, labelObj (SDictGetStr (&extern_labels, e)), Cat3 ("near '", e, "'"));
        }
        for (i = 0; i < SeqSize (&extern_data_keys); i++)
        {
            char * e = (char *) SeqGet (&extern_data_keys, i);
            if (SDictContains (&label_objs, SDictGetStr (&extern_data, e)))   /* jen pouzite: nepouzity nedefinovany
                                                                               symbol odmitne linker tcc */
                AsmDir (prog, iExtrn, labelObj (SDictGetStr (&extern_data, e)), Cat3 ("qword '", e, "'"));
        }
        if (has_main)
            AsmDir (prog, iPublic, labelObj ("CMAIN"), "'main'");
        for (i = 0; i < SeqSize (&public_labels); i++)
            AsmDir (prog, iPublic, labelObj ((char *) SeqGet (&public_labels, i)),
                    Cat3 ("'", (char *) SeqGet (&public_names, i), "'"));
    }
    AsmSetCursor (prog, code);
}

/* znaky v polich a za ukazateli zabiraji 1 bajt (char buf [N], char * p),
   skalarni promenne a polozky typu char zustavaji v 8-bajtovych slotech */
static bool isByteType (TypePtr type)
{
    return type != NULL && type->sort == Char;
}

/* l-hodnota typu char v bajtovem poli nebo za ukazatelem: a [i], *p */
/* ---- nacitani a ukladani podle typu ------------------------------------ */

/* l-hodnota v pameti: velikost v bajtech (prvek kontejneru - 8bajtovy slot) */
static long lvalueSize (ExprPtr e)
{
    e = SkipParens (e);
    if (e->op == LBRACK && IsContainer (SkipParens (e->e1)->type))
        return VAR_SIZE;
    return scalarSize (e->type);
}

/* cast registru dane velikosti (rax => eax, ax, al) */
static Register subReg (Register r, long size)
{
    if (size >= 8) return r;
    if (r >= RAX && r <= RDI)
    {
        int k = r - RAX;
        if (size == 4) return (Register) (EAX + k);
        if (size == 2) return (Register) (AX + k);
        return k < 4 ? (Register) (AL + k) : (Register) (SPL + k - 4);
    }
    int k = r - R8;
    if (size == 4) return (Register) (R8D + k);
    if (size == 2) return (Register) (R8W + k);
    return (Register) (R8B + k);
}

static AsmOp * sized_ptr (long size, Register b, long off) { return AsmPMem (prog, (int) size, b, off); }

/* rax := hodnota velikosti size na adrese rax (uns - rozsireni nulami, jinak znamenkem) */
static void loadSized (long size, bool uns)
{
    if (size == 1)
    {
        if (uns) ins2 (iMovzx, gp (EAX), sized_ptr (1, RAX, 0));
        else ins2 (iMovsx, gp (RAX), sized_ptr (1, RAX, 0));
    }
    else if (size == 2)
    {
        if (uns) ins2 (iMovzx, gp (EAX), sized_ptr (2, RAX, 0));
        else ins2 (iMovsx, gp (RAX), sized_ptr (2, RAX, 0));
    }
    else if (size == 4)
    {
        if (uns) ins2 (iMov, gp (EAX), sized_ptr (4, RAX, 0));
        else ins2 (iMovsxd, gp (RAX), sized_ptr (4, RAX, 0));
    }
    else
        ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));
}

/* rax = adresa l-hodnoty lv => jeji hodnota (float: 32bitovy obraz, horni bity 0) */
static void loadValue (ExprPtr lv)
{
    long size = lvalueSize (lv);
    loadSized (size, isUnsignedType (lv->type) || fpKind (lv->type) == 1);
}

/* [addr] := val (velikost size) */
static void storeSized (Register addr, Register val, long size)
{
    ins2 (iMov, sized_ptr (size, addr, 0), gp (subReg (val, size)));
}

/* rax := hodnota orezana na velikost size a rozsirena (uns - nulami, jinak znamenkem) */
static void normalizeSize (long size, bool uns)
{
    if (size == 1)
    {
        if (uns) ins2 (iMovzx, gp (EAX), gp (AL)); else ins2 (iMovsx, gp (RAX), gp (AL));
    }
    else if (size == 2)
    {
        if (uns) ins2 (iMovzx, gp (EAX), gp (AX)); else ins2 (iMovsx, gp (RAX), gp (AX));
    }
    else if (size == 4)
    {
        if (uns) ins2 (iMov, gp (EAX), gp (EAX)); else ins2 (iMovsxd, gp (RAX), gp (EAX));
    }
}

/* rax := cele cislo prevedene na typ t (preteceni jako v C, bool 0 / 1) */
static void normalizeTo (TypePtr t)
{
    if (!isIntegralType (t)) return;
    if (Resolve (t)->sort == Bool)
    {
        normalizeBool ();
        return;
    }
    normalizeSize (scalarSize (t), isUnsignedType (t));
}

/* obvykly aritmeticky prevod (C): velikost (4, 8) a znamenko spolecneho typu;
   char, short, bool => int; ukazatel - 8 bajtu bez znamenka (porovnani) */
static void promoted (TypePtr t, long * size, bool * uns)
{
    TypePtr r = Resolve (t);
    if (r != NULL && r->sort == Reference) r = Resolve (r->from);
    if (r == NULL || !isIntegralType (r))
    {
        * size = 8;
        * uns = r != NULL && (r->sort == Pointer || r->sort == Array);
        return;
    }
    * size = scalarSize (r);
    * uns = isUnsignedType (r);
    if (* size < 4) { * size = 4; * uns = false; }
}

static void arithType (TypePtr t1, TypePtr t2, long * size, bool * uns)
{
    long s1, s2;
    bool u1, u2;
    promoted (t1, &s1, &u1);
    if (t2 == NULL) { * size = s1; * uns = u1; return; }
    promoted (t2, &s2, &u2);
    if (s1 == s2) { * size = s1; * uns = u1 || u2; }
    else if (s1 > s2) { * size = s1; * uns = u1; }
    else { * size = s2; * uns = u2; }
}

/* kopie n bajtu z [rsi] do [rdi] */
static void copyBytes (long n)
{
    ins2 (iMov, gp (RCX), imm (n));
    insPrefix (iRep, iMovsb);
}

/* volani behove rutiny, rsp zarovnany na 16 (vyzaduje glibc) */
static void callRt (const char * label, const char * text)
{
    bool pad = (depth % 2) != 0;
    if (pad) ins2 (iSub, gp (RSP), imm (8));
    note (ins1 (iCall, jlab (label)), text);
    if (pad) ins2 (iAdd, gp (RSP), imm (8));
}

/* push / pop s pocitanim hloubky zasobniku */

static void push (Register r)
{
    ins1 (iPush, gp (r));
    depth ++;
}

static void pop (Register r)
{
    ins1 (iPop, gp (r));
    depth --;
}

/* ------------------------------------------------------------------ */

/* bajty retezce s koncovou nulou: label db 'text', 10, 0 */
static void emitBytes (const char * label, const char * value)
{
    Seq * items = SeqNew ();           /* AsmOp * - polozky DB */
    Seq * widths = SeqNew ();          /* delka polozky v textu pro pasm */
    char text [64];                    /* nejvyse 40 znaku */
    long tlen = 0;
    long vlen = strlen (value);
    long i;
    for (i = 0; i < vlen; i++)
    {
        long c = (unsigned char) value [i];
        if (c >= ' ' && c < 127 && c != '\'')
        {
            text [tlen ++] = (char) c;
            text [tlen] = 0;
            if (tlen >= 40)
            {
                SeqAdd (items, (long) AsmPStr (prog, text));
                SeqAdd (widths, tlen + 2);
                tlen = 0;
            }
        }
        else
        {
            if (tlen > 0)
            {
                text [tlen] = 0;
                SeqAdd (items, (long) AsmPStr (prog, text));
                SeqAdd (widths, tlen + 2);
                tlen = 0;
            }
            SeqAdd (items, (long) imm (c));
            SeqAdd (widths, 3);
        }
    }
    if (tlen > 0)
    {
        text [tlen] = 0;
        SeqAdd (items, (long) AsmPStr (prog, text));
        SeqAdd (widths, tlen + 2);
    }
    SeqAdd (items, (long) imm (0));
    SeqAdd (widths, 1);

    /* nejvyse 8 polozek na radku a radek kratsi nez LineLen = 160 znaku
       (pasm delsi radek orizne - retezec by prisel o konec a nulu) */
    AsmNode * save = dataBegin ();
    i = 0;
    while (i < SeqSize (items))
    {
        AsmLabel * l = NULL;
        if (i == 0) l = labelObj (label);
        AsmNode * n = AsmDataP (prog, l, iDb, (AsmOp *) SeqGet (items, i));
        long len = 16 + SeqGet (widths, i);            /* navesti, db, prvni polozka */
        long k = i + 1;
        while (k < SeqSize (items) && k < i + 8 && len + 2 + SeqGet (widths, k) < 140)
        {
            AsmDataAdd (n, (AsmOp *) SeqGet (items, k));
            len += 2 + SeqGet (widths, k);
            k ++;
        }
        i = k;
    }
    dataEnd (save);
}

/* retezcova konstanta do datoveho segmentu: db 'text', 10, 0 */
static char * stringLabel (const char * value)
{
    if (SDictContains (&string_labels, value))
        return SDictGetStr (&string_labels, value);

    char * label = newLabel ("S");
    SDictStore (&string_labels, value, (long) label);
    emitBytes (label, value);
    return label;
}

/* ------------------------------------------------------------------ */

/* externi funkce (glibc): navesti pro EXTRN */
static char * externLabel (const char * name)
{
    long i;
    if (SDictContains (&extern_labels, name))
        return SDictGetStr (&extern_labels, name);
    char * label = Cat ("LC_", name);
    bool ok = strlen (label) <= 31;
    for (i = 0; i < (long) strlen (name); i++)
    {
        char c = name [i];
        if (! (isalnum ((unsigned char) c) || c == '_')) ok = false;
    }
    /* pasm nerozlisuje velka a mala pismena: Abort (asm) a abort (glibc) */
    long llen = strlen (label);
    char * lower = (char *) MemAlloc (llen + 1);
    for (i = 0; i < llen; i++)
        lower [i] = (char) tolower ((unsigned char) label [i]);
    lower [llen] = 0;
    if (ok && SDictContains (&extern_lower, lower))
        ok = false;
    if (! ok)
        label = newLabel ("LCX");
    else
        SDictStore (&extern_lower, lower, 1);
    char * key = Str (name);
    SDictStore (&extern_labels, key, (long) label);
    insertSortedStr (&extern_keys, key);
    return label;
}

/* konstanta std::string do datoveho segmentu: delka, znaky, 0
   (navesti ukazuje na znaky, delka je 8 bajtu pred nimi) */
static char * stringConstLabel (const char * value)
{
    if (SDictContains (&sconst_labels, value))
        return SDictGetStr (&sconst_labels, value);

    char * label = newLabel ("SL");
    SDictStore (&sconst_labels, value, (long) label);
    AsmNode * save = dataBegin ();
    note (AsmDir (prog, iAlign, NULL, "qword"), "dolni bity = znacka docasnych hodnot (RT_TREL)");
    dataEnd (save);
    dataQ ("", imm (-1), "std::string - trvaly (pocet referenci)");
    dataQ ("", imm ((long) strlen (value)), "std::string");
    emitBytes (label, value);
    return label;
}

/* promenna / polozka se pri vytvoreni nuluje (prazdny retezec, kontejner) */
/* const string & - predava se jako hodnota (retezce se nemeni) */
static bool isValueRef (TypePtr t)
{
    return t != NULL && t->sort == Reference && IsStdString (t->from) && t->from->Const;
}

/* sizeof: znak 1 bajt, pole znaku n bajtu, ostatni promenne 8 bajtu
   (std::string, int, ukazatel, ...), struktury a pole podle typeCount */
static long byteSize (TypePtr type)
{
    if (type == NULL) return VAR_SIZE;
    return typeBytes (type);
}

/* vyctova konstanta:  enum K { a, b = 5, c };  =>  hodnota (c = 6) */
static bool enumValue (ObjectPtr obj, long * value)
{
    NodePtr p;
    if (obj == NULL || obj->fce || obj->type == NULL) return false;
    TypePtr typ = Resolve (obj->type);
    if (typ == NULL || typ->sort != Enum) return false;

    long v = 0;
    for (p = typ->list.first; p != NULL; p = p->next)
    {
        if (p->item->init != NULL && !constValue (p->item->init, &v))
            InstrError (Cat ("Constant expression expected: ", p->item->name));
        if (p->item == obj)
        {
            * value = v;
            return true;
        }
        v ++;
    }
    return false;   /* promenna vyctoveho typu */
}

static bool constValue (ExprPtr expr, long * value)
{
    if (expr == NULL) return false;

    long v1, v2;
    switch (expr->op)
    {
        case NUM:
            * value = (long) strtoul (expr->id, NULL, 0);   /* i 18446744073709551615UL; strtoul (ne strtoull) - ma prototyp v cmini/ownheaders/ (cmini) */
            return true;

        case CHR:
            * value = (signed char) expr->id [0];
            return true;

        case TRUE_:    * value = 1; return true;
        case FALSE_:   * value = 0; return true;
        case NULLPTR_: * value = 0; return true;

        case SIZEOF_:  /* velikost v bajtech podle modelu pameti instr.c */
            * value = byteSize (expr->e2 != NULL ? SkipParens (expr->e2)->type : expr->obj->type);
            return true;

        case LPAR:
            return constValue (expr->e1, value);

        case IDENT:   /* const int N = 100;  vyctova konstanta */
            if (enumValue (expr->obj, value))
                return true;
            if (IsConstInit (expr->obj) ||       /* const size_t N = 10;  (const u aliasu) */
                (expr->obj != NULL && expr->obj->init != NULL && IsStaticMember (expr->obj)))
                return constValue (expr->obj->init, value);
            return false;

        case UNARYPLUS:
            return constValue (expr->e2, value);

        case UNARYMINUS:
            if (!constValue (expr->e2, value)) return false;
            * value = - * value;
            return true;

        case NOT:       /* ~x (bitovy doplnek) */
            if (!constValue (expr->e2, value)) return false;
            * value = ~ * value;
            return true;

        case LOGNOT:    /* !x */
            if (!constValue (expr->e2, value)) return false;
            * value = ! * value;
            return true;

        case PLUS: case MINUS: case ASTERISK: case SLASH: case MOD:
        case SHL: case SHR: case AND: case OR: case XOR:
            if (!constValue (expr->e1, &v1) || !constValue (expr->e2, &v2)) return false;
            if ((expr->op == SLASH || expr->op == MOD) && v2 == 0) return false;
            switch (expr->op)
            {
                case PLUS:     * value = v1 + v2; break;
                case MINUS:    * value = v1 - v2; break;
                case ASTERISK: * value = v1 * v2; break;
                case SLASH:    * value = v1 / v2; break;
                case SHL:      * value = v1 << v2; break;
                case SHR:      * value = v1 >> v2; break;
                case AND:      * value = v1 & v2; break;
                case OR:       * value = v1 | v2; break;
                case XOR:      * value = v1 ^ v2; break;
                default:       * value = v1 % v2; break;
            }
            return true;

        default:
            return false;
    }
}

/* velikost prvku pole nebo cile ukazatele v bajtech (char => 1) */
static long elemSize (TypePtr type)
{
    if (type == NULL || type->sort == Void) return 1;   /* void * (jako gcc) */
    return typeBytes (type);
}

/* ---- velikosti jako v C (x86-64) ---------------------------------------
   char, bool 1 bajt, short 2, int, unsigned, vycet, float 4, long, ukazatel,
   double, std::string, Sequence, Dictionary (ukazatel na hlavicku) 8; struktury
   s prirozenym zarovnanim polozek. Hodnota v registru je 64bitova, rozsirena podle
   typu (int znamenkem, unsigned nulami - normalizeTo), float = 32bitovy obraz. */

/* velikost typu v bajtech */
static long typeBytes (TypePtr type)
{
    if (type == NULL) return VAR_SIZE;
    if (type->sort == Array)
    {
        long n;
        if (!constValue (type->range, &n) || n <= 0)
            InstrError ("Array size must be a positive constant");
        return n * typeBytes (type->from);
    }
    if (IsClsSort (type->sort))
        return structBytes (type);
    TypePtr r = Resolve (type);
    if (r != type) return typeBytes (r);
    return scalarSize (type);
}

static long typeAlign (TypePtr type)
{
    if (type == NULL) return 8;
    if (type->sort == Array) return typeAlign (type->from);
    if (IsClsSort (type->sort))
    {
        structBytes (type);
        return DictGet (&struct_aligns, (long) type);
    }
    TypePtr r = Resolve (type);
    if (r != type) return typeAlign (r);
    return scalarSize (type);
}

/* pocet 8-bajtovych mist, ktere typ zabira v ramci funkce a v datech */
static long typeCount (TypePtr type)
{
    return (typeBytes (type) + VAR_SIZE - 1) / VAR_SIZE;
}

/* ------------------------------------------------------------------ */

static char * className (TypePtr cls)
{
    return (cls != NULL && cls->tag != NULL) ? cls->tag : (char *) "?";
}

/* rozlozeni struktury jako v C: polozky baze, ukazatel na tabulku virtualnich metod
   (zavede ho prvni polymorfni trida v hierarchii), vlastni polozky - kazda zarovnana
   na svou velikost; vysledek v bajtech */
static long structBytes (TypePtr type)
{
    if (DictContains (&struct_sizes, (long) type))
    {
        long size = DictGet (&struct_sizes, (long) type);
        if (size < 0)
            InstrError ("Structure contains itself");
        return size;
    }
    if (!type->defined)
        InstrError (Cat ("Structure is not defined: ", className (type)));
    DictStore (&struct_sizes, (long) type, -1);      /* rozpracovano */

    long n = 0;
    long align = 1;
    if (type->base != NULL)
    {
        structBytes (type->base);
        n = DictGet (&struct_ends, (long) type->base);      /* baze na zacatku - stejne adresy */
        align = DictGet (&struct_aligns, (long) type->base);
    }

    if (IsPolymorphic (type))
    {
        if (BaseIsPolymorphic (type))
        {
            /* nejdriv hodnota z baze: zapis vptr_offsets [type] = vptr_offsets [type->base]
               by ukazal do pole, ktere vlozeni klice type->base prehashovanim uvolni */
            long base_off = DictGet (&vptr_offsets, (long) type->base);
            DictStore (&vptr_offsets, (long) type, base_off);
        }
        else
        {
            n = (n + 7) / 8 * 8;
            DictStore (&vptr_offsets, (long) type, n);
            n += 8;
        }
        if (align < 8) align = 8;
    }

    long max_union = 0;
    NodePtr p;
    for (p = type->list.first; p != NULL; p = p->next)
    {
        ObjectPtr m = p->item;
        if (! IsField (m))
            continue;
        if (m->width != NULL)
            InstrError (Cat ("Bit fields are not supported: ", m->name));
        if (m->type->sort == Reference)
            InstrError (Cat ("Reference members are not supported: ", m->name));

        long size = typeBytes (m->type);
        long a = typeAlign (m->type);
        if (a > align) align = a;
        if (type->sort == Union)
        {
            DictStore (&member_offsets, (long) m, 0);
            if (size > max_union) max_union = size;
        }
        else
        {
            n = (n + a - 1) / a * a;
            DictStore (&member_offsets, (long) m, n);
            n += size;
        }
    }
    if (type->sort == Union) n = max_union;
    DictStore (&struct_ends, (long) type, n);
    if (n == 0) n = 1;
    n = (n + align - 1) / align * align;
    DictStore (&struct_sizes, (long) type, n);
    DictStore (&struct_aligns, (long) type, align);
    return n;
}

/* pocet 8-bajtovych mist struktury (ramec, docasne misto) */
static long structCount (TypePtr type)
{
    return (structBytes (type) + VAR_SIZE - 1) / VAR_SIZE;
}

static long memberOffset (ObjectPtr member, TypePtr cls)
{
    structCount (cls);
    if (member == NULL || ! DictContains (&member_offsets, (long) member))
        InstrError ("Unknown structure member");
    return DictGet (&member_offsets, (long) member);
}

/* tabulka virtualnich metod: sloty baze, prekryte metody na stejnem miste,
   nove virtualni metody na konci */
static int vtableSlot (TypePtr cls, ObjectPtr m)
{
    int i = VtIndex (cls, m->name);   /* sem.c */
    if (i < 0) InstrError (Cat ("Virtual method not found: ", m->name));
    return i < 0 ? 0 : i;
}

static char * vtableLabel (TypePtr cls)
{
    if (DictContains (&vtable_labels, (long) cls))
        return DictGetStr (&vtable_labels, (long) cls);
    char * label = newLabel ("VT");
    DictStore (&vtable_labels, (long) cls, (long) label);
    insertSortedType (&vtable_keys, cls);
    return label;
}

/* tabulky virtualnich metod do datoveho segmentu */
static void codeVtables (void)
{
    /* tabulky vsech polymorfnich bazi (retez pro dynamic_cast) */
    bool added;
    long k;
    for (added = true; added; )
    {
        added = false;
        Seq * classes = SeqCopy (&vtable_keys);
        for (k = 0; k < SeqSize (classes); k++)
        {
            TypePtr c = (TypePtr) SeqGet (classes, k);
            if (BaseIsPolymorphic (c) &&
                ! DictContains (&vtable_labels, (long) c->base))
            {
                vtableLabel (c->base);
                added = true;
            }
        }
    }

    for (k = 0; k < SeqSize (&vtable_keys); k++)
    {
        TypePtr vt = (TypePtr) SeqGet (&vtable_keys, k);
        stringLabel (className (vt));               /* jmeno tridy pred tabulkou */
        dataComment (Cat ("vtable ", className (vt)));
        /* [VT - 16] = jmeno tridy, [VT - 8] = tabulka nadrazene polymorfni
           tridy nebo 0 (dynamic_cast - i mezi soubory, tabulky jsou v kazdem
           objektu zvlast) */
        char * name_label = stringLabel (className (vt));   /* emituje retezec */
        dataQ ("", AsmPOffset (prog, labelObj (name_label)), "jmeno tridy");
        TypePtr b = vt->base;
        if (BaseIsPolymorphic (vt))
            dataQ ("", AsmPOffset (prog, labelObj (DictGetStr (&vtable_labels, (long) b))), Cat ("base ", className (b)));
        else
            dataQ ("", imm (0), "bez nadrazene polymorfni tridy");
        int i;
        for (i = 0; i < VtCount (vt); i++)
        {
            ObjectPtr m = VtGet (vt, i)->impl;
            char * target;
            if (strcmp (m->name, "~") == 0)
            {
                /* destruktor: vzdy destruktor teto tridy (i implicitni) */
                TypePtr c = vt;
                if (c->dtor != NULL && c->dtor->pure && c->dtor->body == NULL)
                    target = (char *) "RT_PURE";
                else
                    target = dtorLabel (c);
            }
            else if (m->pure)
                target = (char *) "RT_PURE";
            else
                target = methodTarget (m, "Method");
            dataQ (i == 0 ? DictGetStr (&vtable_labels, (long) vt) : "", AsmPOffset (prog, labelObj (target)),
                   Cat3 (className (m->owner), "::", m->name));
        }
    }
}

/* ------------------------------------------------------------------ */

/* trida potrebuje konstruktor: uzivatelsky, tabulka virtualnich metod,
   konstruktor baze nebo vnorenych objektu */
/* navesti konstruktoru bez argumentu (implicitni se vygeneruje pozdeji) */
static char * ctorLabel (TypePtr cls)
{
    if (DictContains (&ctor_labels, (long) cls))
        return DictGetStr (&ctor_labels, (long) cls);

    char * label;
    if (cls->ctor != NULL)
        label = methodTarget (cls->ctor, "Constructor");
    else
    {
        label = newLabel ("C");
        SeqAdd (&implicit_ctors, (long) cls);
    }
    DictStore (&ctor_labels, (long) cls, (long) label);
    return label;
}

static void addDefaultArgs (ObjectPtr fce, Seq * args)
{
    addDefaultArgsT (fce->type, args, fce->name);
}

static void addDefaultArgsT (TypePtr ftype, Seq * args, const char * name)
{
    long i = 0;
    NodePtr p;
    for (p = ftype->list.first; p != NULL; p = p->next, i++)
    {
        ObjectPtr par = p->item;
        if (par->type == NULL || par->type->sort == Void) break;
        if (i >= SeqSize (args))
        {
            if (par->init == NULL)
                InstrError (Cat ("Too few arguments: ", name));
            SeqAdd (args, (long) par->init);
        }
    }
}

/* jmeno pro linker: funkce jako v C, metody  T::f,  T::T,  T::~T */
static char * symName (ObjectPtr f)
{
    if (f->owner == NULL) return f->name;
    char * cls = f->owner->tag != NULL ? f->owner->tag : (char *) "?";
    if (f->owner->ctor == f) return Cat3 (cls, "::", cls);
    if (f->owner->dtor == f) return Cat3 (cls, "::~", cls);
    return Cat3 (cls, "::", f->name);
}

/* verejna funkce objektu: ne static, ne inline, metoda jen definovana mimo
   tridu (tela uvnitr tridy jsou v hlavicce - v kazdem souboru) */
static bool exported (ObjectPtr f)
{
    if (f->body == NULL || f->storage == Static || f->Inline) return false;
    if (StrEq (f->name, "main") && f->owner == NULL) return false;   /* CMAIN */
    if (f->owner != NULL) return f->outdef;
    return true;
}

/* navesti metody, konstruktoru, destruktoru: telo v tomto souboru, jinak
   (objekt) externi symbol T::f */
static char * methodTarget (ObjectPtr m, const char * what)
{
    if (m->body != NULL && DictContains (&fce_labels, (long) m))
        return DictGetStr (&fce_labels, (long) m);
    if (objmode)
        return externLabel (symName (m));
    InstrError (Cat3 (what, " is not defined: ", symName (m)));
    return (char *) "";
}

/* adresa funkce (ukazatel na funkci):  f = add;  &add;  dq offset ... */
static char * fnLabel (ObjectPtr fce)
{
    if (fce->method)
        InstrError (Cat ("Pointers to methods are not supported: ", fce->name));
    if (fce->body != NULL)
    {
        /* fce_labels [fce] - chybejici klic vlozi "" (jako std::map) */
        if (! DictContains (&fce_labels, (long) fce))
            DictStore (&fce_labels, (long) fce, (long) "");
        return DictGetStr (&fce_labels, (long) fce);
    }
    if (fce->owner != NULL)
        return methodTarget (fce, "Method");
    if (use_libc)
        return externLabel (fce->name);                /* funkce z glibc */
    char * rt = rtlFunction (fce->name);
    if (StrEmpty (rt))
        InstrError (Cat ("Undefined function: ", fce->name));
    return rt;
}

/* volani pres ukazatel na funkci:  f (a),  (*f) (a),  ops [i] (a),  p->op (a),
   g (x) (y) - adresa funkce se schova na zasobnik, argumenty jako u
   primeho volani (System V), call r11 */
static void codeIndirectCall (ExprPtr expr, ExprPtr f)
{
    ExprPtr callee = (f->op == DEREF) ? f->e2 : f;
    TypePtr ft = Resolve (callee->type);
    TypePtr ftype = (ft->sort == Pointer) ? Resolve (ft->from) : ft;
    if (ftype == NULL || ftype->sort != Function)
        InstrError ("Call of nonfunction");
    if (isStructVal (ftype->from))
        InstrError ("Structure returned by value through function pointer is not supported");

    codeExpr (callee);                          /* adresa funkce */
    push (RAX);
    long fdepth = depth;

    Seq * args = SeqNew ();
    collectArgs (expr->e2, args);
    addDefaultArgsT (ftype, args, "(function pointer)");
    Seq * ptypes = SeqNew ();
    NodePtr par = ftype->list.first;
    long i;
    for (i = 0; i < SeqSize (args); i++)
    {
        SeqAdd (ptypes, (long) (par != NULL ? par->item->type : NULL));
        if (par != NULL && par->item->type != NULL) par = par->next;
    }
    int nxmm = 0;
    long drop = passArgs (NULL, args, ptypes, &nxmm, 0);

    note (ins2 (iMov, gp (R11), qword_ptr (RSP, 8 * (depth - fdepth))), "adresa funkce");
    bool pad = (depth % 2) != 0;
    if (pad) ins2 (iSub, gp (RSP), imm (8));
    if (nxmm == 0) ins2 (iXor, gp (EAX), gp (EAX));
    else ins2 (iMov, gp (EAX), imm (nxmm));
    ins1 (iCall, gp (R11));
    if (pad) ins2 (iAdd, gp (RSP), imm (8));
    if (drop > 0)
    {
        ins2 (iAdd, gp (RSP), imm (drop * VAR_SIZE));
        depth -= drop;
    }
    ins2 (iAdd, gp (RSP), imm (8));                     /* adresa funkce */
    depth --;

    TypePtr res = ftype->from;
    if (fpKind (res) != 0)
        fromXmm (fpKind (res));
    else if (ownedTemp (res))
        ;
    else if (res != NULL && isIntegralType (res))
        normalizeTo (res);      /* cil muze byt i funkce z glibc - vysledek jen v dolni casti rax */
}

/* rax = adresa objektu => konstruktor s argumenty, vysledek rax = this */
/* konstruktor bez argumentu */
static void callCtor0 (TypePtr cls)
{
    Seq * none = SeqNew ();
    callCtor (cls, none);
}

static void callCtor (TypePtr cls, Seq * args_in)
{
    /* drive Sequence hodnotou: addDefaultArgs meni jen kopii */
    Seq * args = SeqCopy (args_in);

    ObjectPtr ctor = cls->ctor;
    if (ctor == NULL)
    {
        if (! SeqEmpty (args))
            InstrError (Cat ("Constructor with arguments is not defined: ", className (cls)));
        ins2 (iMov, gp (RDI), gp (RAX));
        note (ins1 (iCall, jlab (ctorLabel (cls))), Cat (className (cls), " ()"));
        return;
    }
    char * ctor_label = methodTarget (ctor, "Constructor");

    addDefaultArgs (ctor, args);

    Seq * ptypes = SeqNew ();
    NodePtr par = ctor->type->list.first;
    long i;
    for (i = 0; i < SeqSize (args); i++)
    {
        SeqAdd (ptypes, (long) (par != NULL ? par->item->type : NULL));
        if (par != NULL) par = par->next;
    }

    push (RAX);                                  /* this - schovany na zasobniku */
    long this_depth = depth;
    ThisCode * code_this = (ThisCode *) MemAlloc (sizeof (ThisCode));
    code_this->kind = THIS_STACK;
    code_this->this_depth = this_depth;
    int nxmm = 0;
    long drop = passArgs (code_this, args, ptypes, &nxmm, 0);

    bool pad = (depth % 2) != 0;
    if (pad) ins2 (iSub, gp (RSP), imm (8));
    note (ins1 (iCall, jlab (ctor_label)), Cat (className (cls), " (...)"));   /* vraci this */
    if (pad) ins2 (iAdd, gp (RSP), imm (8));
    if (drop > 0)
    {
        ins2 (iAdd, gp (RSP), imm (drop * VAR_SIZE));
        depth -= drop;
    }
    ins2 (iAdd, gp (RSP), imm (8));                        /* this */
    depth --;
}

/* rax = adresa promenne => konstruktory (objekt nebo pole objektu) */
static void constructVar (TypePtr type)
{
    TypePtr elem = ArrayElem (type);
    if (elem == NULL || !IsClsSort (elem->sort) || !NeedsCtor (elem, true))
        return;

    if (type->sort != Array)
    {
        callCtor0 (type);
        return;
    }

    /* pole objektu: cyklus pres prvky, [rsp+8] = adresa, [rsp] = pocet */
    long size = structBytes (elem);
    long count = typeBytes (type) / size;
    char * loop_label = newLabel ("L");

    push (RAX);
    ins2 (iMov, gp (RCX), imm (count));
    push (RCX);
    bind (loop_label);
    ins2 (iMov, gp (RAX), qword_ptr (RSP, 8));
    callCtor0 (elem);
    ins2 (iAdd, qword_ptr (RSP, 8), imm (size));
    ins1 (iDec, qword_ptr (RSP, 0));
    ins1 (iJnz, jlab (loop_label));
    ins2 (iAdd, gp (RSP), imm (16));
    depth -= 2;
}

/* rax := hodnota std::string z vyrazu typu std::string, retezcove konstanty,
   char * nebo char */
static void codeStringValue (ExprPtr e)
{
    ExprPtr x = SkipParens (e);
    if (IsStdString (e->type))
        codeExpr (e);
    else if (x->op == STR)
        ins2 (iLea, gp (RAX), alab (stringConstLabel (x->id)));   /* konstanta - bez alokace */
    else if (e->type != NULL && IsIntegerSort (e->type->sort))   /* znak */
    {
        codeExpr (e);
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt ("RT_SCHR", "");
        strTemp ();
    }
    else                                                     /* char * */
    {
        codeExpr (e);
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt ("RT_SFROMC", "");
        strTemp ();
    }
}

/* rax = adresa => vynulovani promenne (prazdne retezce) */
static void zeroVar (TypePtr type)
{
    long count = typeCount (type);
    if (count == 1)
    {
        ins2 (iMov, qword_ptr (RAX, 0), imm (0));
        return;
    }
    char * loop_label = newLabel ("L");
    ins2 (iMov, gp (RCX), imm (count));
    bind (loop_label);
    ins2 (iMov, qword_ptr (RAX, 0), imm (0));
    ins2 (iAdd, gp (RAX), imm (8));
    ins1 (iDec, gp (RCX));
    ins1 (iJnz, jlab (loop_label));
}

/* zacatek konstruktoru: konstruktor baze, tabulka virtualnich metod,
   konstruktory vnorenych objektu (potom inicializace a telo) */
static void codeCtorPrologue (TypePtr cls, ObjectPtr ctor)
{
    if (ctor != NULL && ctor->base_init)
    {
        /* T (...) : Base (args) */
        Seq * args = SeqNew ();
        collectArgs (ctor->base_args, args);
        loadThis ();
        callCtor (cls->base, args);
    }
    else if (cls->base != NULL && NeedsCtor (cls->base, true))
    {
        loadThis ();
        callCtor0 (cls->base);
    }

    if (IsPolymorphic (cls))
    {
        structCount (cls);
        loadThis ();
        note (ins2 (iLea, gp (RCX), alab (vtableLabel (cls))), Cat ("vtable ", className (cls)));
        ins2 (iMov, qword_ptr (RAX, DictGet (&vptr_offsets, (long) cls)), gp (RCX));
    }

    NodePtr p;
    for (p = cls->list.first; p != NULL; p = p->next)
    {
        ObjectPtr m = p->item;
        if (! IsField (m)) continue;
        TypePtr t = ArrayElem (m->type);
        if (isZeroInit (t))              /* std::string, kontejner => prazdny */
        {
            long off = memberOffset (m, cls);
            loadThis ();
            if (off != 0) note (ins2 (iAdd, gp (RAX), imm (off)), m->name);
            zeroVar (m->type);
            continue;
        }
        if (t == NULL || !IsClsSort (t->sort) || !NeedsCtor (t, true)) continue;
        long off = memberOffset (m, cls);
        loadThis ();
        if (off != 0) note (ins2 (iAdd, gp (RAX), imm (off)), m->name);
        constructVar (m->type);
    }
}

/* implicitni konstruktor (trida bez uzivatelskeho konstruktoru) */
static void codeImplicitCtor (TypePtr cls)
{
    current_class = cls;
    this_obj = newObj ("this", Link (Pointer, cls));
    frame_size = 0;
    depth = 0;
    allocLocal (this_obj);

    blank ();
    comment (Cat ("implicit constructor ", className (cls)));
    bind (DictGetStr (&ctor_labels, (long) cls));
    ins1 (iPush, gp (RBP));
    ins2 (iMov, gp (RBP), gp (RSP));
    ins2 (iSub, gp (RSP), imm (16));
    note (ins2 (iMov, qword_ptr (RBP, ((Variable *) DictGet (&vars, (long) this_obj))->offset), gp (RDI)), "this");
    codeCtorPrologue (cls, NULL);
    loadThis ();
    ins2 (iMov, gp (RSP), gp (RBP));
    ins1 (iPop, gp (RBP));
    ins0 (iRet);

    current_class = NULL;
    this_obj = NULL;
}

/* ---- destruktory ---------------------------------------------------- */

/* navesti destruktoru (implicitni se vygeneruje na konci) */
static char * dtorLabel (TypePtr cls)
{
    if (DictContains (&dtor_labels, (long) cls))
        return DictGetStr (&dtor_labels, (long) cls);
    char * label;
    if (cls->dtor != NULL)
        label = methodTarget (cls->dtor, "Destructor");
    else
    {
        label = newLabel ("D");
        SeqAdd (&implicit_dtors, (long) cls);
    }
    DictStore (&dtor_labels, (long) cls, (long) label);
    return label;
}

/* rax = adresa promenne => destruktory (objekt, pole od posledniho prvku),
   std::string - reference -1, Sequence / Dictionary - uvolneni */
static void destructVar (TypePtr type)
{
    TypePtr elem = ArrayElem (type);
    if (isZeroInit (elem))
    {
        char * rt = IsStdString (elem) ? (char *) "RT_SDEC" : IsDictionary (elem) ? (char *) "RT_DFREE" : (char *) "RT_QFREE";
        long count = typeCount (type);
        if (count == 1)
        {
            ins2 (iMov, gp (RDI), qword_ptr (RAX, 0));
            callRt (rt, "");
            return;
        }
        char * loop_label = newLabel ("L");
        push (RAX);
        ins2 (iMov, gp (RCX), imm (count));
        push (RCX);
        bind (loop_label);
        ins2 (iMov, gp (RAX), qword_ptr (RSP, 8));
        ins2 (iMov, gp (RDI), qword_ptr (RAX, 0));
        callRt (rt, "");
        ins2 (iAdd, qword_ptr (RSP, 8), imm (8));
        ins1 (iDec, qword_ptr (RSP, 0));
        ins1 (iJnz, jlab (loop_label));
        ins2 (iAdd, gp (RSP), imm (16));
        depth -= 2;
        return;
    }
    if (!NeedsDtor (elem, true))
        return;

    if (type->sort != Array)
    {
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt (dtorLabel (elem), Cat ("~", className (elem)));
        return;
    }

    long size = structBytes (elem);
    long count = typeBytes (type) / size;
    char * loop_label = newLabel ("L");
    if (count > 1)
        ins2 (iAdd, gp (RAX), imm ((count - 1) * size));
    push (RAX);
    ins2 (iMov, gp (RCX), imm (count));
    push (RCX);
    bind (loop_label);
    ins2 (iMov, gp (RDI), qword_ptr (RSP, 8));
    callRt (dtorLabel (elem), Cat ("~", className (elem)));
    ins2 (iSub, qword_ptr (RSP, 8), imm (size));
    ins1 (iDec, qword_ptr (RSP, 0));
    ins1 (iJnz, jlab (loop_label));
    ins2 (iAdd, gp (RSP), imm (16));
    depth -= 2;
}

/* konec destruktoru: vnorene objekty (v opacnem poradi), potom baze */
static void codeDtorEpilogue (TypePtr cls)
{
    Seq * members = SeqNew ();       /* ObjectPtr */
    NodePtr p;
    for (p = cls->list.first; p != NULL; p = p->next)
    {
        ObjectPtr m = p->item;
        if (! IsField (m)) continue;
        if (NeedsDtor (ArrayElem (m->type), true) || isZeroInit (ArrayElem (m->type)))
            SeqAdd (members, (long) m);
    }
    long i;
    for (i = SeqSize (members); i-- > 0; )
    {
        ObjectPtr m = (ObjectPtr) SeqGet (members, i);
        long off = memberOffset (m, cls);
        loadThis ();
        if (off != 0) note (ins2 (iAdd, gp (RAX), imm (off)), m->name);
        destructVar (m->type);
    }
    if (cls->base != NULL && NeedsDtor (cls->base, true))
    {
        loadThis ();
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt (dtorLabel (cls->base), Cat ("~", className (cls->base)));   /* bez virtualniho volani */
    }
}

/* implicitni destruktor (trida bez uzivatelskeho destruktoru) */
static void codeImplicitDtor (TypePtr cls)
{
    current_class = cls;
    this_obj = newObj ("this", Link (Pointer, cls));
    frame_size = 0;
    depth = 0;
    allocLocal (this_obj);

    blank ();
    comment (Cat ("implicit destructor ", className (cls)));
    bind (DictGetStr (&dtor_labels, (long) cls));
    ins1 (iPush, gp (RBP));
    ins2 (iMov, gp (RBP), gp (RSP));
    ins2 (iSub, gp (RSP), imm (16));
    note (ins2 (iMov, qword_ptr (RBP, ((Variable *) DictGet (&vars, (long) this_obj))->offset), gp (RDI)), "this");
    codeDtorEpilogue (cls);
    ins2 (iMov, gp (RSP), gp (RBP));
    ins1 (iPop, gp (RBP));
    ins0 (iRet);

    current_class = NULL;
    this_obj = NULL;
}

/* delete p:  p != nullptr => destruktor (virtualni pres tabulku), free */
static void codeDelete (ExprPtr expr)
{
    TypePtr T = expr->e2->type->from;
    bool arr = expr->id != NULL && expr->id [0] != 0;
    if (T == NULL || !IsClsSort (T->sort) || !NeedsDtor (T, true))
    {
        codeExpr (expr->e2);
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt ("RT_FREE", "");
        return;
    }
    if (arr)
        InstrError (Cat ("delete [] is not supported for objects with destructor: ", className (T)));

    char * skip = newLabel ("L");
    codeExpr (expr->e2);
    ins2 (iTest, gp (RAX), gp (RAX));
    ins1 (iJz, jlab (skip));
    push (RAX);
    ins2 (iMov, gp (RDI), gp (RAX));
    int slot = VtIndex (T, (char *) "~");
    if (slot >= 0)
    {
        structCount (T);
        bool pad = (depth % 2) != 0;
        if (pad) ins2 (iSub, gp (RSP), imm (8));
        note (ins2 (iMov, gp (R11), qword_ptr (RDI, DictGet (&vptr_offsets, (long) T))), "vtable");
        note (ins1 (iCall, qword_ptr (R11, slot * VAR_SIZE)), Cat ("virtual ~", className (T)));
        if (pad) ins2 (iAdd, gp (RSP), imm (8));
    }
    else
        callRt (dtorLabel (T), Cat ("~", className (T)));
    pop (RDI);
    callRt ("RT_FREE", "");
    bind (skip);
}

/* destruktory globalnich (i statickych lokalnich) objektu - po main:
   glibc atexit (i pri exit), vlastni knihovna po navratu z main */
static void codeGlobalFini (void)
{
    blank ();
    comment ("destructors of global objects");
    bind ("CFINI");
    ins1 (iPush, gp (RBP));
    ins2 (iMov, gp (RBP), gp (RSP));
    depth = 0;
    long i;
    for (i = SeqSize (&global_dtor_objects); i-- > 0; )
    {
        ObjectPtr obj = (ObjectPtr) SeqGet (&global_dtor_objects, i);
        note (ins2 (iLea, gp (RAX), alab (((Variable *) DictGet (&vars, (long) obj))->label)), Cat ("~", obj->name));
        destructVar (obj->type);
    }
    ins1 (iPop, gp (RBP));
    ins0 (iRet);
}

/* konstruktory globalnich objektu - vola start pred main */
static void codeGlobalInit (void)
{
    blank ();
    comment ("constructors of global objects");
    bind ("CINIT");
    ins1 (iPush, gp (RBP));
    ins2 (iMov, gp (RBP), gp (RSP));
    depth = 0;
    long i;
    for (i = 0; i < SeqSize (&global_objects); i++)
    {
        ObjectPtr obj = (ObjectPtr) SeqGet (&global_objects, i);
        note (ins2 (iLea, gp (RAX), alab (((Variable *) DictGet (&vars, (long) obj))->label)), obj->name);
        constructVar (obj->type);
    }
    ins2 (iXor, gp (EDI), gp (EDI));
    ins1 (iCall, jlab ("RT_TREL"));                 /* docasne hodnoty konstruktoru */
    if (use_libc)
    {
        /* destruktory pres atexit, ne .fini_array: libc ToaruOS 2.3.2 .fini_array
           pri ukonceni nevola (ani u programu z gcc), atexit ano; poradi jako
           drive - CFINI je registrovana pred main, probehne po handlerech z main */
        note (ins2 (iLea, gp (RDI), alab ("CFINI")), "atexit (CFINI)");
        ins1 (iCall, jlab ("LC_atexit"));
    }
    ins1 (iPop, gp (RBP));
    ins0 (iRet);
}

/* ------------------------------------------------------------------ */

/* staticka inicializace (data): hodnoty 8-bajtovych slotu podle rozlozeni
   pameti - cela cisla, "offset navesti" (retezcove konstanty), pole znaku
   po bajtech; chybejici prvky = 0 */
static void flattenList (ExprPtr e, Seq * items)
{
    if (e == NULL) return;
    if (e->op == COMMA)
    {
        flattenList (e->e1, items);
        flattenList (e->e2, items);
    }
    else
        SeqAdd (items, (long) e);
}

/* staticka inicializace: pos je posun v bajtech (rozlozeni jako v C - char 1,
   short 2, int 4, ...), slots = 8bajtova mista dat (dq, char *); hodnota se zapise po
   bajtech do mist, ktera zasahuje */
static void staticBytes (Seq * slots, long pos, unsigned long value, long size, const char * name)
{
    long i;
    for (i = 0; i < size; i++)
    {
        long slot = (pos + i) / VAR_SIZE;
        if (slot >= SeqSize (slots))
            InstrError (Cat ("Static initializer out of object: ", name));
        char * s = (char *) SeqGet (slots, slot);
        if (strncmp (s, "offset ", 7) == 0)
            InstrError (Cat ("Static initializer overlaps address: ", name));
        unsigned long v = strtoul (s, NULL, 10);
        v |= ((value >> (8 * i)) & 0xFF) << (8 * ((pos + i) % VAR_SIZE));
        SeqSet (slots, slot, (long) UIntStr (v));
    }
}

/* adresa (offset navesti) - cele 8bajtove misto, ukazatel je zarovnany na 8 */
static void staticOffset (Seq * slots, long pos, const char * label, const char * name)
{
    if (pos % VAR_SIZE != 0 || pos / VAR_SIZE >= SeqSize (slots))
        InstrError (Cat ("Unaligned static address initializer: ", name));
    SeqSet (slots, pos / VAR_SIZE, (long) Cat ("offset ", label));
}

static void staticInit (TypePtr type, ExprPtr init, Seq * slots, long pos, const char * name)
{
    if (init == NULL) return;
    init = SkipParens (init);
    long value;
    long i;

    if (type->sort == Array)
    {
        TypePtr elem = type->from;
        long n;
        if (!constValue (type->range, &n)) InstrError ("Array size must be a positive constant");

        if (isByteType (elem) && init->op == LBRACE)  /* unsigned char b [n] = { 0x7F, 'E', ... } */
        {
            Seq * items = SeqNew ();
            flattenList (init->e1, items);
            if (SeqSize (items) > n)
                InstrError (Cat ("Too many initializers: ", name));
            for (i = 0; i < SeqSize (items); i++)
            {
                if (!constValue ((ExprPtr) SeqGet (items, i), &value))
                    InstrError (Cat ("Only constant initializers are supported: ", name));
                staticBytes (slots, pos + i, (unsigned long) value, 1, name);
            }
            return;
        }
        if (isByteType (elem))                        /* char s [n] = "abc" */
        {
            if (init->op != STR)
                InstrError (Cat ("Character array must be initialized by string constant: ", name));
            char * txt = init->id;
            long len = strlen (txt);
            for (i = 0; i < n && i < len; i++)
                staticBytes (slots, pos + i, (unsigned long) (unsigned char) txt [i], 1, name);
            return;
        }

        if (init->op != LBRACE)
            InstrError (Cat ("Array initializer { ... } expected: ", name));
        Seq * items = SeqNew ();
        flattenList (init->e1, items);
        if (SeqSize (items) > n)
            InstrError (Cat ("Too many initializers: ", name));
        long step = typeBytes (elem);                 /* prvky po bajtech jako v C */
        for (i = 0; i < SeqSize (items); i++)
            staticInit (elem, (ExprPtr) SeqGet (items, i), slots, pos + i * step, name);
        return;
    }

    if (IsClsSort (type->sort))
    {
        if (init->op != LBRACE)
            InstrError (Cat ("Structure initializer { ... } expected: ", name));
        if (IsPolymorphic (type) || type->base != NULL)
            InstrError (Cat ("Initialization of polymorphic or derived objects is not supported: ", name));
        structCount (type);                            /* rozlozeni (member_offsets) */
        Seq * items = SeqNew ();
        flattenList (init->e1, items);
        long k = 0;
        NodePtr p;
        for (p = type->list.first; p != NULL && k < SeqSize (items); p = p->next)
        {
            ObjectPtr m = p->item;
            if (! IsField (m))
                continue;
            staticInit (m->type, (ExprPtr) SeqGet (items, k), slots, pos + DictGet (&member_offsets, (long) m), name);
            k ++;
        }
        if (k < SeqSize (items))
            InstrError (Cat ("Too many initializers: ", name));
        return;
    }

    if (IsContainer (type))
        InstrError (Cat ("Initialization of global Sequence / Dictionary is not supported: ", name));

    if (IsStdString (type))                                  /* std::string */
    {
        if (init->op != STR)
            InstrError (Cat ("Only string constants can initialize static strings: ", name));
        staticOffset (slots, pos, stringConstLabel (init->id), name);
        return;
    }

    if (init->op == STR && type->sort == Pointer)      /* const char * p = "abc" */
    {
        staticOffset (slots, pos, stringLabel (init->id), name);
        return;
    }

    {
        ExprPtr x = (init->op == ADR) ? SkipParens (init->e2) : init;
        if (x->op == IDENT && x->obj != NULL && x->obj->fce)   /* BinOp f = add; */
        {
            staticOffset (slots, pos, fnLabel (x->obj), name);
            return;
        }
        /* static char * argv [] = { prog, nullptr }; char * p = &g;  (adresa globalni promenne
           nebo statickeho pole - konstanta; pole se rozpadne na ukazatel) */
        if (x->op == IDENT && x->obj != NULL && type->sort == Pointer && DictContains (&vars, (long) x->obj) &&
            (init->op == ADR || (x->obj->type != NULL && x->obj->type->sort == Array)))
        {
            staticOffset (slots, pos, ((Variable *) DictGet (&vars, (long) x->obj))->label, name);
            return;
        }
    }

    if (fpKind (type) != 0)                            /* double d = 1.5; float f = 1.5f; */
    {
        double v;
        if (!fpConstValue (init, &v))
            InstrError (Cat ("Only constant initializers are supported: ", name));
        staticBytes (slots, pos, fpBits (v, fpKind (type)), typeBytes (type), name);
        return;
    }

    if (!constValue (init, &value))
    {
        double v;
        if (fpConstValue (init, &v) && fpKind (init->type) != 0)   /* int i = 2.5; */
            value = (long) v;
        else
            InstrError (Cat ("Only constant initializers are supported: ", name));
    }
    if (Resolve (type) != NULL && Resolve (type)->sort == Bool)  /* bool b = 5; => 1 */
        value = (value != 0) ? 1 : 0;
    staticBytes (slots, pos, (unsigned long) value, typeBytes (type), name);
}

/* globalni (i staticka lokalni) promenna v datovem segmentu */
static void allocGlobal (ObjectPtr obj)
{
    long i;
    if (obj->type->sort == Reference)
        InstrError (Cat ("Global references are not supported: ", obj->name));

    if (objmode && obj->storage == Extern)
    {
        /* extern int x;  - definice je v jinem souboru */
        Variable * var = Variable_new ();
        var->global = true;
        char * name = obj->name;
        if (SDictContains (&extern_data, name))
            var->label = SDictGetStr (&extern_data, name);
        else
        {
            var->label = newLabel ("LD");
            SDictStore (&extern_data, name, (long) var->label);
            insertSortedStr (&extern_data_keys, Str (name));
        }
        DictStore (&vars, (long) obj, (long) var);
        return;
    }

    long count = typeCount (obj->type);

    TypePtr elem = ArrayElem (obj->type);
    if (elem != NULL && IsClsSort (elem->sort) && NeedsCtor (elem, true))
    {
        if (obj->init == NULL)
            SeqAdd (&global_objects, (long) obj);           /* konstruktor pred main (CINIT) */
        else if (elem->ctor != NULL)
            InstrError (Cat ("Initialization of objects with constructor is not supported: ", obj->name));
        /* jinak staticka inicializace (implicitni konstruktor jen nuluje) */
    }

    if (elem != NULL && IsClsSort (elem->sort) && NeedsDtor (elem, true))
        SeqAdd (&global_dtor_objects, (long) obj);          /* destruktor po main (CFINI) */

    Seq * slots = SeqNew ();                    /* char * */
    for (i = 0; i < count; i++)
        SeqAdd (slots, (long) "0");
    staticInit (obj->type, obj->init, slots, 0, obj->name);

    Variable * var = Variable_new ();
    var->global = true;
    var->label = newLabel ("G");
    DictStore (&vars, (long) obj, (long) var);
    /* globalni promenna (ne static; konstanta ma v C++ vnitrni linkovani) */
    TypePtr ct = ArrayElem (obj->type);
    bool constant = (ct != NULL && ct->Const) || obj->type->Const ||
                    (obj->desc != NULL && obj->desc->Const);
    if (obj->extern_decl) constant = false;   /* extern const char T []; v hlavicce - vnejsi vazba */
    if (objmode && obj->storage != Static && obj->owner == NULL && current_fce == NULL && !constant)
        addPublic (var->label, obj->name);

    dataComment (obj->name);
    bool all_zero = true;
    for (i = 0; i < SeqSize (slots); i++)
        if (! StrEq ((char *) SeqGet (slots, i), "0")) all_zero = false;

    if (count == 1 || !all_zero)
    {
        for (i = 0; i < count; i++)
            dataQ (i == 0 ? var->label : "", slotOp ((char *) SeqGet (slots, i)), "");
    }
    else
    {
        /* pocet opakovani DUP je v pasm 16bitovy => po castech */
        long chunk = 30000;
        long done;
        for (done = 0; done < count; done += chunk)
            dataQ (done == 0 ? var->label : "", AsmPDup (prog, (int) (count - done < chunk ? count - done : chunk), 0), "");
    }
}

static void allocGlobals (void)
{
    NodePtr p;
    for (p = glist.first; p != NULL; p = p->next)
    {
        ObjectPtr obj = p->item;
        if (obj->fce || obj->storage == Typedef || obj->name [0] == 0)
            continue;          /* funkce, typedef, struct X { ... }; */
        allocGlobal (obj);
    }
}

static void allocLocal (ObjectPtr obj)
{
    Variable * var = Variable_new ();
    var->ref = obj->type->sort == Reference && !isValueRef (obj->type);

    long count = (obj->type->sort == Reference) ? 1 : typeCount (obj->type);
    frame_size += count * VAR_SIZE;
    var->offset = - frame_size;     /* pole: prvky od [rbp+offset] smerem nahoru */
    DictStore (&vars, (long) obj, (long) var);
}

static void collectLocalDecls (Queue * decl)
{
    NodePtr p;
    for (p = decl->first; p != NULL; p = p->next)
    {
        ObjectPtr obj = p->item;
        if (obj->fce || obj->storage == Typedef || obj->storage == Extern)
            continue;
        if (obj->storage == Static)
        {
            allocGlobal (obj);       /* staticka lokalni promenna = data, inicializace jednou */
            continue;
        }
        if (obj->name [0] == 0)
            continue;
        allocLocal (obj);
    }
}

/* projdi telo funkce a prirad lokalnim promennym misto v ramci */
static void collectLocals (StatPtr stat)
{
    StatPtr s;
    if (stat == NULL) return;

    switch (stat->st)
    {
        case DECL:
            collectLocalDecls (&stat->decl);
            break;
        case LBRACE:
            for (s = stat->s1; s != NULL; s = s->next)
                collectLocals (s);
            break;
        case FOR_:
            collectLocalDecls (&stat->decl);
            collectLocals (stat->s1);
            break;
        case COLON:                        /* for (T x : seq) - promenna a 2 skryte */
            collectLocalDecls (&stat->decl);
            frame_size += 2 * VAR_SIZE;
            DictStore (&range_slots, (long) stat, - frame_size);
            collectLocals (stat->s1);
            break;
        case IF_:
            collectLocals (stat->s1);
            collectLocals (stat->s2);
            break;
        default:
            collectLocals (stat->s1);
            break;
    }
}

static Variable * variable (ExprPtr expr)
{
    if (expr->obj == NULL || ! DictContains (&vars, (long) expr->obj))
        InstrError (Cat ("Unknown variable: ", expr->id));
    return (Variable *) DictGet (&vars, (long) expr->obj);
}

/* ------------------------------------------------------------------ */

static void codeDecl (ObjectPtr obj)
{
    if (obj->fce && obj->body != NULL)
        codeFunction (obj);
}

static void codeFunction (ObjectPtr decl)
{
    long i;
    long q;
    NodePtr p;
    Register arg_regs [6];
    arg_regs [0] = RDI;
    arg_regs [1] = RSI;
    arg_regs [2] = RDX;
    arg_regs [3] = RCX;
    arg_regs [4] = R8;
    arg_regs [5] = R9;

    while (SeqSize (&pending_temps) > 0) SeqPop (&pending_temps);

    current_fce = decl;
    SeqClear (&cleanup);
    InstrContext = (decl->owner != NULL && decl->owner->tag != NULL)
                   ? Cat3 (decl->owner->tag, "::", decl->name) : Str (decl->name);
    DictClear (&user_labels);
    epilogue_label = newLabel ("L");
    frame_size = 0;
    depth = 0;

    /* parametry */
    Seq * params = SeqNew ();                  /* ObjectPtr */
    for (p = decl->type->list.first; p != NULL; p = p->next)
    {
        ObjectPtr par = p->item;
        if (par->type == NULL)
            InstrError (Cat ("Variable number of parameters is not supported: ", decl->name));
        if (par->type->sort == Void)
            break;          /* int f (void) */
        if (par->type->sort == Array)
            InstrError (Cat ("Array parameters are not supported yet: ", decl->name));
        SeqAdd (params, (long) par);
    }

    /* vysledek - struktura hodnotou: skryty parametr (adresa), za this */
    DictClear (&inptr_offsets);
    ret_obj = NULL;
    if (isStructVal (decl->type->from))
    {
        checkStructVal (decl->type->from);
        ret_obj = newObj ("$ret", Link (Pointer, decl->type->from));
        SeqInsertFirst (params, (long) ret_obj);
    }

    /* metoda nebo konstruktor: skryty parametr this */
    bool ctor = decl->owner != NULL && decl->owner->ctor == decl;
    current_class = (decl->method || ctor) ? decl->owner : NULL;
    this_obj = NULL;
    if (current_class != NULL)
    {
        this_obj = newObj ("this", Link (Pointer, current_class));
        SeqInsertFirst (params, (long) this_obj);
    }

    /* parametry: System V (stejne rozlozeni jako u volajiciho, viz classifyArgs):
       registry se ulozi do ramce, zasobnik volajiciho [rbp + 16 + 8 * k] (struktura in situ) */
    Seq * ptys = SeqNew ();                    /* TypePtr */
    Seq * pfp = SeqNew ();                     /* bool */
    for (i = 0; i < SeqSize (params); i++)
    {
        ObjectPtr par = (ObjectPtr) SeqGet (params, i);
        SeqAdd (ptys, (long) par->type);
        SeqAdd (pfp, (long) (par != this_obj && par != ret_obj && isFpArg (par->type, NULL)));
    }
    Seq * slots = classifyArgs (ptys, pfp);    /* ArgSlot * */
    for (i = 0; i < SeqSize (params); i++)
    {
        ObjectPtr par = (ObjectPtr) SeqGet (params, i);
        ArgSlot * sl = (ArgSlot *) SeqGet (slots, i);
        if (sl->kind == 2)
        {
            Variable * var = Variable_new ();
            var->ref = par->type->sort == Reference && !isValueRef (par->type);
            var->offset = 16 + 8 * sl->stack;
            DictStore (&vars, (long) par, (long) var);
        }
        else
            allocLocal (par);
    }

    /* lokalni promenne */
    collectLocals (decl->body);
    fn_temps = statOwned (decl->body);     /* docasne retezce / kontejnery */
    if (fn_temps)
    {
        frame_size += VAR_SIZE;
        temp_mark = - frame_size;
    }
    long frame = (frame_size + 15) & ~15;   /* rsp zustane zarovnany na 16 */

    char * title = decl->name;
    if (decl->owner != NULL)
        title = Cat3 (className (decl->owner), "::", title);
    if (decl->Virtual) title = Cat ("virtual ", title);

    blank ();
    comment (Cat (ctor ? "constructor " : "function ", title));
    bind (DictGetStr (&fce_labels, (long) decl));
    ins1 (iPush, gp (RBP));
    ins2 (iMov, gp (RBP), gp (RSP));
    AsmNode * frame_pos = AsmCursor (prog);   /* za nej sub rsp (docasna mista - upravi se na konci) */
    AsmNode * frame_node = NULL;
    if (frame > 0)
        frame_node = ins2 (iSub, gp (RSP), imm (frame));

    for (i = 0; i < SeqSize (params); i++)
    {
        ArgSlot * sl = (ArgSlot *) SeqGet (slots, i);
        ObjectPtr par = (ObjectPtr) SeqGet (params, i);
        if (sl->kind == 0 && isStructParam (par))
        {
            for (q = 0; q < sl->nq; q++)
                note (ins2 (iMov, qword_ptr (RBP, ((Variable *) DictGet (&vars, (long) par))->offset + 8 * q), gp (arg_regs [sl->regno + q])), par->name);
        }
        else if (sl->kind == 0)
            note (ins2 (iMov, qword_ptr (RBP, ((Variable *) DictGet (&vars, (long) par))->offset), gp (arg_regs [sl->regno])), par->name);
        else if (sl->kind == 1)
            note (ins2 (iMovq, qword_ptr (RBP, ((Variable *) DictGet (&vars, (long) par))->offset), xreg (sl->regno)), par->name);
    }

    /* parametry - struktury hodnotou: uz jsou v ramci (kopie volajiciho); vlastnene polozky +1 */
    for (i = 0; i < SeqSize (params); i++)
    {
        ObjectPtr par = (ObjectPtr) SeqGet (params, i);
        if (isStructParam (par))
        {
            ins2 (iLea, gp (RDI), any_ptr (RBP, ((Variable *) DictGet (&vars, (long) par))->offset));
            incOwned (par->type);
        }
    }

    /* parametry std::string hodnotou: vlastni reference */
    for (i = 0; i < SeqSize (params); i++)
    {
        ObjectPtr par = (ObjectPtr) SeqGet (params, i);
        if (par != this_obj && (IsStdString (par->type) || isValueRef (par->type)))
        {
            note (ins2 (iMov, gp (RDI), qword_ptr (RBP, ((Variable *) DictGet (&vars, (long) par))->offset)), par->name);
            callRt ("RT_SINC", "");
        }
    }
    if (fn_temps)
    {
        ins2 (iMov, gp (RAX), qword_lab ("rt_tcnt"));
        note (ins2 (iMov, qword_ptr (RBP, temp_mark), gp (RAX)), "znacka docasnych hodnot");
    }

    if (ctor)
        codeCtorPrologue (current_class, decl);

    codeStat (decl->body);

    /* docasna mista (vysledky volani - struktury hodnotou) zvetsila ramec */
    long frame2 = (frame_size + 15) & ~15;
    if (frame2 != frame)
    {
        AsmNode * cur = AsmSetCursor (prog, frame_pos);
        if (frame_node != NULL) AsmRemove (prog, frame_node);
        ins2 (iSub, gp (RSP), imm (frame2));
        AsmSetCursor (prog, cur);
    }

    if (ctor)
        loadThis ();                 /* konstruktor vraci this */
    else
        ins2 (iXor, gp (EAX), gp (EAX));    /* bez return => vysledek 0 */
    bind (epilogue_label);
    if (decl->owner != NULL && decl->owner->dtor == decl)
        codeDtorEpilogue (current_class);   /* destruktor: polozky a baze */
    if (fn_temps)
    {
        ins2 (iMov, gp (RDI), qword_ptr (RBP, temp_mark));
        callRt ("RT_TREL", "");                 /* docasne hodnoty (rax se zachova) */
    }
    /* parametry std::string / kontejner hodnotou; kopie objektu s destruktorem */
    bool saved = false;
    for (i = 0; i < SeqSize (params); i++)
    {
        ObjectPtr par = (ObjectPtr) SeqGet (params, i);
        if (isStructParam (par) && NeedsDtor (par->type, true))
        {
            if (!saved) { push (RAX); saved = true; }
            note (ins2 (iLea, gp (RAX), any_ptr (RBP, ((Variable *) DictGet (&vars, (long) par))->offset)), Cat ("~", par->name));
            destructVar (par->type);
        }
    }
    for (i = 0; i < SeqSize (params); i++)
    {
        ObjectPtr par = (ObjectPtr) SeqGet (params, i);
        if (par == this_obj) continue;
        bool str = IsStdString (par->type) || isValueRef (par->type);
        if (!str && !IsContainer (par->type)) continue;
        if (!saved) { push (RAX); saved = true; }
        note (ins2 (iMov, gp (RDI), qword_ptr (RBP, ((Variable *) DictGet (&vars, (long) par))->offset)), par->name);
        callRt (str ? "RT_SDEC" : IsDictionary (par->type) ? "RT_DFREE" : "RT_QFREE", "");
    }
    if (saved) pop (RAX);
    if (fpKind (decl->type->from) != 0)
        ins2 (iMovq, xreg (0), gp (RAX));          /* vysledek float / double v xmm0 */
    ins2 (iMov, gp (RSP), gp (RBP));
    ins1 (iPop, gp (RBP));
    ins0 (iRet);

    current_fce = NULL;
    current_class = NULL;
    this_obj = NULL;
    ret_obj = NULL;
}

/* rax := this */
static void loadThis (void)
{
    if (this_obj == NULL)
        InstrError ("this is not available here");
    note (ins2 (iMov, gp (RAX), qword_ptr (RBP, ((Variable *) DictGet (&vars, (long) this_obj))->offset)), "this");
}

/* ------------------------------------------------------------------ */

/* rax := (rax != 0) ? 1 : 0 */
static void normalizeBool (void)
{
    ins2 (iTest, gp (RAX), gp (RAX));
    ins1 (iSetne, gp (AL));
    ins2 (iMovzx, gp (RAX), gp (AL));
}

/* rax := adresa promenne (u reference adresa ulozena v promenne) */
static void codeVarAddress (ExprPtr expr)
{
    /* polozka tridy v metode nebo konstruktoru:  this + posun */
    if (current_class != NULL && expr->obj != NULL && ! DictContains (&vars, (long) expr->obj) &&
        !expr->obj->fce && IsMemberOf (expr->obj, current_class))
    {
        long off = memberOffset (expr->obj, current_class);
        loadThis ();
        if (off != 0) note (ins2 (iAdd, gp (RAX), imm (off)), expr->id);
        return;
    }

    Variable * var = variable (expr);
    if (var->global)
        note (ins2 (iLea, gp (RAX), alab (var->label)), expr->id);
    else
        note (ins2 (iLea, gp (RAX), any_ptr (RBP, var->offset)), expr->id);

    if (var->ref)
        ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));
}

/* rax := adresa l-hodnoty */
static void codeAddress (ExprPtr expr)
{
    expr = SkipParens (expr);

    switch (expr->op)
    {
        case IDENT:
            if (expr->obj != NULL && expr->obj->fce)
            {
                note (ins2 (iLea, gp (RAX), alab (fnLabel (expr->obj))), Cat ("&", expr->id));   /* &f */
                break;
            }
            codeVarAddress (expr);
            break;

        case LBRACK:
        {
            ExprPtr arr = SkipParens (expr->e1);
            if (IsContainer (arr->type))            /* seq [i],  dict [k] */
            {
                codeContElemAddress (expr);
                break;
            }
            if (IsStdString (arr->type))
                InstrError ("Changing characters of std::string is not supported (use +=)");
            if (arr->type->sort == Pointer)
            {
                checkPointee (arr->type);
                codeExpr (arr);               /* ukazatel => jeho hodnota */
            }
            else if (arr->type->sort == Array)
                codeAddress (arr);
            else
                InstrError ("Array or pointer expected");
            push (RAX);
            codeExpr (expr->e2);
            ins2 (iMov, gp (RCX), gp (RAX));
            pop (RAX);

            long elem_size = elemSize (expr->type);
            if (elem_size == 1)
                ins2 (iAdd, gp (RAX), gp (RCX));
            else if (elem_size == 2 || elem_size == 4 || elem_size == 8)
                ins2 (iLea, gp (RAX), any_ptr_idx (RAX, RCX, (int) elem_size, 0));
            else
            {
                ins3 (iImul, gp (RCX), gp (RCX), imm (elem_size));
                ins2 (iAdd, gp (RAX), gp (RCX));
            }
            break;
        }

        case DEREF:
            checkPointee (expr->e2->type);
            codeExpr (expr->e2);
            break;

        case DYNAMIC_CAST_:                   /* dynamic_cast <T &> (x) */
            codeAddress (expr->e1);
            codeDynamicCheck (SkipParens (expr->e1)->type, expr->type, true, expr);
            break;

        case PERIOD:                          /* s.x */
        {
            if (expr->obj != NULL && expr->obj->fce)
                InstrError (Cat ("Method used as a variable: ", expr->id));
            long off = memberOffset (expr->obj, SkipParens (expr->e1)->type);
            codeAddress (expr->e1);
            if (off != 0) note (ins2 (iAdd, gp (RAX), imm (off)), Cat (".", expr->id));
            break;
        }

        case ARROW:                           /* p->x */
        {
            if (expr->obj != NULL && expr->obj->fce)
                InstrError (Cat ("Method used as a variable: ", expr->id));
            long off = memberOffset (expr->obj, SkipParens (expr->e1)->type->from);
            codeExpr (expr->e1);
            if (off != 0) note (ins2 (iAdd, gp (RAX), imm (off)), Cat ("->", expr->id));
            break;
        }

        case FCE:                             /* f ().x - struktura hodnotou */
            if (! isStructVal (expr->type))
                InstrError ("lvalue expected");
            codeCall (expr);                  /* adresa docasneho mista */
            break;

        default:
            InstrError ("lvalue expected");
    }
}

/* ukazatel na 8-bajtove promenne nebo na znaky (bajty) */
static void checkPointee (TypePtr ptr)
{
    if (ptr == NULL || ptr->from == NULL || ptr->from->sort == Void)
        InstrError ("Dereferencing of void pointer");
}

/* rax := hodnota promenne nebo prvku pole */
static void codeLoad (ExprPtr expr)
{
    if (IsClsSort (expr->type->sort))
        InstrError ("Structures as values are not supported yet (use pointer or reference)");
    if (expr->type->sort == Function)
        InstrError ("Function or method used as a value");

    if (expr->type->sort == Array)
        codeAddress (expr);           /* pole => adresa prvniho prvku */
    else
    {
        codeAddress (expr);
        loadValue (expr);              /* podle velikosti typu (jako v C) */
    }
}

/* rax op= rcx */
/* operace bez znamenka: nektery operand je unsigned (jako obvykly prevod v C),
   ukazatele se porovnavaji bez znamenka */
static bool isUnsignedOp (TypePtr t1, TypePtr t2)
{
    return (t1 != NULL && (t1->sign == Unsigned || t1->sort == Pointer)) ||
           (t2 != NULL && (t2->sign == Unsigned || t2->sort == Pointer));
}

static void codeOperation (SYMBOL kind, bool uns)
{
    switch (kind)
    {
        case PLUS:     ins2 (iAdd, gp (RAX), gp (RCX)); break;
        case MINUS:    ins2 (iSub, gp (RAX), gp (RCX)); break;
        case ASTERISK: ins2 (iImul, gp (RAX), gp (RCX)); break;

        case SLASH:
            if (uns)
            {
                ins2 (iXor, gp (EDX), gp (EDX));
                ins1 (iDiv, gp (RCX));
            }
            else
            {
                ins0 (iCqo);          /* rdx:rax = znamenkove rozsireny rax */
                ins1 (iIdiv, gp (RCX));
            }
            break;

        case MOD:
            if (uns)
            {
                ins2 (iXor, gp (EDX), gp (EDX));
                ins1 (iDiv, gp (RCX));
            }
            else
            {
                ins0 (iCqo);
                ins1 (iIdiv, gp (RCX));
            }
            ins2 (iMov, gp (RAX), gp (RDX));
            break;

        case SHL: ins2 (iShl, gp (RAX), gp (CL)); break;
        case SHR: if (uns) ins2 (iShr, gp (RAX), gp (CL)); else ins2 (iSar, gp (RAX), gp (CL)); break;

        case AND: ins2 (iAnd, gp (RAX), gp (RCX)); break;
        case OR:  ins2 (iOr, gp (RAX), gp (RCX)); break;
        case XOR: ins2 (iXor, gp (RAX), gp (RCX)); break;

        default:
            ins2 (iCmp, gp (RAX), gp (RCX));
            switch (kind)
            {
                case LESS:         if (uns) ins1 (iSetb, gp (AL)); else ins1 (iSetl, gp (AL)); break;
                case LESSEQUAL:    if (uns) ins1 (iSetbe, gp (AL)); else ins1 (iSetle, gp (AL)); break;
                case GREATER:      if (uns) ins1 (iSeta, gp (AL)); else ins1 (iSetg, gp (AL)); break;
                case GREATEREQUAL: if (uns) ins1 (iSetae, gp (AL)); else ins1 (iSetge, gp (AL)); break;
                case EQUAL:        ins1 (iSete, gp (AL)); break;
                case UNEQUAL:      ins1 (iSetne, gp (AL)); break;
                default:           Bug ();
            }
            ins2 (iMovzx, gp (RAX), gp (AL));
            break;
    }
}

static void codeBinary (ExprPtr expr)
{
    /* std::string:  a + b  (novy retezec),  porovnani */
    if (IsStdString (expr->e1->type) || IsStdString (expr->e2->type))
    {
        codeStringValue (expr->e1);
        push (RAX);
        codeStringValue (expr->e2);
        ins2 (iMov, gp (RSI), gp (RAX));
        pop (RDI);
        if (expr->op == PLUS)
        {
            callRt ("RT_SCAT", "");
            strTemp ();
            return;
        }
        callRt ("RT_SCMP", "");          /* -1, 0, 1 */
        ins2 (iXor, gp (ECX), gp (ECX));
        codeOperation (expr->op, false);         /* cmp rax, rcx / setX al */
        return;
    }

    if ((expr->op == PLUS || expr->op == MINUS) &&
        (isPtrValue (expr->e1->type) || isPtrValue (expr->e2->type)))
    {
        codePointerArith (expr);
        return;
    }

    if (fpKind (expr->e1->type) != 0 || fpKind (expr->e2->type) != 0)
    {
        codeFpBinary (expr);
        return;
    }

    codeExpr (expr->e1);
    push (RAX);
    codeExpr (expr->e2);
    ins2 (iMov, gp (RCX), gp (RAX));     /* rcx = pravy operand */
    pop (RAX);                  /* rax = levy operand */
    /* obvykly aritmeticky prevod (jako v C); posun: rozhoduje jen typ leveho operandu */
    bool shift = expr->op == SHL || expr->op == SHR;
    long size;
    bool uns;
    if (shift) arithType (expr->e1->type, NULL, &size, &uns);
    else arithType (expr->e1->type, expr->e2->type, &size, &uns);
    if (size == 4 && uns && !shift)
    {
        ins2 (iMov, gp (EAX), gp (EAX));      /* int => unsigned int */
        ins2 (iMov, gp (ECX), gp (ECX));
    }
    codeOperation (expr->op, uns);
    switch (expr->op)
    {
        case PLUS: case MINUS: case ASTERISK: case SLASH: case MOD:
        case SHL: case SHR: case AND: case OR: case XOR:
            normalizeSize (size, uns);         /* preteceni jako v C (int 32 bitu) */
            break;
        default:
            break;                             /* porovnani: 0 / 1 */
    }
}

static bool isPtrValue (TypePtr t)
{
    return t != NULL && (t->sort == Pointer || t->sort == Array);
}
/* ---- plovouci carka (SSE2) -------------------------------------------
   Hodnota float / double je v rax jako bitovy obraz (double 64 bitu,
   float dolnich 32 bitu, horni nulove), v pameti v 8bajtovem slotu.
   Vypocet v xmm0 / xmm1: double - instrukce ...SD, float - ...SS. */


/* bitovy obraz konstanty pro typ (float / double) */
static unsigned long fpBits (double v, int kind)
{
    if (kind == 1)
    {
        float f = (float) v;
        uint64_t u = 0;         /* ne uint32_t: cmini ma int v 8bajtovem slotu a memcpy */
        memcpy (&u, &f, 4);     /* 4 bajtu by nechalo horni polovinu neurcenou (x86-64: little endian) */
        return u;
    }
    uint64_t u;
    memcpy (&u, &v, 8);
    return u;
}

/* konstantni vyraz s plovouci carkou (inicializace v datech) */
static bool fpConstValue (ExprPtr expr, double * value)
{
    if (expr == NULL) return false;
    long iv;
    switch (expr->op)
    {
        case FLT:
            * value = strtod (expr->id, NULL);
            return true;
        case LPAR:
            return fpConstValue (expr->e1, value);
        case UNARYPLUS:
            return fpConstValue (expr->e2, value);
        case UNARYMINUS:
            if (!fpConstValue (expr->e2, value)) return false;
            * value = - * value;
            return true;
        case STATIC_CAST_:
            return fpConstValue (expr->e1, value);
        case PLUS: case MINUS: case ASTERISK: case SLASH:
        {
            double a, b;
            if (!fpConstValue (expr->e1, &a) || !fpConstValue (expr->e2, &b)) return false;
            switch (expr->op)
            {
                case PLUS:     * value = a + b; break;
                case MINUS:    * value = a - b; break;
                case ASTERISK: * value = a * b; break;
                default:       * value = a / b; break;
            }
            return true;
        }
        default:
            if (!constValue (expr, &iv)) return false;
            * value = (double) iv;
            return true;
    }
}

/* xmm<n> := hodnota v rax */
static void toXmm (int n, int kind)
{
    if (kind == 1) ins2 (iMovd, xreg (n), gp (EAX));
    else ins2 (iMovq, xreg (n), gp (RAX));
}

/* rax := hodnota v xmm0 */
static void fromXmm (int kind)
{
    if (kind == 1) ins2 (iMovd, gp (EAX), xreg (0));
    else ins2 (iMovq, gp (RAX), xreg (0));
}

/* rax: float / double => bool (0.0 a -0.0 => 0, NaN => 1) */
static void fpToBool (int kind)
{
    toXmm (0, kind);
    ins2 (iXorpd, xreg (1), xreg (1));
    if (kind == 1) ins2 (iUcomiss, xreg (0), xreg (1)); else ins2 (iUcomisd, xreg (0), xreg (1));
    ins1 (iSetne, gp (AL));
    ins1 (iSetp, gp (CL));
    ins2 (iOr, gp (AL), gp (CL));
    ins2 (iMovzx, gp (RAX), gp (AL));
}

/* rax: hodnota typu from => hodnota typu to (cele cislo <=> float <=> double) */
static void convert (TypePtr from, TypePtr to)
{
    int f = fpKind (from), t = fpKind (to);
    if (from != NULL && to != NULL && f == 0 && t == 0 && isIntegralType (to) && Resolve (to)->sort != Bool)
    {
        normalizeTo (to);                       /* cele cislo => uzsi / jine znamenko (jako v C) */
        return;
    }
    if (from == NULL || to == NULL || (f == 0 && t == 0) || f == t)
        return;
    if (to->sort == Bool)
    {
        fpToBool (f);
        return;
    }
    if (t != 0)
    {
        if (f != 0)
        {
            toXmm (0, f);
            if (t == 2) ins2 (iCvtss2sd, xreg (0), xreg (0)); else ins2 (iCvtsd2ss, xreg (0), xreg (0));
        }
        else
            if (t == 2) ins2 (iCvtsi2sd, xreg (0), gp (RAX)); else ins2 (iCvtsi2ss, xreg (0), gp (RAX));
        fromXmm (t);
        return;
    }
    /* plovouci carka => cele cislo (orezani k nule) */
    toXmm (0, f);
    if (f == 2) ins2 (iCvttsd2si, gp (RAX), xreg (0)); else ins2 (iCvttss2si, gp (RAX), xreg (0));
    normalizeTo (to);
}

/* rax := hodnota vyrazu jako typ 'to' */
static void codeExprAs (ExprPtr e, TypePtr to)
{
    codeExpr (e);
    convert (e->type, to);
}

/* rax := podminka 0 / 1 (nebo cele cislo) - i z plovouci carky */
static void codeBoolValue (ExprPtr e)
{
    codeExpr (e);
    int k = fpKind (e->type);
    if (k != 0) fpToBool (k);
}

static TypePtr fpCommon (TypePtr t1, TypePtr t2)
{
    return (fpKind (t1) == 2 || fpKind (t2) == 2) ? &DoubleDef : &FloatDef;
}

/* a op b s plovouci carkou: + - * / a porovnani */
static void codeFpBinary (ExprPtr expr)
{
    TypePtr common = fpCommon (expr->e1->type, expr->e2->type);
    int k = fpKind (common);

    codeExprAs (expr->e1, common);
    push (RAX);
    codeExprAs (expr->e2, common);
    toXmm (1, k);
    pop (RAX);
    toXmm (0, k);

    switch (expr->op)
    {
        case PLUS:     ins2 (k == 2 ? iAddsd : iAddss, xreg (0), xreg (1)); fromXmm (k); break;
        case MINUS:    ins2 (k == 2 ? iSubsd : iSubss, xreg (0), xreg (1)); fromXmm (k); break;
        case ASTERISK: ins2 (k == 2 ? iMulsd : iMulss, xreg (0), xreg (1)); fromXmm (k); break;
        case SLASH:    ins2 (k == 2 ? iDivsd : iDivss, xreg (0), xreg (1)); fromXmm (k); break;

        /* porovnani (NaN => nepravda, krome !=):  a < b  <=>  b > a */
        case LESS:
        case LESSEQUAL:
        case GREATER:
        case GREATEREQUAL:
        {
            bool swap = expr->op == LESS || expr->op == LESSEQUAL;
            ins2 (k == 2 ? iUcomisd : iUcomiss, xreg (swap ? 1 : 0), xreg (swap ? 0 : 1));
            bool eq = expr->op == LESSEQUAL || expr->op == GREATEREQUAL;
            if (eq) ins1 (iSetae, gp (AL)); else ins1 (iSeta, gp (AL));
            ins2 (iMovzx, gp (RAX), gp (AL));
            break;
        }
        case EQUAL:
        case UNEQUAL:
            ins2 (k == 2 ? iUcomisd : iUcomiss, xreg (0), xreg (1));
            if (expr->op == EQUAL)
            {
                ins1 (iSete, gp (AL));
                ins1 (iSetnp, gp (CL));
                ins2 (iAnd, gp (AL), gp (CL));
            }
            else
            {
                ins1 (iSetne, gp (AL));
                ins1 (iSetp, gp (CL));
                ins2 (iOr, gp (AL), gp (CL));
            }
            ins2 (iMovzx, gp (RAX), gp (AL));
            break;
        default:
            InstrError ("Invalid operation with floating point operands");
    }
    if (expr->type != NULL && fpKind (expr->type) != 0 && fpKind (expr->type) != k)
        convert (common, expr->type);
}

/* a op= b, ++, -- s plovouci carkou */
static void codeFpAssign (ExprPtr expr, SYMBOL op)
{
    TypePtr lt = expr->e1->type;
    TypePtr common = (fpKind (lt) != 0 || expr->e2 == NULL) ? lt : fpCommon (lt, expr->e2->type);
    if (expr->e2 != NULL && fpKind (expr->e2->type) == 2) common = &DoubleDef;
    int k = fpKind (common);

    codeAddress (expr->e1);
    push (RAX);
    if (expr->e2 != NULL)
        codeExprAs (expr->e2, common);
    else
        ins2 (iMov, gp (RAX), imm_hex (fpBits (1.0, k)));   /* ++ a -- */
    toXmm (1, k);
    ins2 (iMov, gp (RAX), qword_ptr (RSP, 0));
    loadValue (expr->e1);                    /* puvodni hodnota */
    convert (lt, common);
    push (RAX);                              /* puvodni hodnota (postfix) */
    toXmm (0, k);
    switch (op)
    {
        case PLUS:     ins2 (k == 2 ? iAddsd : iAddss, xreg (0), xreg (1)); break;
        case MINUS:    ins2 (k == 2 ? iSubsd : iSubss, xreg (0), xreg (1)); break;
        case ASTERISK: ins2 (k == 2 ? iMulsd : iMulss, xreg (0), xreg (1)); break;
        case SLASH:    ins2 (k == 2 ? iDivsd : iDivss, xreg (0), xreg (1)); break;
        default:       InstrError ("Invalid operation with floating point operands");
    }
    fromXmm (k);
    convert (common, lt);
    pop (RCX);                               /* puvodni hodnota */
    pop (R11);
    storeSized (R11, RAX, lvalueSize (expr->e1));
}

/* pretypovani: cele cislo se zuzi na velikost ciloveho typu (char, short,
   int, unsigned), bool => 0 / 1, ukazatele a long beze zmeny */
static void codeCast (ExprPtr expr)
{
    TypePtr t = expr->type;
    if (IsStdString (t))
    {
        ExprPtr x = SkipParens (expr->e1);
        if (x->op == COMMA)
        {
            /* string (p, n),  string (n, c) */
            bool chars = isPtrValue (x->e1->type);
            codeExpr (x->e1);
            push (RAX);
            codeExpr (x->e2);
            ins2 (iMov, gp (RSI), gp (RAX));
            pop (RDI);
            callRt (chars ? "RT_SFROMN" : "RT_SREP", "");
            strTemp ();
            return;
        }
        codeStringValue (expr->e1);
        return;
    }
    if (IsClsSort (expr->e1->type->sort))
    {
        if (t->sort == Void)
        {
            codeStructAddr (expr->e1);        /* (void) s - jen vyhodnoceni */
            return;
        }
        InstrError ("Unsupported type conversion");
    }
    if (fpKind (t) != 0 || fpKind (expr->e1->type) != 0)
    {
        codeExpr (expr->e1);
        convert (expr->e1->type, t);
        return;
    }

    codeExpr (expr->e1);
    normalizeTo (t);                            /* cele cislo, bool; ukazatele, void beze zmeny */
}

/* dynamic_cast: rax = adresa objektu tridy src => rax = adresa (je-li
   skutecny typ target nebo odvozeny), jinak NULL / chyba (reference);
   retez tabulek virtualnich metod pres [VT - 8] */
static void codeDynamicCheck (TypePtr src, TypePtr target, bool ref, ExprPtr expr)
{
    src = Resolve (src);
    target = Resolve (target);
    if (expr->id != NULL && expr->id [0] == '^')
        return;                                   /* k bazi - bez kontroly */

    char * end_label = newLabel ("L");
    if (!IsPolymorphic (target))
    {
        /* nepolymorfni cil nemuze byt skutecnym typem polymorfniho objektu */
        if (ref)
        {
            ins2 (iLea, gp (RDI), alab ("rt_bcmsg"));
            callRt ("RT_ASSERT", "");
        }
        ins2 (iXor, gp (EAX), gp (EAX));
        return;
    }

    char * fail_label = newLabel ("L");
    structCount (src);
    ins2 (iTest, gp (RAX), gp (RAX));
    ins1 (iJz, jlab ((ref ? fail_label : end_label)));
    push (RAX);
    note (ins2 (iMov, gp (RDI), qword_ptr (RAX, (int) DictGet (&vptr_offsets, (long) src))), "vtable");
    note (ins2 (iLea, gp (RSI), alab (vtableLabel (target))), className (target));
    callRt ("RT_DCAST", "");
    ins2 (iTest, gp (RAX), gp (RAX));
    pop (RAX);
    ins1 (iJnz, jlab (end_label));
    bind (fail_label);
    if (ref)
    {
        ins2 (iLea, gp (RDI), alab ("rt_bcmsg"));
        callRt ("RT_ASSERT", "");                     /* bad_cast - vyjimky nejsou */
    }
    else
        ins2 (iXor, gp (EAX), gp (EAX));
    bind (end_label);
}

/* p + n,  n + p,  p - n  (n * velikost prvku),  p - q  (pocet prvku) */
static void codePointerArith (ExprPtr expr)
{
    bool p1 = isPtrValue (expr->e1->type);
    bool p2 = isPtrValue (expr->e2->type);
    long size = elemSize (p1 ? expr->e1->type->from : expr->e2->type->from);

    codeExpr (expr->e1);
    push (RAX);
    codeExpr (expr->e2);
    ins2 (iMov, gp (RCX), gp (RAX));
    pop (RAX);

    if (p1 && p2)
    {
        if (expr->op != MINUS)
            InstrError ("Invalid pointer arithmetic");
        ins2 (iSub, gp (RAX), gp (RCX));
        if (size != 1)
        {
            ins2 (iMov, gp (RCX), imm (size));
            ins0 (iCqo);
            ins1 (iIdiv, gp (RCX));
        }
        return;
    }

    if (p2)                                  /* n + p */
    {
        if (expr->op != PLUS)
            InstrError ("Invalid pointer arithmetic");
        ins2 (iXchg, gp (RAX), gp (RCX));
    }
    if (size != 1)
        ins3 (iImul, gp (RCX), gp (RCX), imm (size));
    if (expr->op == PLUS)
        ins2 (iAdd, gp (RAX), gp (RCX));
    else
        ins2 (iSub, gp (RAX), gp (RCX));
}

static void codeAndOr (ExprPtr expr)
{
    char * end_label = newLabel ("L");

    long mark1 = SeqSize (&pending_temps);
    codeBoolValue (expr->e1);
    releaseTemps (mark1);
    ins2 (iTest, gp (RAX), gp (RAX));

    if (expr->op == LOGAND)
        ins1 (iJz, jlab (end_label));    /* levy = 0 => vysledek 0 */
    else
        ins1 (iJnz, jlab (end_label));    /* levy != 0 => vysledek 1 */

    long mark2 = SeqSize (&pending_temps);
    codeBoolValue (expr->e2);
    releaseTemps (mark2);                /* pravy operand se vyhodnoti jen podmineně */
    bind (end_label);
    normalizeBool ();
}

static void codeConditional (ExprPtr expr)
{
    // e1 ? e2 : e3
    char * else_label = newLabel ("L");
    char * end_label = newLabel ("L");

    bool str = IsStdString (expr->type);     /* c ? s : "abc" - obe vetve jako std::string */
    codeCond (expr->e1);
    ins1 (iJz, jlab (else_label));
    long mark2 = SeqSize (&pending_temps);
    if (str) codeStringValue (expr->e2); else codeExprAs (expr->e2, expr->type);
    releaseTemps (mark2);
    ins1 (iJmp, jlab (end_label));
    bind (else_label);
    long mark3 = SeqSize (&pending_temps);
    if (str) codeStringValue (expr->e3); else codeExprAs (expr->e3, expr->type);
    releaseTemps (mark3);
    bind (end_label);
}

/* op= => op */
static SYMBOL assignOp (SYMBOL op)
{
    switch (op)
    {
        case ADDASSIGN: return PLUS;
        case SUBASSIGN: return MINUS;
        case MULASSIGN: return ASTERISK;
        case DIVASSIGN: return SLASH;
        case MODASSIGN: return MOD;
        case SHLASSIGN: return SHL;
        case SHRASSIGN: return SHR;
        case ANDASSIGN: return AND;
        case ORASSIGN:  return OR;
        case XORASSIGN: return XOR;
        default:        InstrError ("Unknown assignment operator"); return op;
    }
}

/* =, +=, -=, ... */
static void codeAssign (ExprPtr expr)
{

    /* std::string:  s = x,  s += x  (retezce se nemeni - staci ukazatel) */
    if (IsStdString (expr->e1->type))
    {
        codeAddress (expr->e1);
        push (RAX);
        codeStringValue (expr->e2);
        if (expr->op == ADDASSIGN)
        {
            ins2 (iMov, gp (RSI), gp (RAX));
            ins2 (iMov, gp (RAX), qword_ptr (RSP, 0));    /* adresa */
            ins2 (iMov, gp (RDI), qword_ptr (RAX, 0));    /* puvodni hodnota */
            callRt ("RT_SCAT", "");
            strTemp ();
        }
        ins2 (iMov, gp (RSI), gp (RAX));
        pop (RDI);
        callRt ("RT_SASSIGN", "");                         /* reference: nova +1, stara -1 */
        return;
    }

    /* s [i] = c  =>  s = RT_SSET (s, i, c)  (kopie retezce) */
    ExprPtr lhs = SkipParens (expr->e1);
    if (lhs->op == LBRACK && IsStdString (SkipParens (lhs->e1)->type))
    {
        if (expr->op != ASSIGN)
            InstrError ("Only s [i] = c is supported for characters of std::string");
        codeAddress (lhs->e1);
        push (RAX);                       /* adresa retezce */
        codeExpr (lhs->e2);
        push (RAX);                       /* index */
        codeExpr (expr->e2);
        ins2 (iMov, gp (RDX), gp (RAX));
        pop (RSI);
        ins2 (iMov, gp (RAX), qword_ptr (RSP, 0));
        ins2 (iMov, gp (RDI), qword_ptr (RAX, 0));
        push (RDX);
        callRt ("RT_SSET", "");
        strTemp ();
        pop (RDX);
        ins2 (iMov, gp (RSI), gp (RAX));
        pop (RDI);
        push (RDX);
        callRt ("RT_SASSIGN", "");
        pop (RDX);
        ins2 (iMovsx, gp (RAX), gp (DL));            /* vysledek = prirazeny znak */
        return;
    }

    if (IsContainer (expr->e1->type))
    {
        /* a = b (kontejnery): kopie */
        if (expr->op != ASSIGN)
            InstrError ("Invalid operation with Sequence / Dictionary");
        codeAddress (expr->e1);
        push (RAX);
        codeContValue (expr->e2);
        pop (R11);
        ins2 (iMov, gp (RDI), qword_ptr (R11, 0));       /* puvodni hodnota se uvolni */
        ins2 (iMov, qword_ptr (R11, 0), gp (RAX));
        push (RAX);
        callRt (IsDictionary (expr->e1->type) ? "RT_DFREE" : "RT_QFREE", "");
        pop (RAX);
        return;
    }

    bool fp = fpKind (expr->e1->type) != 0 || fpKind (expr->e2->type) != 0;
    if (fp && expr->op != ASSIGN)
    {
        codeFpAssign (expr, assignOp (expr->op));
        return;
    }
    if (fp)
    {
        /* x = vyraz (prevod mezi celym cislem, float a double) */
        codeAddress (expr->e1);
        push (RAX);
        codeExprAs (expr->e2, expr->e1->type);
        pop (R11);
        storeSized (R11, RAX, lvalueSize (expr->e1));
        return;
    }

    if (IsClsSort (expr->e1->type->sort))
    {
        /* a = b (struktury): kopie vsech 8-bajtovych slotu, std::string
           reference +1, kontejnery vlastni kopie; puvodni hodnoty a se
           uvolni; vysledek = adresa a */
        if (expr->op != ASSIGN)
            InstrError ("Invalid operation with structure");
        long count = structBytes (expr->e1->type);
        codeStructAddr (expr->e2);            /* i vysledek funkce */
        push (RAX);
        codeAddress (expr->e1);
        ins2 (iMov, gp (RDI), gp (RAX));
        pop (RSI);
        ins2 (iMov, gp (RDX), gp (RDI));

        Seq * owned = SeqNew ();
        long oi;
        collectContMembers (expr->e1->type, 0, owned);
        char * done_label = newLabel ("L");
        if (! SeqEmpty (owned))
        {
            ins2 (iCmp, gp (RDI), gp (RSI));
            ins1 (iJe, jlab (done_label));     /* a = a */
            push (RSI);
            push (RDI);
            for (oi = 0; oi < SeqSize (owned); oi++)
            {
                OwnedMember * c = (OwnedMember *) SeqGet (owned, oi);
                ins2 (iMov, gp (RAX), qword_ptr (RSP, 0));
                ins2 (iMov, gp (RDI), qword_ptr (RAX, (int) c->offset));
                callRt (IsStdString (c->type) ? "RT_SDEC" : IsDictionary (c->type) ? "RT_DFREE" : "RT_QFREE", "");
            }
            pop (RDI);
            pop (RSI);
            ins2 (iMov, gp (RDX), gp (RDI));
        }
        ins2 (iMov, gp (RCX), imm (count));
        insPrefix (iRep, iMovsb);

        if (! SeqEmpty (owned))
        {
            push (RDX);
            for (oi = 0; oi < SeqSize (owned); oi++)
            {
                OwnedMember * c = (OwnedMember *) SeqGet (owned, oi);
                ins2 (iMov, gp (RAX), qword_ptr (RSP, 0));
                ins2 (iMov, gp (RDI), qword_ptr (RAX, (int) c->offset));
                if (IsStdString (c->type))
                {
                    callRt ("RT_SINC", "");
                    continue;
                }
                callRt (IsDictionary (c->type) ? "RT_DCOPY" : "RT_QCOPY", "kopie");
                ins2 (iMov, gp (RCX), qword_ptr (RSP, 0));
                ins2 (iMov, qword_ptr (RCX, (int) c->offset), gp (RAX));
            }
            pop (RDX);
            bind (done_label);
        }
        if (IsPolymorphic (expr->e1->type))
        {
            /* rez (base = derived): cil si ponecha tabulku virtualnich metod sveho typu */
            note (ins2 (iLea, gp (RCX), alab (vtableLabel (expr->e1->type))), Cat ("vtable ", className (expr->e1->type)));
            ins2 (iMov, qword_ptr (RDX, DictGet (&vptr_offsets, (long) expr->e1->type)), gp (RCX));
        }
        ins2 (iMov, gp (RAX), gp (RDX));
        return;
    }
    bool ptr_arith = expr->op != ASSIGN && expr->e1->type->sort == Pointer;
    if (ptr_arith && expr->op != ADDASSIGN && expr->op != SUBASSIGN)
        InstrError ("Invalid pointer arithmetic");
    long size = lvalueSize (expr->e1);

    codeAddress (expr->e1);
    push (RAX);
    if (expr->op == ASSIGN)
        codeExprAs (expr->e2, expr->e1->type);   /* prevod na typ cile (orezani jako v C) */
    else
        codeExpr (expr->e2);
    pop (R11);                  /* r11 = adresa */

    if (expr->op != ASSIGN)
    {
        ins2 (iMov, gp (RCX), gp (RAX));
        if (ptr_arith)            /* p += n  =>  n * velikost prvku */
        {
            long esize = elemSize (expr->e1->type->from);
            if (esize != 1)
                ins3 (iImul, gp (RCX), gp (RCX), imm (esize));
        }
        ins2 (iMov, gp (RAX), gp (R11));
        loadValue (expr->e1);
        SYMBOL op = assignOp (expr->op);
        long csize;
        bool uns;
        if (op == SHL || op == SHR) arithType (expr->e1->type, NULL, &csize, &uns);
        else arithType (expr->e1->type, expr->e2->type, &csize, &uns);
        if (ptr_arith) { csize = 8; uns = false; }
        if (csize == 4 && uns && op != SHL && op != SHR)
        {
            ins2 (iMov, gp (EAX), gp (EAX));      /* unsigned int - spolecny typ */
            ins2 (iMov, gp (ECX), gp (ECX));
        }
        codeOperation (op, uns);
    }

    normalizeTo (expr->e1->type);   /* vysledek prirazeni = ulozena hodnota (bool 0 / 1) */
    storeSized (R11, RAX, size);
}

/* ++ a -- */
static void codeIncDec (ExprPtr expr)
{
    bool postfix = expr->e1 != NULL;     /* e1++  versus  ++e2 */
    ExprPtr operand = postfix ? expr->e1 : expr->e2;
    keyword op = (expr->op == DPLUS) ? iAdd : iSub;

    if (fpKind (operand->type) != 0)
    {
        /* x++, ++x (double, float): vysledek ++x = nova hodnota, x++ = puvodni */
        Expr tmp;
        memcpy (&tmp, expr, sizeof (Expr));
        tmp.e1 = operand;
        tmp.e2 = NULL;
        codeFpAssign (&tmp, expr->op == DPLUS ? PLUS : MINUS);
        if (postfix) ins2 (iMov, gp (RAX), gp (RCX));
        return;
    }

    long step = 1;
    if (operand->type->sort == Pointer)     /* p++  =>  + velikost prvku */
        step = elemSize (operand->type->from);
    long size = lvalueSize (operand);

    codeAddress (operand);
    ins2 (iMov, gp (RCX), gp (RAX));
    loadValue (operand);

    if (postfix)
    {
        ins2 (iMov, gp (RDX), gp (RAX));           /* vysledek = puvodni hodnota */
        ins2 (op, gp (RDX), imm (step));
        storeSized (RCX, RDX, size);
    }
    else
    {
        ins2 (op, gp (RAX), imm (step));
        normalizeTo (operand->type);             /* preteceni jako v C */
        storeSized (RCX, RAX, size);
    }
}

static void collectArgs (ExprPtr expr, Seq * args)
{
    if (expr == NULL) return;
    if (expr->op == COMMA)
    {
        collectArgs (expr->e1, args);
        collectArgs (expr->e2, args);
    }
    else
        SeqAdd (args, (long) expr);
}

/* assert (podminka) - vestavena funkce */
static void codeAssert (Seq * args)
{
    if (SeqSize (args) != 1)
        InstrError ("assert expects one argument");

    char * ok_label = newLabel ("L");
    codeCond ((ExprPtr) SeqGet (args, 0));
    ins1 (iJnz, jlab (ok_label));
    ins2 (iLea, gp (RDI), alab (stringLabel (current_fce != NULL ? current_fce->name : "?")));
    callRt ("RT_ASSERT", "");
    bind (ok_label);
    ins2 (iXor, gp (EAX), gp (EAX));
}

/* new T  /  new T (argumenty konstruktoru) */
static void codeNew (ExprPtr expr)
{
    TypePtr T = expr->type->from;
    long size = typeCount (T) * VAR_SIZE;

    if (expr->e2 != NULL)
    {
        /* new T [n] => calloc (n, velikost prvku); prvky jsou vynulovane
           (std::string prazdne, ukazatele NULL), konstruktory se nevolaji */
        if (IsClsSort (T->sort) && NeedsCtor (T, true))
            InstrError ("new T [n] is not supported for structures with constructor");
        codeExpr (expr->e2);
        ins2 (iMov, gp (RDI), gp (RAX));
        ins2 (iMov, gp (RSI), imm (elemSize (T)));
        callRt ("RT_CALLOC", Cat3 ("new ", expr->id, " [n]"));
        return;
    }

    Seq * args = SeqNew ();
    collectArgs (expr->e1, args);
    if (!IsClsSort (T->sort) && !SeqEmpty (args))
        InstrError ("new T (...) is supported only for structures with constructor");

    /* calloc (1, size) - pamet je vynulovana */
    ins2 (iMov, gp (EDI), imm (1));
    ins2 (iMov, gp (RSI), imm (size));
    callRt ("RT_CALLOC", Cat ("new ", expr->id));

    if (IsClsSort (T->sort) && (NeedsCtor (T, true) || !SeqEmpty (args)))
        callCtor (T, args);                    /* vraci this */
}

/* struktury hodnotou (ExprRec, AsmOp - parametr, vysledek funkce): ve vyrazu
   je struktura reprezentovana adresou; parametr - volajici preda adresu,
   volany si strukturu v prologu zkopiruje do sveho ramce; vysledek -
   volajici preda adresu docasneho mista ve svem ramci (skryty parametr za
   this), return do nej kopiruje a vraci ji v rax */
/* kopie po 8-bajtovych slotech nestaci pro std::string a kontejnery
   (pocty referenci, vlastni kopie) a objekty s destruktorem */
static void checkStructVal (TypePtr t)
{
    /* vysledek funkce hodnotou: vlastnene polozky (std::string, kontejnery) se kopiruji a
       uvolnuji; uzivatelsky destruktor a jiny destruktor (vnorena trida) ne */
    Seq * owned = SeqNew ();
    collectContMembers (t, 0, owned);
    if (hasUserDtor (t) || (SeqEmpty (owned) && NeedsDtor (t, true)))
        InstrError (Cat ("Structures with a destructor as values are not supported: ", className (t)));
}

/* vlastni kopirovaci konstruktor T (const T &) - jediny konstruktor tridy */
/* kopie parametru hodnotou a T x = y: objekt s destruktorem staci zkopirovat po
   slotech (vychozi kopie po polozkach), kdyz nema std::string, kontejnery ani
   vlastni kopirovaci konstruktor; destruktor kopie na konci funkce / bloku */
static void checkStructCopy (TypePtr t)
{
    if (hasCopyCtor (t))
        InstrError (Cat ("Structures with a copy constructor as values are not supported: ", className (t)));
}

/* docasne misto v ramci (prolog se upravi na konci funkce) => [rbp + vysledek] */
static long allocTemp (long count)
{
    frame_size += count * VAR_SIZE;
    return - frame_size;
}

/* kopie count 8-bajtovych slotu:  rsi => rdi */
static void copySlots (long count)
{
    ins2 (iMov, gp (RCX), imm (count));
    insPrefix (iRep, iMovsq);
}

/* struktura hodnotou: po bajtove kopii do rdi  => nove vlastnene polozky (std::string:
   pocet referenci, Sequence / Dictionary: hluboka kopie) - jako u prirazeni */
static void incOwned (TypePtr type)
{
    Seq * owned = SeqNew ();
    long oi;
    collectContMembers (type, 0, owned);
    if (SeqEmpty (owned)) return;
    push (RDI);
    for (oi = 0; oi < SeqSize (owned); oi++)
    {
        OwnedMember * c = (OwnedMember *) SeqGet (owned, oi);
        ins2 (iMov, gp (RAX), qword_ptr (RSP, 0));
        ins2 (iMov, gp (RDI), qword_ptr (RAX, (int) c->offset));
        if (IsStdString (c->type))
        {
            callRt ("RT_SINC", "");
            continue;
        }
        callRt (IsDictionary (c->type) ? "RT_DCOPY" : "RT_QCOPY", "kopie");
        ins2 (iMov, gp (RCX), qword_ptr (RSP, 0));
        ins2 (iMov, qword_ptr (RCX, (int) c->offset), gp (RAX));
    }
    pop (RDI);
}

/* struktura hodnotou: uvolneni vlastnenych polozek ve strukture na adrese rdi
   (docasny objekt, ktery uz byl zkopirovany) */
static void decOwned (TypePtr type)
{
    Seq * owned = SeqNew ();
    long oi;
    collectContMembers (type, 0, owned);
    if (SeqEmpty (owned)) return;
    push (RDI);
    for (oi = 0; oi < SeqSize (owned); oi++)
    {
        OwnedMember * c = (OwnedMember *) SeqGet (owned, oi);
        ins2 (iMov, gp (RAX), qword_ptr (RSP, 0));
        ins2 (iMov, gp (RDI), qword_ptr (RAX, (int) c->offset));
        callRt (IsStdString (c->type) ? "RT_SDEC" : IsDictionary (c->type) ? "RT_DFREE" : "RT_QFREE", "");
    }
    pop (RDI);
}

/* uvolni docasne objekty od pozice mark (vcetne) - RAX (vysledek) se zachova */
static void releaseTemps (long mark)
{
    if (SeqSize (&pending_temps) <= mark) return;
    push (RAX);
    while (SeqSize (&pending_temps) > mark)
    {
        PendingTemp * t = (PendingTemp *) SeqLast (&pending_temps);
        SeqPop (&pending_temps);
        ins2 (iLea, gp (RDI), any_ptr (RBP, (int) t->off));
        decOwned (t->type);
    }
    pop (RAX);
}

/* rax := adresa struktury: promenna, *p, p->x, ... i vysledek volani funkce
   (adresa docasneho mista volajiciho) */
static void codeStructAddr (ExprPtr e)
{
    e = SkipParens (e);
    if (e->op == FCE)
        codeCall (e);
    else
        codeAddress (e);
}

/* rax := hodnota argumentu pro parametr typu ptype (NULL => ...) */
static void codeArg (ExprPtr arg, TypePtr ptype)
{
    if (isStructVal (ptype))
    {
        checkStructCopy (ptype);
        codeStructAddr (arg);              /* struktura hodnotou => adresa (kopii udela volany) */
    }
    else if (ptype != NULL && ptype->sort == Reference && !isValueRef (ptype))
        codeAddress (arg);                 /* reference => adresa */
    else if (IsStdString (ptype) || isValueRef (ptype))
        codeStringValue (arg);             /* std::string, const std::string & */
    else if (IsContainer (ptype))
        codeContValue (arg);               /* Sequence / Dictionary hodnotou - kopie */
    else if (ptype == NULL)
    {
        codeExpr (arg);                    /* ... : float => double */
        if (fpKind (arg->type) == 1) convert (arg->type, &DoubleDef);
    }
    else
        codeExprAs (arg, ptype);
}

/* parametr / argument v registru XMM (float, double hodnotou) */
static bool isFpArg (TypePtr ptype, ExprPtr arg)
{
    if (ptype != NULL) return fpKind (ptype) != 0;
    return arg != NULL && fpKind (arg->type) != 0;     /* ... */
}


/* umisteni parametru podle System V: cele cislo / ukazatel - rdi, rsi, rdx,
   rcx, r8, r9; float, double - xmm0 .. xmm7; dalsi na zasobniku (v poradi);
   reg >= 0 - obecny registr, xmm >= 0 - registr XMM, jinak stack (poradi) */
/* vyhodnoti this (code_this != NULL) a argumenty, ulozi je do registru
   a na zasobnik; nxmm = pocet pouzitych registru XMM; vraci pocet
   8bajtovych mist na zasobniku k uvolneni po volani */
/* rax := this podle code_this */
static void codeThis (ThisCode * t)
{
    switch (t->kind)
    {
        case THIS_STACK:
            note (ins2 (iMov, gp (RAX), qword_ptr (RSP, 8 * (depth - t->this_depth))), "this");
            break;
        case THIS_LOAD:
            loadThis ();
            break;
        case THIS_ADDR:
            codeAddress (t->object);           /* obj.f () => &obj */
            break;
        case THIS_EXPR:
            codeExpr (t->object);              /* p->f () => p */
            break;
    }
}

/* klasifikace argumentu: types[i] = typ parametru (NULL = skryty parametr nebo ...),
   fp[i] = float / double v XMM. Struktura hodnotou: do 16 bajtu v registrech INTEGER (qword po
   qwordu), vetsi na zasobniku (cela, od nejnizsiho qwordu). Ostatni jako System V. */
static Seq * classifyArgs (Seq * types, Seq * fp)
{
    Seq * out = SeqNew ();       /* ArgSlot * */
    long gi = 0, xi = 0, si = 0;
    long i;
    for (i = 0; i < SeqSize (types); i++)
    {
        ArgSlot * s = (ArgSlot *) MemAlloc (sizeof (ArgSlot));
        s->kind = 2; s->nq = 1; s->regno = -1; s->stack = -1;
        TypePtr t = (TypePtr) SeqGet (types, i);
        bool f = SeqGet (fp, i) != 0;
        if (t != NULL && isStructVal (t))
        {
            if (hasFloatMember (t))
                InstrError (Cat ("Structures with floating members by value are not supported: ", className (t)));
            long S = (structBytes (t) + 7) / 8;
            if (S <= 2 && gi + S <= 6)
            {
                s->kind = 0; s->nq = S; s->regno = gi; gi += S;
            }
            else
            {
                s->kind = 2; s->nq = S; s->stack = si; si += S;
            }
        }
        else if (f && xi < 8)
        {
            s->kind = 1; s->regno = xi ++;
        }
        else if (! f && gi < 6)
        {
            s->kind = 0; s->regno = gi ++;
        }
        else
        {
            s->kind = 2; s->stack = si ++;
        }
        SeqAdd (out, (long) s);
    }
    return out;
}

/* parametr je struktura hodnotou (ne this, ne skryty vysledek) */
static bool isStructParam (ObjectPtr par)
{
    return par != this_obj && par != ret_obj && isStructVal (par->type);
}

/* registry parametru (System V) */
static const Register pass_arg_regs [] = { RDI, RSI, RDX, RCX, R8, R9 };

static long passArgs (ThisCode * code_this, Seq * args, Seq * ptypes, int * nxmm, long sret_off)
{
    /* skryte parametry: this, adresa pro vysledek (sret_off != 0), pak argumenty */
    long nthis = (code_this != NULL) ? 1 : 0;
    long first = nthis + ((sret_off != 0) ? 1 : 0);
    long total = first + SeqSize (args);
    Seq * types = SeqNew ();     /* TypePtr */
    Seq * fp = SeqNew ();        /* bool */
    long i, j, q;
    for (j = 0; j < first; j++) { SeqAdd (types, 0); SeqAdd (fp, 0); }
    for (i = 0; i < SeqSize (args); i++)
    {
        SeqAdd (types, SeqGet (ptypes, i));
        SeqAdd (fp, isFpArg ((TypePtr) SeqGet (ptypes, i), (ExprPtr) SeqGet (args, i)) ? 1 : 0);
    }
    Seq * slots = classifyArgs (types, fp);

    /* argumenty na zasobniku: od posledniho, prvni bude na [rsp] */
    long nstack = 0;
    for (i = 0; i < SeqSize (slots); i++)
    {
        ArgSlot * s = (ArgSlot *) SeqGet (slots, i);
        if (s->kind == 2) nstack += s->nq;
    }
    long drop = nstack;
    if (nstack > 0 && (depth + nstack) % 2 != 0)  /* rsp pri volani zarovnany na 16 */
    {
        ins2 (iSub, gp (RSP), imm (8));
        depth ++;
        drop ++;
    }
    for (j = total - 1; j >= 0; j--)
    {
        ArgSlot * sj = (ArgSlot *) SeqGet (slots, j);
        if (sj->kind == 2)
        {
            if (j < nthis)
            {
                codeThis (code_this);
                push (RAX);
            }
            else if (j < first)
            {
                ins2 (iLea, gp (RAX), any_ptr (RBP, sret_off));   /* misto pro vysledek */
                push (RAX);
            }
            else if ((TypePtr) SeqGet (ptypes, j - first) != NULL && isStructVal ((TypePtr) SeqGet (ptypes, j - first)))
            {
                codeStructAddr ((ExprPtr) SeqGet (args, j - first));   /* rax = adresa struktury */
                for (q = sj->nq - 1; q >= 0; q--)
                {
                    ins2 (iMov, gp (RCX), qword_ptr (RAX, 8 * q));
                    push (RCX);
                }
            }
            else
            {
                codeArg ((ExprPtr) SeqGet (args, j - first), (TypePtr) SeqGet (ptypes, j - first));
                push (RAX);
            }
        }
    }

    /* argumenty v registrech: postupne na zasobnik, potom do registru */
    for (j = 0; j < total; j++)
    {
        ArgSlot * sj = (ArgSlot *) SeqGet (slots, j);
        if (sj->kind != 2)
        {
            if (j < nthis)
            {
                codeThis (code_this);
                push (RAX);
            }
            else if (j < first)
            {
                ins2 (iLea, gp (RAX), any_ptr (RBP, sret_off));   /* misto pro vysledek */
                push (RAX);
            }
            else if (sj->kind == 0 && (TypePtr) SeqGet (ptypes, j - first) != NULL && isStructVal ((TypePtr) SeqGet (ptypes, j - first)))
            {
                codeStructAddr ((ExprPtr) SeqGet (args, j - first));
                for (q = 0; q < sj->nq; q++)
                {
                    ins2 (iMov, gp (RCX), qword_ptr (RAX, 8 * q));
                    push (RCX);
                }
            }
            else
            {
                codeArg ((ExprPtr) SeqGet (args, j - first), (TypePtr) SeqGet (ptypes, j - first));
                push (RAX);
            }
        }
    }
    * nxmm = 0;
    for (j = total - 1; j >= 0; j--)
    {
        ArgSlot * sj = (ArgSlot *) SeqGet (slots, j);
        if (sj->kind == 0)
        {
            for (q = sj->nq - 1; q >= 0; q--)
                pop (pass_arg_regs [sj->regno + q]);
        }
        else if (sj->kind == 1)
        {
            pop (RAX);
            ins2 (iMovq, xreg (sj->regno), gp (RAX));
            if (sj->regno + 1 > * nxmm) * nxmm = sj->regno + 1;
        }
    }
    return drop;
}

/* volani funkce, metody (obj.f (), p->f (), f () v metode, T::f ()),
   virtualni metody pres tabulku virtualnich metod */
static void codeCall (ExprPtr expr)
{
    ExprPtr f = SkipParens (expr->e1);
    ObjectPtr fce = NULL;
    ExprPtr object = NULL;        /* obj.f (), p->f () */
    TypePtr cls = NULL;           /* staticky typ objektu */
    bool qualified = false;       /* T::f () - bez virtualniho volani */
    long i;

    /* primo funkce nebo metoda, jinak volani pres ukazatel na funkci */
    bool direct = ((f->op == IDENT || f->op == DCOLON || f->op == PERIOD || f->op == ARROW) &&
                   f->obj != NULL && f->obj->fce);
    if (!direct)
    {
        codeIndirectCall (expr, f);
        return;
    }

    switch (f->op)
    {
        case IDENT:
        case DCOLON:
            fce = f->obj;
            if (fce == NULL || !fce->fce)
                InstrError ("Only direct function calls are supported");
            if (fce->method)
            {
                if (current_class == NULL || !IsMemberOf (fce, current_class))
                    InstrError (Cat ("Method called without object: ", fce->name));
                cls = current_class;
            }
            qualified = f->op == DCOLON;
            break;

        case PERIOD:
        case ARROW:
            fce = f->obj;
            if (fce == NULL || !fce->fce || !fce->method)
                InstrError (Cat ("Method expected: ", f->id));
            object = f->e1;
            cls = (f->op == PERIOD) ? SkipParens (f->e1)->type : SkipParens (f->e1)->type->from;
            break;

        default:
            InstrError ("Only direct function calls are supported");
    }

    /* vestavene metody Sequence a Dictionary */
    if (cls != NULL && IsContainer (cls))
    {
        codeContMethod (expr, f, fce, cls);
        return;
    }

    /* vestavene metody std::string: c_str, length, size, empty,
       substr, find, rfind, clear */
    if (cls != NULL && IsStdString (cls))
    {
        char * name = fce->name;
        if (StrEq (name, "reserve"))          /* bez ucinku (retezce se nemeni) */
        {
            Seq * rargs = SeqNew ();
            collectArgs (expr->e2, rargs);
            if (! SeqEmpty (rargs)) codeExpr ((ExprPtr) SeqGet (rargs, 0));
            return;
        }
        if (StrEq (name, "clear"))            /* s = "" */
        {
            if (f->op == ARROW)
                codeExpr (object);
            else
                codeAddress (object);
            ins2 (iMov, qword_ptr (RAX, 0), imm (0));
            return;
        }

        codeExpr (object);
        if (f->op == ARROW)
            ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));

        if (StrEq (name, "substr") || StrEq (name, "find") || StrEq (name, "rfind"))
        {
            push (RAX);
            Seq * args = SeqNew ();
            collectArgs (expr->e2, args);
            ExprPtr a [2];
            NodePtr par = fce->type->list.first;
            for (i = 0; i < 2; i++)           /* chybejici argumenty - implicitni hodnoty */
            {
                a [i] = (i < SeqSize (args)) ? (ExprPtr) SeqGet (args, i) : par->item->init;
                par = par->next;
            }
            if (StrEq (name, "substr"))
                codeExpr (a [0]);
            else
                codeStringValue (a [0]);
            push (RAX);
            codeExpr (a [1]);
            ins2 (iMov, gp (RDX), gp (RAX));
            pop (RSI);
            pop (RDI);
            callRt (StrEq (name, "substr") ? "RT_SSUB" : StrEq (name, "find") ? "RT_SFIND" : "RT_SRFIND", "");
            if (StrEq (name, "substr")) strTemp ();
            return;
        }

        ins2 (iMov, gp (RDI), gp (RAX));
        if (StrEq (name, "c_str"))
            callRt ("RT_SCSTR", "");
        else
        {
            callRt ("RT_SLEN", "");
            if (StrEq (name, "empty"))
            {
                ins2 (iTest, gp (RAX), gp (RAX));
                ins1 (iSete, gp (AL));
                ins2 (iMovzx, gp (RAX), gp (AL));
            }
        }
        return;
    }

    Seq * args = SeqNew ();      /* ExprPtr */
    collectArgs (expr->e2, args);

    if (fce->implicit && StrEq (fce->name, "assert"))
    {
        codeAssert (args);
        return;
    }

    /* chybejici argumenty => implicitni hodnoty parametru:  int level = 0 */
    addDefaultArgs (fce, args);


    /* cil volani */
    bool is_virtual = fce->method && fce->Virtual && !qualified;
    char * target = (char *) "";
    if (!is_virtual)
    {
        if (fce->body != NULL)
            target = DictGetStr (&fce_labels, (long) fce);
        else if (fce->owner != NULL)
            target = methodTarget (fce, "Method");
        else if (use_libc)
            target = externLabel (fce->name);     /* glibc - overi linker */
        else
        {
            target = rtlFunction (fce->name);
            if (StrEmpty (target))
                InstrError (Cat ("Undefined function: ", fce->name));
        }
    }

    /* typy parametru k argumentum (NULL => ...) */
    Seq * ptypes = SeqNew ();    /* TypePtr */
    {
        NodePtr par = fce->type->list.first;
        for (i = 0; i < SeqSize (args); i++)
        {
            SeqAdd (ptypes, par != NULL ? (long) par->item->type : 0);
            if (par != NULL && par->item->type != NULL) par = par->next;
        }
    }

    /* this a argumenty do registru (System V), prebytecne na zasobnik */
    ThisCode * code_this = NULL;
    if (fce->method)
    {
        code_this = (ThisCode *) MemAlloc (sizeof (ThisCode));
        code_this->object = object;
        if (object == NULL)
            code_this->kind = THIS_LOAD;
        else if (f->op == PERIOD)
            code_this->kind = THIS_ADDR;
        else
            code_this->kind = THIS_EXPR;
    }
    /* vysledek - struktura hodnotou: docasne misto v ramci volajiciho */
    long sret_off = 0;
    if (isStructVal (fce->type->from))
    {
        checkStructVal (fce->type->from);
        sret_off = allocTemp (structCount (fce->type->from));
        Seq * owned = SeqNew ();     /* OwnedMember * */
        collectContMembers (fce->type->from, 0, owned);
        if (! SeqEmpty (owned) && current_fce != NULL)
        {
            PendingTemp * t = (PendingTemp *) MemAlloc (sizeof (PendingTemp));   /* uvolni se po vyrazu (releaseTemps) */
            t->off = sret_off;
            t->type = fce->type->from;
            SeqAdd (&pending_temps, (long) t);
        }
    }
    int nxmm = 0;
    long stack_drop = passArgs (code_this, args, ptypes, &nxmm, sret_off);

    bool pad = (depth % 2) != 0;               /* zarovnani rsp na 16 bajtu */
    if (pad) ins2 (iSub, gp (RSP), imm (8));

    if (is_virtual)
    {
        /* call [[this + vptr] + 8 * slot] */
        structCount (cls);
        note (ins2 (iMov, gp (R11), qword_ptr (RDI, DictGet (&vptr_offsets, (long) cls))), "vtable");
        note (ins1 (iCall, qword_ptr (R11, vtableSlot (cls, fce) * VAR_SIZE)), Cat4 ("virtual ", className (cls), "::", fce->name));
    }
    else
    {
        if (nxmm == 0)
            ins2 (iXor, gp (EAX), gp (EAX));          /* al = pocet registru XMM (variadicke funkce) */
        else
            ins2 (iMov, gp (EAX), imm (nxmm));
        char * title = fce->name;
        if (fce->owner != NULL) title = Cat3 (className (fce->owner), "::", title);
        note (ins1 (iCall, jlab (target)), title);
    }

    if (pad) ins2 (iAdd, gp (RSP), imm (8));

    if (stack_drop > 0)                        /* argumenty na zasobniku */
    {
        ins2 (iAdd, gp (RSP), imm (stack_drop * VAR_SIZE));
        depth -= stack_drop;
    }

    /* vysledek float / double v xmm0 */
    TypePtr res = fce->type->from;
    if (fpKind (res) != 0)
    {
        fromXmm (fpKind (res));
        return;
    }
    if (ownedTemp (res))            /* std::string, kontejner - docasna hodnota */
        return;

    /* cele cislo jen v dolni casti rax (knihovni funkce, ABI) => rozsirit podle typu */
    if (res != NULL && isIntegralType (res))
        normalizeTo (res);
}

static void codeExpr (ExprPtr expr)
{
    if (expr == NULL) return;

    SYMBOL kind = expr->op;
    /* see struct Expr in decl.h */

    switch (kind)
    {
        case NUM:
            ins2 (iMov, gp (RAX), imm ((long) strtoul (expr->id, NULL, 0)));
            break;

        case CHR:
            ins2 (iMov, gp (RAX), imm ((signed char) expr->id [0]));
            break;

        case FLT:                                   /* 3.5, 1e-3, 2.5f */
        {
            int k = fpKind (expr->type);
            if (k == 0) k = 2;
            note (ins2 (iMov, gp (RAX), imm_hex (fpBits (strtod (expr->id, NULL), k))), expr->id);
            break;
        }

        case TRUE_:
            ins2 (iMov, gp (EAX), imm (1));
            break;

        case FALSE_:
        case NULLPTR_:
            ins2 (iXor, gp (EAX), gp (EAX));
            break;

        case STR:
            ins2 (iLea, gp (RAX), alab (stringLabel (expr->id)));
            break;

        case IDENT:
        {
            long value;
            if (expr->obj != NULL && expr->obj->fce)   /* jmeno funkce => adresa */
                note (ins2 (iLea, gp (RAX), alab (fnLabel (expr->obj))), Cat ("&", expr->id));
            else if (enumValue (expr->obj, &value))    /* vyctova konstanta */
                ins2 (iMov, gp (RAX), imm (value));
            else if (IsStaticMember (expr->obj))  /* static const int max = 10; */
            {
                if (!constValue (expr, &value))
                    InstrError (Cat ("Static data members are supported only as integer constants: ", expr->id));
                ins2 (iMov, gp (RAX), imm (value));
            }
            else
                codeLoad (expr);
            break;
        }

        case LBRACK:
            if (IsStdString (SkipParens (expr->e1)->type))   /* s [i] - znak retezce */
            {
                codeExpr (expr->e1);
                ins2 (iMov, gp (RDI), gp (RAX));
                callRt ("RT_SCSTR", "");
                push (RAX);
                codeExpr (expr->e2);
                ins2 (iMov, gp (RCX), gp (RAX));
                pop (RAX);
                ins2 (iAdd, gp (RAX), gp (RCX));
                ins2 (iMovsx, gp (RAX), byte_ptr (RAX, 0));
                break;
            }
            codeLoad (expr);
            break;

        case PERIOD:
        case ARROW:
        {
            long value;
            if (IsStaticMember (expr->obj))      /* p->max - staticka konstanta */
            {
                if (!constValue (expr->obj->init, &value))
                    InstrError (Cat ("Static data members are supported only as integer constants: ", expr->id));
                ins2 (iMov, gp (RAX), imm (value));
                break;
            }
            codeLoad (expr);
            break;
        }

        case DEREF:
            if (expr->type != NULL && expr->type->sort == Function)
                codeExpr (expr->e2);          /* *f - ukazatel na funkci */
            else
                codeLoad (expr);
            break;

        case ADR:
            codeAddress (expr->e2);
            break;

        case NEW_:
            codeNew (expr);
            break;

        case STATIC_CAST_:                     /* (T) e,  static_cast <T> (e) */
            codeCast (expr);
            break;

        case DYNAMIC_CAST_:                    /* dynamic_cast <T *> (p) */
            if (IsClsSort (expr->type->sort))
                codeAddress (expr);           /* dynamic_cast <T &> - adresa objektu */
            else
            {
                codeExpr (expr->e1);
                codeDynamicCheck (SkipParens (expr->e1)->type->from, expr->type->from, false, expr);
            }
            break;

        case DELETE_:                          /* delete p => destruktor, free */
            codeDelete (expr);
            break;

        case SIZEOF_:
        {
            long value;
            constValue (expr, &value);
            ins2 (iMov, gp (RAX), imm (value));
            break;
        }

        case THIS_:
            loadThis ();
            break;

        case DCOLON:
            InstrError ("Method call expected");
            break;

        case LPAR:
            codeExpr (expr->e1);
            break;

        case FCE:
            codeCall (expr);
            break;

        case ASSIGN:
        case ADDASSIGN: case SUBASSIGN: case MULASSIGN: case DIVASSIGN: case MODASSIGN:
        case SHLASSIGN: case SHRASSIGN: case ANDASSIGN: case ORASSIGN: case XORASSIGN:
            codeAssign (expr);
            break;

        case DPLUS:
        case DMINUS:
            codeIncDec (expr);
            break;

        case LOGAND:
        case LOGOR:
            codeAndOr (expr);
            break;

        case QUESTION:
            codeConditional (expr);
            break;

        case COMMA:
            codeExpr (expr->e1);
            codeExpr (expr->e2);
            break;

        case PLUS: case MINUS: case ASTERISK: case SLASH: case MOD:
        case SHL: case SHR: case AND: case OR: case XOR:
        case LESS: case LESSEQUAL: case GREATER: case GREATEREQUAL:
        case EQUAL: case UNEQUAL:
            codeBinary (expr);
            break;

        case UNARYPLUS:
            codeExpr (expr->e2);
            break;

        case UNARYMINUS:
            codeExpr (expr->e2);
            if (fpKind (expr->e2->type) != 0)
            {
                /* zmena znamenkoveho bitu */
                if (fpKind (expr->e2->type) == 2) ins2 (iMov, gp (RCX), imm_hex_lit (0x8000000000000000UL)); else ins2 (iMov, gp (ECX), imm_hex_lit (0x80000000UL));
                ins2 (iXor, gp (RAX), gp (RCX));
                convert (expr->e2->type, expr->type);
            }
            else
            {
                ins1 (iNeg, gp (RAX));
                long size;
                bool uns;
                arithType (expr->e2->type, NULL, &size, &uns);
                normalizeSize (size, uns);      /* -x v typu int / unsigned (jako v C) */
            }
            break;

        case UNARYNOT:            /* ~ */
        {
            codeExpr (expr->e2);
            ins1 (iNot, gp (RAX));
            long size;
            bool uns;
            arithType (expr->e2->type, NULL, &size, &uns);
            normalizeSize (size, uns);
            break;
        }

        case LOGNOT:              /* ! */
            codeBoolValue (expr->e2);
            ins2 (iTest, gp (RAX), gp (RAX));
            ins1 (iSete, gp (AL));
            ins2 (iMovzx, gp (RAX), gp (AL));
            break;

        default:
            InstrError ("Unsupported expression");
    }
}

/* ------------------------------------------------------------------ */

/* vyhodnot podminku a nastav priznaky (ZF=1 <=> nepravda) */
static void codeCond (ExprPtr expr)
{
    long mark = SeqSize (&pending_temps);
    codeBoolValue (expr);
    releaseTemps (mark);
    ins2 (iTest, gp (RAX), gp (RAX));
}

/* lokalni objekt k uvolneni na konci aktualniho bloku */
static void cleanupAdd (ObjectPtr obj)
{
    ObjectList * l = (ObjectList *) SeqLast (&cleanup);
    SeqAdd (&l->items, (long) obj);
}

/* deklarace lokalnich promennych - inicializace */
static void codeLocalDecls (Queue * decl)
{
    NodePtr p;
    for (p = decl->first; p != NULL; p = p->next)
    {
        ObjectPtr obj = p->item;
        if (! DictContains (&vars, (long) obj))
            continue;         /* typedef, extern, deklarace funkce */

        Variable * var = (Variable *) DictGet (&vars, (long) obj);
        if (var->global)
            continue;         /* staticka lokalni promenna - inicializovana v datech */

        /* Sequence / Dictionary: kopie inicializace nebo prazdny */
        if (IsContainer (ArrayElem (obj->type)))
        {
            if (obj->init != NULL)
            {
                if (obj->type->sort == Array)
                    InstrError (Cat ("Initialization of arrays of Sequence / Dictionary is not supported: ", obj->name));
                codeContValue (obj->init);
                note (ins2 (iMov, qword_ptr (RBP, var->offset), gp (RAX)), obj->name);
            }
            else
            {
                note (ins2 (iLea, gp (RAX), any_ptr (RBP, var->offset)), obj->name);
                zeroVar (obj->type);
            }
            if (! SeqEmpty (&cleanup) && obj->type->sort != Reference)
                cleanupAdd (obj);     /* uvolni se na konci bloku */
            continue;
        }

        /* std::string: inicializace nebo prazdny retezec */
        TypePtr st = isValueRef (obj->type) ? obj->type->from : obj->type;
        if (IsStdString (ArrayElem (st)))
        {
            if (obj->init != NULL && st->sort == Array)
            {
                /* string a [] = { "x", "y" } */
                note (ins2 (iLea, gp (RAX), any_ptr (RBP, var->offset)), obj->name);
                zeroVar (obj->type);
                codeLocalInit (obj->type, obj->init, var->offset, obj->name);
            }
            else if (obj->init != NULL)
            {
                codeStringValue (obj->init);
                if (! isValueRef (obj->type))
                {
                    ins2 (iMov, gp (RDI), gp (RAX));
                    callRt ("RT_SINC", "");       /* promenna vlastni referenci */
                }
                note (ins2 (iMov, qword_ptr (RBP, var->offset), gp (RAX)), obj->name);
            }
            else
            {
                note (ins2 (iLea, gp (RAX), any_ptr (RBP, var->offset)), obj->name);
                zeroVar (obj->type);
            }
            if (! SeqEmpty (&cleanup) && ! isValueRef (obj->type) && obj->type->sort != Reference)
                cleanupAdd (obj);     /* reference se uvolni na konci bloku */
            continue;
        }

        TypePtr elem = ArrayElem (obj->type);
        if (elem != NULL && IsClsSort (elem->sort))
        {
            /* T x = y (y typu T): kopie po slotech, destruktor na konci bloku */
            if (obj->init != NULL && obj->type == elem && obj->init->op != LBRACE &&
                obj->init->type == elem && (elem->ctor != NULL || IsPolymorphic (elem) || NeedsDtor (elem, true)))
            {
                checkStructCopy (elem);
                codeLocalInit (obj->type, obj->init, var->offset, obj->name);
                if (NeedsDtor (elem, true) && ! SeqEmpty (&cleanup))
                    cleanupAdd (obj);
                continue;
            }
            if (obj->init != NULL && (elem->ctor != NULL || IsPolymorphic (elem)))
                InstrError (Cat ("Initialization of objects with constructor is not supported: ", obj->name));
            if (obj->init != NULL)
            {
                /* struktura (pole struktur) s inicializaci { ... } */
                note (ins2 (iLea, gp (RAX), any_ptr (RBP, var->offset)), obj->name);
                zeroVar (obj->type);
                if (NeedsCtor (elem, true))
                {
                    note (ins2 (iLea, gp (RAX), any_ptr (RBP, var->offset)), obj->name);
                    constructVar (obj->type);
                }
                codeLocalInit (obj->type, obj->init, var->offset, obj->name);
            }
            else if (NeedsCtor (elem, true))
            {
                note (ins2 (iLea, gp (RAX), any_ptr (RBP, var->offset)), obj->name);
                constructVar (obj->type);
            }
            if (NeedsDtor (elem, true) && ! SeqEmpty (&cleanup))
                cleanupAdd (obj);     /* destruktor na konci bloku */
            continue;
        }

        if (obj->init == NULL)
        {
            if (var->ref)
                InstrError (Cat ("Reference must be initialized: ", obj->name));
            continue;
        }

        if (obj->type->sort == Array)
        {
            /* int a [3] = { 1, 2, 3 },  char s [] = "abc" - zbytek nulovy */
            note (ins2 (iLea, gp (RAX), any_ptr (RBP, var->offset)), obj->name);
            zeroVar (obj->type);
            codeLocalInit (obj->type, obj->init, var->offset, obj->name);
            continue;
        }
        if (obj->init->op == LBRACE)
            InstrError (Cat ("Initializer { ... } is supported only for arrays and structures: ", obj->name));

        if (var->ref)
            codeAddress (obj->init);
        else
        {
            codeExprAs (obj->init, obj->type);
            if (obj->type->sort == Bool && fpKind (obj->init->type) == 0)
                normalizeBool ();
        }
        note (ins2 (iMov, qword_ptr (RBP, var->offset), gp (RAX)), obj->name);
    }
}

/* ---- uvolnovani pameti (pocitani referenci) -------------------------- */

/* rax = novy retezec (1 reference) => docasna hodnota (uvolni se na
   zacatku pristiho prikazu, pokud ji nevezme promenna) */
static void strTemp (void)
{
    ins2 (iMov, gp (RDI), gp (RAX));
    callRt ("RT_STEMP", "");
}

/* vysledek volani typu std::string / kontejner => docasna hodnota */
static bool ownedTemp (TypePtr res)
{
    if (IsStdString (res)) { strTemp (); return true; }
    if (IsContainer (res))
    {
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt (IsDictionary (res) ? "RT_DTEMP" : "RT_QTEMP", "");
        return true;
    }
    return false;
}

/* funkce pracuje s retezci / kontejnery => docasne hodnoty, znacka */
static bool exprOwned (ExprPtr e)
{
    if (e == NULL) return false;
    if (e->type != NULL && (e->type->sort == StdString || e->type->sort == StdSequence ||
                            e->type->sort == StdDictionary))
        return true;
    return exprOwned (e->e1) || exprOwned (e->e2) || exprOwned (e->e3);
}

static bool statOwned (StatPtr s)
{
    for (; s != NULL; s = s->next)
    {
        NodePtr p;
        if (exprOwned (s->e1) || exprOwned (s->e2) || exprOwned (s->e3)) return true;
        for (p = s->decl.first; p != NULL; p = p->next)
            if (exprOwned (p->item->init)) return true;
        if (statOwned (s->s1) || statOwned (s->s2)) return true;
    }
    return false;
}
/* ---- Sequence <T>, Dictionary <K, V> --------------------------------- */

/* hodnota kontejneru: z l-hodnoty kopie (hodnotova semantika jako v C++),
   vysledek volani funkce uz je nova hodnota */
static void codeContValue (ExprPtr e)
{
    codeExpr (e);
    ExprPtr x = SkipParens (e);
    if (x->op == FCE)
    {
        /* vysledek volani: docasnou hodnotu prevezme promenna (bez kopie) */
        ins2 (iMov, gp (RDI), gp (RAX));
        ins2 (iOr, gp (RDI), imm (IsDictionary (e->type) ? 2 : 1));
        callRt ("RT_TSTEAL", "");
        return;
    }
    TypePtr ct = Resolve (e->type);
    if (ct != NULL && ct->sort == Reference) ct = Resolve (ct->from);
    if (ct != NULL && isBoxedElem (ct->elem))
        InstrError ("Copy of Sequence / Dictionary of structures is not supported (instr.cc)");
    ins2 (iMov, gp (RDI), gp (RAX));
    callRt (IsDictionary (e->type) ? "RT_DCOPY" : "RT_QCOPY", "kopie");
}

/* polozky typu std::string, Sequence, Dictionary ve strukture (i v bazi,
   vnorenych objektech a polich): (posun v bajtech, typ) */
static void collectContMembers (TypePtr cls, long base, Seq * out)
{
    NodePtr p;
    structCount (cls);
    if (cls->base != NULL)
        collectContMembers (cls->base, base, out);
    for (p = cls->list.first; p != NULL; p = p->next)
    {
        ObjectPtr m = p->item;
        if (! IsField (m)) continue;
        TypePtr t = ArrayElem (m->type);
        long off = base + DictGet (&member_offsets, (long) m);
        long n = (m->type->sort == Array) ? typeBytes (m->type) / typeBytes (t) : 1;
        long k;
        for (k = 0; k < n; k++)
        {
            if (isZeroInit (t))
            {
                OwnedMember * om = (OwnedMember *) MemAlloc (sizeof (OwnedMember));
                om->offset = off + k * VAR_SIZE;
                om->type = t;
                SeqAdd (out, (long) om);
            }
            else if (t != NULL && IsClsSort (t->sort))
                collectContMembers (t, off + k * typeBytes (t), out);
        }
    }
}

/* druh hodnoty pro porovnani v behove knihovne */
static int cmpKind (TypePtr t)
{
    if (IsStdString (t)) return 1;
    if (fpKind (t) == 2) return 2;
    if (fpKind (t) == 1) return 3;
    return 0;
}

/* vestavene metody Sequence a Dictionary;  f = obj.m / p->m */
static void codeContMethod (ExprPtr expr, ExprPtr f, ObjectPtr fce, TypePtr cls)
{
    ExprPtr object = f->e1;
    char * name = fce->name;
    Seq * args = SeqNew ();
    (void) object;
    collectArgs (expr->e2, args);


    if (StrEq (name, "size") || StrEq (name, "empty"))
    {
        contHandle (f);
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt (IsSequence (cls) ? "RT_QSIZE" : "RT_DSIZE", "");
        if (StrEq (name, "empty"))
        {
            ins2 (iTest, gp (RAX), gp (RAX));
            ins1 (iSete, gp (AL));
            ins2 (iMovzx, gp (RAX), gp (AL));
        }
        return;
    }
    if (StrEq (name, "clear"))
    {
        contSlotAddr (f);
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt (IsSequence (cls) ? "RT_QCLEAR" : "RT_DCLEAR", "");
        return;
    }

    if (IsSequence (cls))
    {
        TypePtr T = cls->elem;
        if (StrEq (name, "add") || StrEq (name, "push") || StrEq (name, "insertFirst"))
        {
            contSlotAddr (f);
            push (RAX);
            codeArg ((ExprPtr) SeqGet (args, 0), T);
            if (isBoxedElem (T)) boxElem (T);  /* struktura: kopie na halde */
            if (IsStdString (T))
            {
                ins2 (iMov, gp (RDI), gp (RAX));
                callRt ("RT_SINC", "");             /* prvek vlastni referenci */
            }
            ins2 (iMov, gp (RSI), gp (RAX));
            pop (RDI);
            ins2 (iMov, gp (EDX), imm (elemKind (T)));
            callRt (StrEq (name, "insertFirst") ? "RT_QINSF" : "RT_QADD", "");
        }
        else if (StrEq (name, "get"))
        {
            contHandle (f);
            push (RAX);
            codeExpr ((ExprPtr) SeqGet (args, 0));
            ins2 (iMov, gp (RSI), gp (RAX));
            pop (RDI);
            callRt ("RT_QREF", "");
            ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));
        }
        else if (StrEq (name, "change"))
        {
            contHandle (f);
            push (RAX);
            codeExpr ((ExprPtr) SeqGet (args, 0));
            push (RAX);
            codeArg ((ExprPtr) SeqGet (args, 1), T);
            if (isBoxedElem (T)) boxElem (T);
            ins2 (iMov, gp (RDX), gp (RAX));
            pop (RSI);
            pop (RDI);
            push (RDX);
            callRt ("RT_QREF", "");
            pop (RDX);
            storeElem (T);
        }
        else if (StrEq (name, "contains"))
        {
            if (isBoxedElem (T)) InstrError ("Sequence <struct>::contains is not supported");
            contHandle (f);
            push (RAX);
            codeArg ((ExprPtr) SeqGet (args, 0), T);
            ins2 (iMov, gp (RSI), gp (RAX));
            pop (RDI);
            ins2 (iMov, gp (EDX), imm (cmpKind (T)));
            callRt ("RT_QFIND", "");
        }
        else if (StrEq (name, "last") || StrEq (name, "top"))
        {
            contHandle (f);
            ins2 (iMov, gp (RDI), gp (RAX));
            push (RDI);
            callRt ("RT_QSIZE", "");
            ins2 (iLea, gp (RSI), any_ptr (RAX, -1));
            pop (RDI);
            callRt ("RT_QREF", "");
            ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));
        }
        else if (StrEq (name, "pop"))
        {
            contSlotAddr (f);
            ins2 (iMov, gp (RDI), gp (RAX));
            callRt ("RT_QPOP", "");
        }
        else
            InstrError (Cat ("Unknown method of Sequence: ", name));
        return;
    }

    codeDictMethod (name, args, cls, f);
}

/* adresa promenne kontejneru (zmeny);  f = obj.m / p->m */
static void contSlotAddr (ExprPtr f)
{
    if (f->op == ARROW) codeExpr (f->e1);
    else codeAddress (f->e1);
}

/* hodnota (hlavicka) kontejneru */
static void contHandle (ExprPtr f)
{
    codeExpr (f->e1);
    if (f->op == ARROW) ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));
}

static void codeDictMethod (const char * name, Seq * args, TypePtr cls, ExprPtr f)
{
    TypePtr K = cls->key;
    TypePtr V = cls->elem;
    if (StrEq (name, "store"))
    {
        contSlotAddr (f);
        push (RAX);
        codeArg ((ExprPtr) SeqGet (args, 0), K);
        push (RAX);
        codeArg ((ExprPtr) SeqGet (args, 1), V);
        if (isBoxedElem (V)) boxElem (V);
        push (RAX);
        ins2 (iMov, gp (RSI), qword_ptr (RSP, 8));
        ins2 (iMov, gp (RDI), qword_ptr (RSP, 16));
        ins2 (iMov, gp (EDX), imm (dictKind (K)));
        ins2 (iMov, gp (ECX), imm (1));
        ins2 (iMov, gp (R8D), imm (elemKind (V)));
        callRt ("RT_DREF", "");
        pop (RDX);
        storeElem (V);
        ins2 (iAdd, gp (RSP), imm (16));
        depth -= 2;
    }
    else if (StrEq (name, "get") || StrEq (name, "contains"))
    {
        contHandle (f);
        push (RAX);
        codeArg ((ExprPtr) SeqGet (args, 0), K);
        ins2 (iMov, gp (RSI), gp (RAX));
        pop (RDI);
        callRt (StrEq (name, "get") ? "RT_DGET" : "RT_DHAS", "");
        if (StrEq (name, "get") && isBoxedElem (V)) unboxElem (V, false);
    }
    else
        InstrError (Cat ("Unknown method of Dictionary: ", name));
}

/* druh prvku kontejneru (uvolnovani): 0 jiny, 1 std::string */
static int elemKind (TypePtr T)
{
    return IsStdString (T) ? 1 : 0;
}

/* rax = adresa prvku, rdx = nova hodnota typu T */
static void storeElem (TypePtr T)
{
    if (IsStdString (T))
    {
        ins2 (iMov, gp (RDI), gp (RAX));
        ins2 (iMov, gp (RSI), gp (RDX));
        callRt ("RT_SASSIGN", "");
    }
    else
        ins2 (iMov, qword_ptr (RAX, 0), gp (RDX));
}

/* prvek kontejneru struktura (Sequence <DtorVar> ve zdrojich cmm): ve slotu je ukazatel
   na vlastni kopii na halde; s [i], get, last vraci jeji adresu (hodnota struktury
   = adresa), add, change, store ulozi novou kopii, d [k] s novym klicem ji zalozi;
   kopie celeho kontejneru neni podporovana (sdilene kopie) */
static bool isBoxedElem (TypePtr T)
{
    return isStructVal (Resolve (T));
}

/* rax = adresa struktury typu T => rax = jeji kopie na halde */
static void boxElem (TypePtr T)
{
    TypePtr R = Resolve (T);
    push (RAX);
    ins2 (iMov, gp (EDI), imm (1));
    ins2 (iMov, gp (ESI), imm (structCount (R) * VAR_SIZE));
    callRt ("RT_CALLOC", "");
    pop (RSI);
    ins2 (iMov, gp (RDI), gp (RAX));
    copyBytes (structBytes (R));            /* rax zustava */
}

/* rax = adresa slotu => rax = adresa struktury ve slotu; prazdny slot (novy klic
   slovniku, chybejici klic v get) - nova vynulovana struktura (store_slot: ulozit do slotu) */
static void unboxElem (TypePtr T, bool store_slot)
{
    TypePtr R = Resolve (T);
    char * ok = newLabel ("L");
    if (store_slot)
    {
        ins2 (iMov, gp (RCX), qword_ptr (RAX, 0));
        ins2 (iTest, gp (RCX), gp (RCX));
        ins1 (iJnz, jlab (ok));
        push (RAX);
        ins2 (iMov, gp (EDI), imm (1));
        ins2 (iMov, gp (ESI), imm (structCount (R) * VAR_SIZE));
        callRt ("RT_CALLOC", "");
        pop (RDX);
        ins2 (iMov, qword_ptr (RDX, 0), gp (RAX));
        ins2 (iMov, gp (RCX), gp (RAX));
        bind (ok);
        ins2 (iMov, gp (RAX), gp (RCX));
        return;
    }
    ins2 (iTest, gp (RAX), gp (RAX));         /* rax = obsah slotu (RT_DGET) */
    ins1 (iJnz, jlab (ok));
    ins2 (iMov, gp (EDI), imm (1));
    ins2 (iMov, gp (ESI), imm (structCount (R) * VAR_SIZE));
    callRt ("RT_CALLOC", "");
    bind (ok);
}

/* druh klice slovniku: 0 cislo / ukazatel, 1 std::string */
static int dictKind (TypePtr K)
{
    return IsStdString (K) ? 1 : 0;
}

/* rax := adresa prvku kontejneru:  seq [i],  dict [k] */
static void codeContElemAddress (ExprPtr expr)
{
    ExprPtr arr = SkipParens (expr->e1);
    if (IsSequence (arr->type))
    {
        codeExpr (arr);                   /* hlavicka */
        push (RAX);
        codeExpr (expr->e2);
        ins2 (iMov, gp (RSI), gp (RAX));
        pop (RDI);
        callRt ("RT_QREF", "");
        if (isBoxedElem (arr->type->elem)) ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));   /* struktura */
        return;
    }
    /* dict [k] - adresa hodnoty, chybejici klic se vlozi (jako std::map) */
    codeAddress (arr);
    push (RAX);
    codeArg (expr->e2, arr->type->key);
    ins2 (iMov, gp (RSI), gp (RAX));
    pop (RDI);
    ins2 (iMov, gp (EDX), imm (dictKind (arr->type->key)));
    ins2 (iMov, gp (ECX), imm (1));
    ins2 (iMov, gp (R8D), imm (elemKind (arr->type->elem)));
    callRt ("RT_DREF", "");
    if (isBoxedElem (arr->type->elem)) unboxElem (arr->type->elem, true);
}

/* for (T x : seq),  for (T & x : seq):  [h] = hlavicka, [i] = index */
static void codeRangeFor (StatPtr stat)
{
    comment ("for (T x : seq)");
    ObjectPtr var = stat->decl.first->item;
    int h = (int) DictGet (&range_slots, (long) stat);
    int i = h + 8;
    char * begin_label = newLabel ("L");
    char * next_label = newLabel ("L");
    char * end_label = newLabel ("L");
    Variable * vv = (Variable *) DictGet (&vars, (long) var);

    bool str = IsStdString (var->type);           /* promenna vlastni referenci */
    bool steal = SkipParens (stat->e1)->op == FCE;
    if (str)
        note (ins2 (iMov, qword_ptr (RBP, vv->offset), imm (0)), var->name);
    if (steal)
        codeContValue (stat->e1);           /* vysledek volani - prevzit, uvolnit po cyklu */
    else
        codeExpr (stat->e1);
    ins2 (iMov, qword_ptr (RBP, h), gp (RAX));
    ins2 (iMov, qword_ptr (RBP, i), imm (0));
    bind (begin_label);
    ins2 (iMov, gp (RDI), qword_ptr (RBP, h));
    callRt ("RT_QSIZE", "");
    ins2 (iCmp, qword_ptr (RBP, i), gp (RAX));
    ins1 (iJae, jlab (end_label));
    ins2 (iMov, gp (RDI), qword_ptr (RBP, h));
    ins2 (iMov, gp (RSI), qword_ptr (RBP, i));
    callRt ("RT_QREF", "");
    if (str)
    {
        ins2 (iMov, gp (RSI), qword_ptr (RAX, 0));
        note (ins2 (iLea, gp (RDI), any_ptr (RBP, vv->offset)), var->name);
        callRt ("RT_SASSIGN", "");
    }
    else if (isBoxedElem (Resolve (stat->e1->type)->sort == Reference ? Resolve (Resolve (stat->e1->type)->from)->elem
                                                                     : Resolve (stat->e1->type)->elem))
    {
        ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));         /* adresa struktury */
        if (var->type->sort == Reference)
            note (ins2 (iMov, qword_ptr (RBP, vv->offset), gp (RAX)), var->name);
        else
        {
            ins2 (iMov, gp (RSI), gp (RAX));            /* kopie struktury do promenne */
            note (ins2 (iLea, gp (RDI), any_ptr (RBP, vv->offset)), var->name);
            copyBytes (structBytes (Resolve (var->type)));
        }
    }
    else
    {
        if (var->type->sort != Reference)
            ins2 (iMov, gp (RAX), qword_ptr (RAX, 0));     /* hodnota prvku */
        note (ins2 (iMov, qword_ptr (RBP, vv->offset), gp (RAX)), var->name);
    }

    /* return uvnitr cyklu uvolni promennou (a prevzaty kontejner) */
    SeqAdd (&cleanup, (long) MemAlloc (sizeof (ObjectList)));
    if (str) SeqAdd (& ((ObjectList *) SeqLast (&cleanup))->items, (long) var);
    if (steal)
    {
        ObjectPtr hidden = newObj ("range", stat->e1->type);
        Variable * hv = Variable_new ();
        hv->offset = h;
        DictStore (&vars, (long) hidden, (long) hv);
        SeqAdd (& ((ObjectList *) SeqLast (&cleanup))->items, (long) hidden);
    }
    SeqAdd (&loops, (long) LoopLabels_new (end_label, next_label, SeqSize (&cleanup), SeqSize (&cleanup)));
    codeStat (stat->s1);
    SeqPop (&loops);
    SeqPop (&cleanup);

    bind (next_label);
    ins1 (iInc, qword_ptr (RBP, i));
    ins1 (iJmp, jlab (begin_label));
    bind (end_label);
    if (str)
    {
        note (ins2 (iMov, gp (RDI), qword_ptr (RBP, vv->offset)), var->name);
        callRt ("RT_SDEC", "");
    }
    if (steal)
    {
        ins2 (iMov, gp (RDI), qword_ptr (RBP, h));
        callRt ("RT_QFREE", "");
    }
}

/* inicializace lokalni promenne (pole, struktury) po prvcich;
   promenna uz je vynulovana, off = posun v ramci [rbp + off] */
static void codeLocalInit (TypePtr type, ExprPtr init, long off, const char * name)
{
    init = SkipParens (init);

    if (type->sort == Array)
    {
        TypePtr elem = type->from;
        long n;
        long i;
        if (!constValue (type->range, &n)) InstrError ("Array size must be a positive constant");
        if (isByteType (elem) && init->op == LBRACE)  /* unsigned char b [n] = { 0x7F, 'E', ... } */
        {
            Seq * items = SeqNew ();
            flattenList (init->e1, items);
            if (SeqSize (items) > n)
                InstrError (Cat ("Too many initializers: ", name));
            for (i = 0; i < SeqSize (items); i++)
            {
                codeExprAs ((ExprPtr) SeqGet (items, i), elem);
                ins2 (iMov, byte_ptr (RBP, (int) (off + i)), gp (AL));
            }
            return;
        }
        if (isByteType (elem))                        /* char s [n] = "abc" */
        {
            if (init->op != STR)
                InstrError (Cat ("Character array must be initialized by string constant: ", name));
            char * txt = Str (init->id);
            long len = strlen (txt);
            for (i = 0; i < n && i < len; i++)
                ins2 (iMov, byte_ptr (RBP, (int) (off + i)), imm ((int) (unsigned char) txt [i]));
            return;
        }
        if (init->op != LBRACE)
            InstrError (Cat ("Array initializer { ... } expected: ", name));
        Seq * items = SeqNew ();
        flattenList (init->e1, items);
        if (SeqSize (items) > n)
            InstrError (Cat ("Too many initializers: ", name));
        long step = typeBytes (elem);
        for (i = 0; i < SeqSize (items); i++)
            codeLocalInit (elem, (ExprPtr) SeqGet (items, i), off + i * step, name);
        return;
    }

    if (IsClsSort (type->sort))
    {
        if (init->op != LBRACE)
        {
            /* T x = y;  T x = f ();  - kopie (struktura hodnotou) */
            checkStructCopy (type);
            long mark = SeqSize (&pending_temps);
            codeStructAddr (init);
            ins2 (iMov, gp (RSI), gp (RAX));
            ins2 (iLea, gp (RDI), any_ptr (RBP, (int) off));
            copyBytes (structBytes (type));
            ins2 (iLea, gp (RDI), any_ptr (RBP, (int) off));
            incOwned (type);
            releaseTemps (mark);               /* docasny objekt volani uz neni potreba */
            return;
        }
        if (IsPolymorphic (type) || type->base != NULL)
            InstrError (Cat ("Initialization of polymorphic or derived objects is not supported: ", name));
        structCount (type);
        Seq * items = SeqNew ();
        flattenList (init->e1, items);
        long i = 0;
        NodePtr p;
        for (p = type->list.first; p != NULL && i < SeqSize (items); p = p->next)
        {
            ObjectPtr m = p->item;
            if (! IsField (m))
                continue;
            codeLocalInit (m->type, (ExprPtr) SeqGet (items, i), off + DictGet (&member_offsets, (long) m), name);
            i ++;
        }
        if (i < SeqSize (items))
            InstrError (Cat ("Too many initializers: ", name));
        return;
    }

    if (IsStdString (type))
    {
        codeStringValue (init);
        ins2 (iMov, gp (RDI), gp (RAX));
        callRt ("RT_SINC", "");
    }
    else
    {
        codeExprAs (init, type);
        if (type->sort == Bool && fpKind (init->type) == 0)
            normalizeBool ();
    }
    ins2 (iMov, qword_ptr (RBP, (int) off), gp (RAX));
}

static void codeIf (StatPtr stat)
{
    /* if (e1) s1 else s2 */
    comment ("if statement");
    char * end_label = newLabel ("L");
    codeCond (stat->e1);

    if (stat->s2 == NULL)
    {
        ins1 (iJz, jlab (end_label));
        codeStat (stat->s1);
    }
    else
    {
        char * else_label = newLabel ("L");
        ins1 (iJz, jlab (else_label));
        codeStat (stat->s1);
        ins1 (iJmp, jlab (end_label));

        comment ("else");
        bind (else_label);
        codeStat (stat->s2);
    }

    bind (end_label);
}

/* navesti pro goto (v ramci funkce) */
static char * userLabel (const char * id)
{
    if (SDictContains (&user_labels, id))
        return SDictGetStr (&user_labels, id);
    char * label = newLabel ("L");
    SDictStore (&user_labels, id, (long) label);
    return label;
}

/* case a default patrici k tomuto switch (ne k vnorenym) */
static void collectCases (StatPtr stat, Seq * cases)
{
    StatPtr s;
    if (stat == NULL) return;
    switch (stat->st)
    {
        case CASE_:
        case DEFAULT_:
            SeqAdd (cases, (long) stat);
            break;
        case LBRACE:
            for (s = stat->s1; s != NULL; s = s->next)
                collectCases (s, cases);
            break;
        case IF_:
            collectCases (stat->s1, cases);
            collectCases (stat->s2, cases);
            break;
        case WHILE_: case DO_: case FOR_: case LAB: case COLON:
            collectCases (stat->s1, cases);
            break;
        default:              /* SWITCH_ - vlastni case */
            break;
    }
}

static void codeSwitch (StatPtr stat)
{
    /* switch (e1) s1 - porovnani s hodnotami case, skok na navesti */
    comment ("switch statement");
    char * end_label = newLabel ("L");

    Seq * cases = SeqNew ();
    collectCases (stat->s1, cases);

    codeExpr (stat->e1);
    char * default_label = end_label;
    long ci;
    for (ci = 0; ci < SeqSize (cases); ci++)
    {
        StatPtr c = (StatPtr) SeqGet (cases, ci);
        char * label = newLabel ("L");
        DictStore (&case_labels, (long) c, (long) label);
        if (c->st == DEFAULT_)
        {
            default_label = label;
            continue;
        }
        long value;
        if (!constValue (c->e1, &value))
            InstrError ("case value must be a constant");
        if (value >= -2147483648L && value <= 2147483647L)
            ins2 (iCmp, gp (RAX), imm (value));
        else
        {
            ins2 (iMov, gp (RCX), imm (value));
            ins2 (iCmp, gp (RAX), gp (RCX));
        }
        ins1 (iJe, jlab (label));
    }
    ins1 (iJmp, jlab (default_label));

    char * cont = SeqEmpty (&loops) ? (char *) "" : ((LoopLabels *) SeqLast (&loops))->continue_label;
    long cont_depth = SeqEmpty (&loops) ? 0 : ((LoopLabels *) SeqLast (&loops))->continue_depth;
    SeqAdd (&loops, (long) LoopLabels_new (end_label, cont, SeqSize (&cleanup), cont_depth));    /* break ukonci switch */
    codeStat (stat->s1);
    SeqPop (&loops);

    bind (end_label);
}

static void codeWhile (StatPtr stat)
{
    /* while (e1) s1 */
    comment ("while statement");
    char * begin_label = newLabel ("L");
    char * end_label = newLabel ("L");

    bind (begin_label);
    codeCond (stat->e1);
    ins1 (iJz, jlab (end_label));

    SeqAdd (&loops, (long) LoopLabels_new (end_label, begin_label, SeqSize (&cleanup), SeqSize (&cleanup)));
    codeStat (stat->s1);
    SeqPop (&loops);
    ins1 (iJmp, jlab (begin_label));

    bind (end_label);
}

static void codeDo (StatPtr stat)
{
    /* do s1 while (e1) */
    comment ("do statement");
    char * begin_label = newLabel ("L");
    char * cond_label = newLabel ("L");
    char * end_label = newLabel ("L");

    bind (begin_label);
    SeqAdd (&loops, (long) LoopLabels_new (end_label, cond_label, SeqSize (&cleanup), SeqSize (&cleanup)));
    codeStat (stat->s1);
    SeqPop (&loops);

    bind (cond_label);
    codeCond (stat->e1);
    ins1 (iJnz, jlab (begin_label));

    bind (end_label);
}

static void codeFor (StatPtr stat)
{
    /* for (e1;e2;e3) s1     nebo  for (deklarace; e2; e3) s1 */
    comment ("for statement");
    char * begin_label = newLabel ("L");
    char * next_label = newLabel ("L");
    char * end_label = newLabel ("L");

    long mark0 = SeqSize (&pending_temps);
    codeLocalDecls (&stat->decl);
    codeExpr (stat->e1);              /* inicializace */
    releaseTemps (mark0);

    bind (begin_label);
    if (stat->e2 != NULL)          /* for (;;) => bez testu */
    {
        codeCond (stat->e2);
        ins1 (iJz, jlab (end_label));
    }

    SeqAdd (&loops, (long) LoopLabels_new (end_label, next_label, SeqSize (&cleanup), SeqSize (&cleanup)));
    codeStat (stat->s1);
    SeqPop (&loops);

    bind (next_label);
    long mark3 = SeqSize (&pending_temps);
    codeExpr (stat->e3);              /* inkrement */
    releaseTemps (mark3);
    ins1 (iJmp, jlab (begin_label));

    bind (end_label);
}

static void codeReturn (StatPtr stat)
{
    long mark = SeqSize (&pending_temps);
    if (stat->e1 != NULL)
    {
        if (ret_obj != NULL)
        {
            /* struktura hodnotou: kopie na misto volajiciho, vysledek = jeho adresa */
            long ret_off = ((Variable *) DictGet (&vars, (long) ret_obj))->offset;
            codeStructAddr (stat->e1);
            ins2 (iMov, gp (RSI), gp (RAX));
            ins2 (iMov, gp (RDI), qword_ptr (RBP, ret_off));
            copyBytes (structBytes (current_fce->type->from));
            ins2 (iMov, gp (RDI), qword_ptr (RBP, ret_off));
            incOwned (current_fce->type->from);
            ins2 (iMov, gp (RAX), qword_ptr (RBP, ret_off));
        }
        else if (current_fce != NULL && IsStdString (current_fce->type->from))
        {
            codeStringValue (stat->e1);        /* funkce vraci std::string */
            ins2 (iMov, gp (RDI), gp (RAX));
            callRt ("RT_SINC", "");                /* volajici dostane vlastni referenci */
        }
        else if (current_fce != NULL && IsContainer (current_fce->type->from))
            codeContValue (stat->e1);          /* funkce vraci kontejner - kopie */
        else if (current_fce != NULL)
            codeExprAs (stat->e1, current_fce->type->from);
        else
            codeExpr (stat->e1);
    }
    else
        ins2 (iXor, gp (EAX), gp (EAX));
    releaseTemps (mark);
    if (hasLocalsToDestroy (0))
    {
        push (RAX);                       /* vysledek */
        destroyLocals (0);
        pop (RAX);
    }
    ins1 (iJmp, jlab (epilogue_label));
}

static void codeCompoundStat (StatPtr stat)
{
    /* { s1 s1->next ... }  - na konci se zrusi lokalni objekty bloku */
    StatPtr body;
    SeqAdd (&cleanup, (long) MemAlloc (sizeof (ObjectList)));
    for (body = stat->s1; body != NULL; body = body->next)
        codeStat (body);
    destroyLocals (SeqSize (&cleanup) - 1);
    SeqPop (&cleanup);
}

static bool hasLocalsToDestroy (long depth)
{
    long i;
    for (i = depth; i < SeqSize (&cleanup); i++)
        if (! SeqEmpty (& ((ObjectList *) SeqGet (&cleanup, i))->items)) return true;
    return false;
}

/* destruktory lokalnich objektu v blocich od nejvnitrnejsiho po uroven depth
   (vcetne), v opacnem poradi deklarace */
static void destroyLocals (long depth)
{
    long i;
    long k;
    for (i = SeqSize (&cleanup); i-- > depth; )
    {
        ObjectList * ol = (ObjectList *) SeqGet (&cleanup, i);
        for (k = SeqSize (&ol->items); k-- > 0; )
        {
            ObjectPtr obj = (ObjectPtr) SeqGet (&ol->items, k);
            note (ins2 (iLea, gp (RAX), any_ptr (RBP, ((Variable *) DictGet (&vars, (long) obj))->offset)), Cat ("~", obj->name));
            destructVar (obj->type);
        }
    }
}

static void codeStat (StatPtr stat)
{
    if (stat == NULL) return;

    if (fn_temps && current_fce != NULL && stat->st != LBRACE)
    {
        /* docasne hodnoty predchoziho prikazu */
        ins2 (iMov, gp (RDI), qword_ptr (RBP, temp_mark));
        callRt ("RT_TREL", "");
    }

    switch (stat->st)
    {
        case SIM:
        {
            long mark = SeqSize (&pending_temps);
            codeExpr (stat->e1);
            releaseTemps (mark);
            break;
        }
        case LBRACE:  codeCompoundStat (stat);       break;
        case DECL:
        {
            long mark = SeqSize (&pending_temps);
            codeLocalDecls (&stat->decl);
            releaseTemps (mark);
            break;
        }
        case IF_:     codeIf (stat);                 break;
        case WHILE_:  codeWhile (stat);              break;
        case DO_:     codeDo (stat);                 break;
        case FOR_:    codeFor (stat);                break;
        case RETURN_: codeReturn (stat);             break;

        case BREAK_:
            if (SeqEmpty (&loops)) InstrError ("break outside of loop");
            destroyLocals (((LoopLabels *) SeqLast (&loops))->break_depth);
            ins1 (iJmp, jlab (((LoopLabels *) SeqLast (&loops))->break_label));
            break;

        case CONTINUE_:
            if (SeqEmpty (&loops) || StrEmpty (((LoopLabels *) SeqLast (&loops))->continue_label))
                InstrError ("continue outside of loop");
            destroyLocals (((LoopLabels *) SeqLast (&loops))->continue_depth);
            ins1 (iJmp, jlab (((LoopLabels *) SeqLast (&loops))->continue_label));
            break;

        case GOTO_:
            note (ins1 (iJmp, jlab (userLabel (stat->id))), Cat ("goto ", stat->id));
            break;

        case LAB:
            bind (userLabel (stat->id));
            codeStat (stat->s1);
            break;

        case SWITCH_:
            codeSwitch (stat);
            break;

        case COLON:                              /* for (T x : seq) */
            codeRangeFor (stat);
            break;

        case CASE_:
        case DEFAULT_:
        {
            if (! DictContains (&case_labels, (long) stat))
                InstrError ("case or default outside of switch");
            bind (DictGetStr (&case_labels, (long) stat));
            break;
        }

        default:
            InstrError ("Unsupported statement");
    }
}

/* ---------------------------------------------------------------------- */

long InstrLink (char * * inputs, int n_inputs, const char * out)
{
    Seq * files = SeqNew ();
    int i;
    for (i = 0; i < n_inputs; i++)
        SeqAdd (files, (long) Str (inputs [i]));
    initState ();
    InstrContext = (char *) "";
    long result = linkAndRun (files, out);
    freeState ();
    return result;
}

long runInstructions (int verbose)
{
    initState ();
    InstrContext = (char *) "";
    long result = run (verbose);
    freeState ();
    InstrContext = (char *) "";
    return result;
}

void instructions (void)
{
    long result = runInstructions (true);
    printf ("main returned %ld\n", result);
}
