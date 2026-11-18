
/* load.cc */

#include "load.h"

#include "load-conf.h" // symbol USE_LTDL

#ifdef USE_LTDL
   #include <ltdl.h>
#endif

#ifdef USE_DL
   #include <dlfcn.h>
#endif

#include "inf.h" // function info

OPEN_NAMESPACE

#ifdef USE_LTDL

string load_and_run (string file_name, string func_name)
{
   static bool first = true;
   int code = 0;
   string result = "";

   if (first)
   {
      code = lt_dlinit ();
      first = false;
   }

   if (code == 0)
   {
      lt_dlhandle handle = lt_dlopenext (file_name.c_str ());

      if (handle != NULL)
      {
         lt_ptr proc = lt_dlsym (handle, func_name.c_str ());
         if (proc != NULL)
         {
            info ("LOADED using libtool: " + file_name);

            typedef void (* func_t) ();
            func_t func = (func_t) (proc);
            func ();
         }
         else
         {
            result = lt_dlerror ();
         }
      }
      else
      {
         result = lt_dlerror ();
      }
   }
   else
   {
      result = lt_dlerror ();
   }

   return result;
}

#endif

#ifdef USE_DL

string load_and_run (string file_name, string func_name)
{
   string result = "";

   void * handle = dlopen (file_name.c_str (), RTLD_LAZY | RTLD_GLOBAL);

   if (handle == NULL)
   {
      result = dlerror ();
   }
   else
   {
      void * proc = dlsym (handle, func_name.c_str ());
      if (proc == NULL)
      {
         result = dlerror ();
      }
      else
      {
         info ("LOADED " + file_name);
         typedef void (* func_t) ();
         func_t func = (func_t) (proc);
         func ();
      }
   }

   return result;
}

#endif

#ifndef USE_LTDL
   #ifndef USE_DL
   string load_and_run (string file_name, string func_name)
   {
      return ""; // !?
   }
   #endif
#endif

CLOSE_NAMESPACE

