
/* stencil.h */

#ifndef __PROG_STENCIL_H__
#define __PROG_STENCIL_H__

#include "code.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

/* stencils converted from ../src/cmm/comp_stencils.cc
   (sections BROWSE, TEXT IO and CALL METHODS) */

void classProperties (Class * cls);
void classSubitems (Class * cls);

Func * readText (Class * cls);
Func * writeText (Class * cls);
Func * callMethods (Class * cls);

void textIO (Module * module);

/* ---------------------------------------------------------------------- */

/* stencils converted from ../src/qview/stencils.cc (JsonIO)
   and ../src/stencils/xml.t (XmlIO) — dependent on Qt */

Func * readJson (Class * cls);
Func * writeJson (Class * cls);

Func * readXml (Class * cls);
Func * writeXml (Class * cls);

void jsonIO (Module * module);
void xmlIO (Module * module);

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_STENCIL_H__ */
