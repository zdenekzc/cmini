
/* memext.cc - memory stream extension */

#include "memext.h"

// #include "ioexception.h"
#include "inf.h" // function info
#include "str.h" // function extractPath

#include <stdlib.h> // function system

#ifdef __linux__
   // !? ifdef HAVE_SYS_STAT_H
   #include <sys/stat.h>
#endif

#ifdef __linux__
   // !? ifdef HAVE_UNISTD_H
   #include <unistd.h>
#endif

#include <fstream>

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

void MemStreamExt::set_executable ()
{
   #ifdef __linux__
      struct stat buf;
      int code = stat (file_name.c_str (), & buf);
      if (code == 0)
         chmod (file_name.c_str (), buf.st_mode | S_IXUSR);
   #endif
}

/* ---------------------------------------------------------------------- */

static bool FileExists (string file_name)
{
   ifstream f;

   f.open (file_name.c_str ());
   bool found = f.good ();
   f.close ();

   return found;
}

void MemStreamExt::make_directory ()
{
   #ifdef __linux__
      string dir_name = extractPath (file_name);
      if (dir_name != "")
      {
         if (! FileExists (dir_name + "/."))
         {
            if (show_msg)
               info ("creating " + dir_name);
            string cmd = "mkdir -p " + dir_name;
            int code = system (cmd.c_str ());
            if (code != 0)
               error ("Error creating directory: " + dir_name);
         }
      }
   #endif
}

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE
