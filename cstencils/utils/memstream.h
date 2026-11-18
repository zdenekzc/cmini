
/* memstream.h */

#ifndef __PROG_MEM_STREAM_H__
#define __PROG_MEM_STREAM_H__

#include "std.h"
#include <sstream>
#include <iostream> // MANDRAKE 9.0

OPEN_NAMESPACE

class MemStream : public ostringstream
{
   protected:
      string file_name;
      bool show_msg;

      void write_text ();

   public:
      void open (string p_file_name);

      void enableOutputMessages (bool value); // enable/disable information output

      string get_file_name ();
      string get_text ();

      bool read_from (string source_file_name);

      void store (); // write if file is not equal
      void store_always (); // write text to file

      MemStream ();
      virtual ~ MemStream () { }
};

CLOSE_NAMESPACE

#endif /* __PROG_MEM_STREAM_H__ */
