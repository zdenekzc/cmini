/* CODE.H */

/* translation and output files (translated from code.pas) */

#ifndef CODE_H
#define CODE_H

#include "key.h"
#include "def.h"
#include "expr.h"
#include "oth.h"
#include "inst.h"

void Compile (void);

/* static linker ELF64 for ToaruOS (PasmLink, see LINKER in code.c) */
int  LinkToaru (const char * out, const char * const * inputs, int count, char * msg, int msg_size);

#endif /* CODE_H */
