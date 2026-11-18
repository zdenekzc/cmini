/* main-code.cc - translator list.t => list.hpp, list.cpp */

#include <getopt.h>
#include <stdlib.h>
#include <cstddef>
#include <iostream>

#include "str.h"
#include "ioexception.h"
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
   static struct option long_options [] =
   { {"header",     required_argument, NULL, 'h'},
     {"code",       required_argument, NULL, 'c'},
     {"output-dir", required_argument, NULL, 'o'},
     {"no-debug",   no_argument,       NULL, 'd'},
     {"no-warn",    no_argument,       NULL, 'w'},
     {"help",       no_argument,       NULL, 'H'},
     {NULL, 0, NULL, 0} };

   int c;
   while ((c = getopt_long (argc, argv, "h:c:", long_options, NULL)) != -1)
   {
      switch (c)
      {
         case 'h':
            if (prj->header_name != "")
               error ("Header filename already defined");
            prj->header_name = optarg;
            break;

         case 'c':
            if (prj->code_name != "")
               error ("Code filename already defined");
            prj->code_name = optarg;
            break;

         case 'o':
            if (prj->output_directory != "")
               error ("Output directory already defined");
            prj->output_directory = optarg;
            break;

         case 'd':
            disable_debug_messages ();
            break;

         case 'w':
            disable_warnings ();
            break;

         case 'H':
            usage ();
            exit (0);

         default:
            usage ();
            error ("Bad option");
      }
   }

   if (optind >= argc)
      error ("Missing input filename");

   if (optind < argc-1)
      error ("Too many input filenames");

   prj->inp_name = argv [optind];
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

   int err_code = 0;

   try
   {
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
