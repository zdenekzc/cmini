/* REG.H - registry procesoru (spolecne pro pasm a asmlist.c - C i C++) */

#ifndef REG_H
#define REG_H

enum Register
{
   AL,  CL,  DL,  BL,  AH,  CH,  DH,  BH,
   AX,  CX,  DX,  BX,  SP,  BP,  SI,  DI,
   EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
   ES,  CS,  SS,  DS,  FS,  GS,

   /* x86-64 (jen v segmentech USE64) */
   RAX,  RCX,  RDX,  RBX,  RSP,  RBP,  RSI,  RDI,
   R8,   R9,   R10,  R11,  R12,  R13,  R14,  R15,
   R8D,  R9D,  R10D, R11D, R12D, R13D, R14D, R15D,
   R8W,  R9W,  R10W, R11W, R12W, R13W, R14W, R15W,
   R8B,  R9B,  R10B, R11B, R12B, R13B, R14B, R15B,
   SPL,  BPL,  SIL,  DIL,

   None
};

#ifndef __cplusplus
typedef enum Register Register;
#endif

/* jmena registru (def.c):  RegTable [RAX] = "RAX" */
extern const char RegTable [None + 1][4 + 1];

#endif
