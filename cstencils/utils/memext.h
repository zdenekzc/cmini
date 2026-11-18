
/* memext.h - memory stream extension */

#ifndef __PROG_MEM_EXT_H__
#define __PROG_MEM_EXT_H__

#include "memstream.h"

OPEN_NAMESPACE

class MemStreamExt : public MemStream
{
   public:
      void set_executable (); // set executable permission
      void make_directory (); // create directory if not exists
};

CLOSE_NAMESPACE

#endif /* __PROG_MEM_EXT_H__ */
