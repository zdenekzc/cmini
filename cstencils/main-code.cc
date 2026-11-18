/* main-code.cc - translator list.t => list.hpp, list.cpp */

#include <stdlib.h>
#include <iostream>

#include "str.h"
#include "inf.h"
#include "code.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

static void usage ()
{
   cerr << "Usage: " << get_prog_name () << " [options]... input-file" << endl;
   cerr << endl;
   cerr << "A program for translation of .t files to .hpp and .cpp" << endl;
   cerr << endl;
   cerr << "Options:" << endl;
   cerr << "  -h, --header FILENAME       header file name (default: input base name + .hpp)" << endl;
   cerr << "  -c, --code FILENAME         code file name (default: input base name + .cpp)" << endl;
   cerr << "  --output-dir DIR            output directory" << endl;
   cerr << "  --no-debug                  disable debug messages" << endl;
   cerr << "  --no-warn                   disable warnings" << endl;
   cerr << "  --help                      show this help" << endl;
   cerr << endl;
}

/* ---------------------------------------------------------------------- */

static void read_options (int argc, char ** argv, Project * prj)
{
   int i = 1;

   while (i < argc && argv [i][0] == '-' && argv [i][1] != '\0')
   {
      string opt = argv [i];

      if (opt == "--no-debug")
      {
         disable_debug_messages ();
         i++;
      }
      else if (opt == "--no-warn")
      {
         disable_warnings ();
         i++;
      }
      else if (opt == "--help")
      {
         usage ();
         exit (0);
      }
      else if (opt == "-h" || opt == "--header" ||
               opt == "-c" || opt == "--code" ||
               opt == "--output-dir")
      {
         if (i+1 >= argc)
            error ("Missing argument for " + opt);

         string val = argv [i+1];

         if (opt == "-h" || opt == "--header")
         {
            if (prj->header_name != "")
               error ("Header filename already defined");
            prj->header_name = val;
         }
         else if (opt == "-c" || opt == "--code")
         {
            if (prj->code_name != "")
               error ("Code filename already defined");
            prj->code_name = val;
         }
         else
         {
            if (prj->output_directory != "")
               error ("Output directory already defined");
            prj->output_directory = val;
         }
         i += 2;
      }
      else
      {
         usage ();
         error ("Bad option: " + opt);
      }
   }

   if (i >= argc)
      error ("Missing input filename");

   if (i < argc-1)
      error ("Too many input filenames");

   prj->inp_name = argv [i];
}

/* ---------------------------------------------------------------------- */

extern "C" int main (int argc, char * argv [])
{
   set_prog_name ("code");

   set_error_handlers ();

   enable_debug_messages ();
   enable_warnings ();

   enable_error_trace ();
   enable_abort_trace ();

   elem_init (); // init code librarry
   code_init ();

   Project * project = new Project;
   read_options (argc, argv, project);

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

   show_info ("O.K.");

   return 0;
}

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE
