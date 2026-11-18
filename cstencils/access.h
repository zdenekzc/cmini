
/* access.h */

#ifndef __PROG_ACCESS_H__
#define __PROG_ACCESS_H__

#include "std.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

enum Access
{
   NoAccess,
   PrivateAccess,
   ProtectedAccess,
   PublicAccess,
   OnlyComment
};

typedef Access CmmAccess; // type alias for cmm.g

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_ACCESS_H__ */
