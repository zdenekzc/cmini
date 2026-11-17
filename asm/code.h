/* CODE.H */

/* preklad a vystupni soubory (prevedeno z code.pas) */

#ifndef CODE_H
#define CODE_H

#include "key.h"
#include "def.h"
#include "expr.h"
#include "oth.h"
#include "inst.h"

void Compile (void);

/* staticky linker ELF64 pro ToaruOS (PasmLink, viz LINKER v code.c) */
int  LinkToaru (const char * out, const char * const * inputs, int count, char * msg, int msg_size);

#endif /* CODE_H */
