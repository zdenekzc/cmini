
/* main-code.cc */

#include "str.h"
#include "ioexception.h"
#include "inf.h"
#include "opt.h"
#include "code.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

extern "C" int main (int argc, char * argv [])
{
   set_prog_name ("code");

   set_error_handlers ();

   enable_debug_messages ();
   enable_warnings ();

   enable_error_trace ();
   enable_abort_trace ();

   int err_code = 0;

   try
   {
      elem_init (); // init code librarry
      code_init ();

      Project * project = new Project;
      read_options (argc, argv, project, false); // last parameter - allow only subset of command line options

      // without -h and -c: <base>.hpp and <base>.cpp from the input name (list.t => list.hpp, list.cpp)
      if (project->header_name == "" && project->code_name == "")
      {
         string base = removePath (project->inp_name);
         string::size_type dot = base.rfind ('.');
         if (dot != string::npos && dot > 0)
            base = base.substr (0, dot);
         project->header_name = base + ".hpp";
         project->code_name = base + ".cpp";
      }

      Module * module = new Module;
      project->add (module);

      module->source_file_name = project->inp_name;
      if (project->header_name != "")
         module->header_name = addSubdir (project->output_directory, project->header_name);
      if (project->code_name != "")
         module->code_name = addSubdir (project->output_directory, project->code_name);

      EnvInput & io = project->input;
      io.open (module->source_file_name);

      io.getToken (); // read first token
      module->readSource (io);

      io.checkEnd ();
      io.close ();

      module->Compile ();

      if (module->header_name != "")
         module->writeInterface ();

      if (module->code_name != "")
         module->writeImplementation ();
   }
   catch (IOException & e)
   {
      show_error (e.getMessage (), e.getFilename (), e.getLine (), e.getColumn ());
      err_code = 1;
   }
   catch (std::exception & e)
   {
      show_error (e.what ());
      err_code = 1;
   }
   catch (...)
   {
      show_error ("unknown exception");
      err_code = 1;
   }

   if (err_code == 0)
   {
      show_info ("O.K.");
   }

   return err_code;
}

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE
