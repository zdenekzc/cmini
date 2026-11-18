
/* opt.cc */

#include "opt.h"

#include "str.h" // function CharsToStr
#include "conv.h" // function StrToInt
#include "inf.h"
#include "ioexception.h"

#include "code.h"
#include "load.h"

#include <iostream>
#include <stdlib.h> // function getenv

#ifdef __linux__
   // !? ifdef HAVE_UNISTD_H
   #include <unistd.h> // variable environ
#endif

#ifdef __linux__
   // extern char * * environ; // !? Red Hat 9.0
#endif

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

#include "path-conf.h"

// use conditional symbols from following configuration files
#include "inf-conf.h"
#include "load-conf.h"

/* ---------------------------------------------------------------------- */

// global variables

// directories for load_modules
InclDirectories module_directories;

// list of loaded modules
StringCollection loaded_modules;

/* ---------------------------------------------------------------------- */

const bool local_data  = LOCAL_DATA_PARAM; // true => use not installed files
const bool local_rpath = LOCAL_RPATH_PARAM;

const string SetupFiles = SETUP_FILES;
const string SetupVariable = SETUP_VARIABLE;

/* ---------------------------------------------------------------------- */

const string BinDir  = removeLastSlash (BIN_DIR);
const string IncDir  = removeLastSlash (INCLUDE_DIR);
const string LibDir  = removeLastSlash (LIB_DIR);
const string DataDir = removeLastSlash (DATA_DIR);

const string SrcDir  = DataDir;

/* ---------------------------------------------------------------------- */

#define SETUP(id) prj->addVariable (#id, id);

void setupConfVariables (Project * prj)
{
   SETUP (PTHREAD_CFLAGS)
   SETUP (PTHREAD_LIBS)

   SETUP (TRACE_CFLAGS)
   SETUP (TRACE_LIBS)

   SETUP (DEMANGLE_CFLAGS)
   SETUP (DEMANGLE_LIBS)

   SETUP (BFD_CFLAGS)
   SETUP (BFD_LIBS)

   SETUP (LTDL_CFLAGS)
   SETUP (LTDL_LIBS)

   SETUP (DL_CFLAGS)
   SETUP (DL_LIBS)
}

string substituteConfVariables (Project * prj, string txt)
{
   //  variables = prj->variables;

   string result = "";
   bool cont = true;

   while (cont)
   {
      cont = false;

      string::size_type start = txt.find ("@{");
      if (start != string::npos )
      {
         string a = txt.substr (0, start); // string before "@{"
         string b = txt.substr (start);
         string::size_type stop = b.find ('}');
         if (stop != string::npos)
         {
            string name = b.substr (2, stop-2);
            string c = b.substr (stop+1); // string after "}"

            result = result + a + prj->recallVariable (name);
            txt = c;
            cont = true;
         }
      }
   }

   return result + txt;
}

/**************************** READ SETUP FILES ****************************/

void readOneSetupFile (Project * prj, string file_name)
{
   assert (prj != NULL);

   // TEnvInput io;
   // io.input.symbols.CopyFrom (prj->symbols);
   // io.incl_directories.CopyFrom (prj->input.incl_directories);

   EnvInput & io = prj->input;

   io.open (file_name);
   io.getToken ();

   while (! io.isEof ())
   {
      string name = io.readIdent ();

      if (io.isSeparator ('='))
      {
         string value = io.readRestOfLine ();
         value = trim (value);
         // debug ("set " + name + "=" + value);
         prj->addVariable (name, value);
         io.getToken ();
      }
      else
      {
         io.error ("Variable value expected");
      }

   }

   io.close ();
   // prj->input.used_files.CopyFrom (io.used_files);
}

#if 0
void read_instructions (Project * prj, const string txt, int inx)
{
   assert (prj != NULL);

   string instr = "";
   string name = "";
   string params = "";
   string value = "";

   int len = txt.length ();

   while (inx < len && txt [inx] <= ' ') inx ++; // skip spaces
   if (inx < len)
   {
      int start = inx;
      while (inx < len && isLetterOrDigit (txt [inx])) inx ++; // identifier
      instr = txt.substr (start, inx-start);
   }

   while (inx < len && txt [inx] <= ' ') inx ++; // skip spaces
   if (inx < len)
   {
      int start = inx;
      while (inx < len && isLetterOrDigit (txt [inx])) inx ++; // identifier
      name = txt.substr (start, inx-start);
   }

   if (inx < len && txt[inx] == '(')
   {
      inx ++; // skip parenthesis
      int start = inx;
      while (inx < len && txt [inx] != ')' ) inx ++; // paramaters
      if (inx < len && txt [inx] == ')' )
      {
         params = trim (txt.substr (start, inx-start));
         inx ++; // skip parenthesis
      }
   }

   if (inx < len)
     value = trim (txt.substr (inx));

   if (instr == "define")
   {
      // info ("define " + name + "(" + params + ")" + " " + value);
      prj->defineSymbolWithParams (name, params, value);
   }
   else if (instr == "undef")
   {
      // info ("undef " + name);
      prj->undefineSymbol (name);
   }
   else error ("Unknown directive in setup file: " + name);
}

void readOneSetupFile (Project * prj, string file_name)
{
   assert (prj != NULL);

   ifstream f;
   f.open (file_name.c_str ());
   check (f.good (), "Cannot open configuration file: " + file_name);
   info ("reading " + file_name);

   while (! f.eof ())
   {
      const int max = 256;
      char buf [max];
      f.getline (buf, max);
      string txt = string (buf);

      debug ("line: " + txt);
      txt = trim (txt);
      if (! hasPrefix (txt, "//") && txt != "") // !? comments
      {

         string::size_type pos = txt.find ('#');
         if (pos != string::npos)
         {
            read_instructions (prj, txt, pos+1); // one character after hash mark
            txt = txt.substr (0, pos); // remove comment, important for following code
         }

         pos = txt.find ('=');
         if (pos != 0 && pos != string::npos)
         {
            string name = trim (txt.substr (0, pos)); // remove spaces
            string value = trim (txt.substr (pos+1));
            if (value.length () > 0)
            {
               char start = value [0];
               if (start == quote1 || start == quote2)
                  value = removeQuotes (value, start); // remove C-style quotes
            }
            debug ("set " + name + "=" + value);
            prj->addVariable (name, value);
         }
      }
   }

   f.close ();
   debug ("closing " + file_name);
}
#endif

void readSetupFiles (Project * prj, string txt)
{
   int len = txt.length ();
   int inx = 0;

   while (inx < len)
   {
      int start = inx;
      while (inx < len && txt [inx] != ':') inx ++;
      if (inx != start)
      {
         string file_name = txt.substr (start, inx-start);
         // info ("trying " + file_name);

         ifstream f;
         f.open (file_name.c_str ());

         if (f.good ())
         {
            f.close ();
            readOneSetupFile (prj, file_name);
         }
      }
      inx ++; // skip colon
   }
}

/*************************** PLATFORM VARIABLES ***************************/

string platformVar (FolderType folder)
{
   string result = "";

   switch (folder)
   {
      case bin_folder:
         result = "FINCH_BIN_DIR";
         break;

      case inc_folder:
         result = "FINCH_INC_DIR";
         break;

      case lib_folder:
         result = "FINCH_LIB_DIR";
         break;

      case data_folder:
         result = "FINCH_DATA_DIR";
         break;

      case src_folder:
         result = "FINCH_SRC_DIR";
         break;

      default:
         assert (false);
   }

   return result;
}

/* ---------------------------------------------------------------------- */

string platformDir (FolderType folder)
{
   string result = "";

   switch (folder)
   {
      case bin_folder:
         result = BinDir;
         break;

      case inc_folder:
         result = IncDir;
         break;

      case lib_folder:
         result = LibDir;
         break;

      case data_folder:
         result = DataDir;
         break;

      case src_folder:
         result = SrcDir;
         break;

      default:
         assert (false);
   }

   return result;
}

/* ---------------------------------------------------------------------- */

string platformSubDir (FolderType folder, const string sub_dir)
{
   string result = sub_dir;

   if (folder == bin_folder)
   {
      if (! local_data)
         result = ""; // no subdirectories
   }

   if (folder == inc_folder)
   {
      if (! local_data)
      {
         result = removePrefix (result, "src/");
      }
   }

   if (folder == lib_folder)
   {
      if (local_data)
      {
         if (hasPrefix (result, "src/"))
            result = result + "/.libs";
            // kit(s) are without .libs subdirectory
      }
      else
      {
         result = ""; // no library subdirectories
      }
   }

   return result;
}

/* ---------------------------------------------------------------------- */

string platformRealDir (FolderType folder, const string sub_dir)
{
   return addSubdir (platformDir (folder),
                     platformSubDir (folder, sub_dir));
}

string platformMakeDir (FolderType folder, const string sub_dir)
{
   return addSubdir ("$(" + platformVar (folder) + ")",
                      platformSubDir (folder, sub_dir));
}

/**************************** LOADABLE MODULES ****************************/

void addModuleDirectory (string name)
// add directory with loadable modules
{
   // info ("MODULE DIRECTORY (" + name + ")");
   module_directories.addInclDir (name);
}

void registerModule (string name)
{
   loaded_modules.add (name);
}

void loadModule (string name)
{
   if (! loaded_modules.contains (name))
   {
      // info ("LOAD MODULE (" + name + ")");

      // string file_name = module_directories.LookupInclFile (".libs/lib" + name + ".la");
      // .la is working only with lt_dlopenext, not with dlopen function

      string file_name = module_directories.lookupInclFile (".libs/lib" + name + ".so");

      if (file_name == "")
         error ("Cannot find plugin module " + name);
      else
      {
         string func_name = modifyIdentifier (name) + "_init";
         // invalid characters -> underlines

         string result = load_and_run (file_name, func_name);

         if (result != "")
            error ("Cannot load plugin module " + file_name + " (" + result + ")");
         else
            registerModule (name);
      }
   }
}

/************************* ENVIRONMENT VARIABLES **************************/

string get_env_variable (string name)
{
   return charsToStr (getenv (name.c_str ())); // CharsToStr solves problems with NULL character pointer
}

static void add_one_variable (Project * prj, string name)
{
   prj->addVariable (name, get_env_variable (name));
}

static void add_all_env_variables (Project * prj)
{
   #ifdef __linux__
      int inx = 0;
      while (environ [inx] != NULL)
      {
         string txt = environ [inx];
         string::size_type pos = txt.find ('=');

         if (pos != 0 && pos != string::npos)
         {
            string name = txt.substr (0, pos);
            string value = txt.substr (pos+1);
            prj->addVariable (name, value);
         }

         inx ++;
      }
   #endif
}

/************************** COMMAND LINE OPTIONS **************************/

static void add_module_dirs (string txt)
{
   string::size_type pos = 0;
   string::size_type len = txt.length ();

   while (pos < len)
   {
      while (pos < len && txt[pos] == ':')
         pos ++;

      string::size_type inx = pos;

      while (inx < len && txt[inx] != ':')
         inx ++;

      if (inx > pos)
      {
         string s = txt.substr (pos, inx-pos);
         addModuleDirectory (s);
      }

      pos = inx;
   }
}

/* ---------------------------------------------------------------------- */

static void add_incl_dirs (Project * prj, string txt)
{
   string::size_type pos = 0;
   string::size_type len = txt.length ();

   while (pos < len)
   {
      while (pos < len && txt[pos] == ':')
         pos ++;

      string::size_type inx = pos;

      while (inx < len && txt[inx] != ':')
         inx ++;

      if (inx > pos)
      {
         string s = txt.substr (pos, inx-pos);
         prj->input.incl_directories.addInclDir (s);
      }

      pos = inx;
   }
}

/* ---------------------------------------------------------------------- */

/*
static void test_source (string file_name)
{
   struct stat buf;
   int code = stat (file_name.c_str (), & buf);
   if (code == 0)
      cout << "STAT " << file_name << " "
           << buf.st_mtim.tv_sec << " " << buf.st_mtim.tv_nsec << endl;
}
*/

/************************** COMMAND LINE OPTIONS **************************/

static void next_option (string & result, // out: value
                         int & i, // current argument index
                         int argc, // totat number of arguments
                         char ** argv, // list of arguments
                         bool inl = false) // extract value from current argument
{
   string inp = argv [i];

   if (inl)
   {
      // read text from this argument
      result = inp.substr (2); // skip two characters
   }
   else
   {
      bool ok = false;
      string txt = "";

      // read text from next argument
      if (i+1 < argc)
      {
         i ++;
         txt = argv [i];
         ok = (txt == "" || txt [0] != '-');
      }

      if (ok)
         result = txt;
      else
      {
         result = ""; // !?
         error ("Missing argument for " + inp + " option");
      }
   }
}

static void next_number (int & result, int & i, int argc, char ** argv)
{
   string txt;
   next_option (txt, i, argc, argv);

   int num;
   bool ok = StrToInt (txt, num);

   if (ok)
   {
      result = num;
      // info ("Text argument: " + txt);
      // info ("Numeric argument: " + IntToStr (num));
   }
   else
   {
      result = 0; // !?
      error ("Bad numeric argument: " + txt);
   }

}

static void next_value (string & name, string & value, bool & any_value,
                        int & i, int argc, char ** argv,
                        bool inl = false)
{
   string txt;
   next_option (txt, i, argc, argv, inl);

   string::size_type pos = txt.find ('=');

   if (pos == 0)
      error ("Strange argument: " + txt);

   if (pos != string::npos)
   {
      name = txt.substr (0, pos);
      value = txt.substr (pos+1);
      any_value = true;
   }
   else
   {
      name = txt;
      value = "";
      any_value = false;
   }
}

void check_options (Project * prj, string txt)
{
   if (prj->header_name != "")
      error ("Incompatible options " + txt + " and --header");

   if (prj->code_name != "")
      error ("Incompatible options " + txt + " and --code");

   if (prj->compile_project)
       error ("Incompatible options " + txt + " and --project");
}

/* ---------------------------------------------------------------------- */

static void usage (bool complete)
{
   cerr << "Usage: " << get_prog_name () << " [options]... input-file" << endl;
   cerr << endl;

   cerr << "A program for translation of .t files to .hpp, .ipp, ..." << endl;
   cerr << endl;

   cerr << "Options:" << endl;
   cerr << "  -h, --header FILENAME       header file name (default: input base name + .hpp)" << endl;
   cerr << "  -c, --code FILENAME         code file name (default: input base name + .cpp)" << endl;

   if (complete)
   {
   cerr << "  --desc-header FILENAME      description header file name" << endl;
   cerr << "  --desc-code FILENAME        description code file name" << endl;
   cerr << "  --project                   translate all files in project" << endl;
   cerr << "  --makefile                  only generate makefile" << endl;
   cerr << "  --touch                     always modify code file time" << endl;
   }

   cerr << "  --output-dir DIR            output directory" << endl;
   cerr << "  --no-debug                  disable debug messages" << endl;
   cerr << "  --no-warn                   disable warnings" << endl;

   if (complete)
   {
   cerr << "  --alt-list                  list alternatives" << endl;
   cerr << "  --alt-count                 count alternatives" << endl;
   cerr << "  --alt-select NUMBER         selected alternative" << endl;
   cerr << "  --gtk1 --gtk2 --gtk3        select GTK alternative" << endl;
   cerr << "  --qt3 --qt4 --qt5           select QT alternative" << endl;
   }

   cerr << "  -D, --define NAME[=VALUE]   define conditional symbol" << endl;
   cerr << "  -I, --include-dir DIR       include directory for source files" << endl;
   cerr << "  -V, --variable NAME[=VALUE] add one environment variable as conditional symbol" << endl;
   cerr << "  --env                       add all environment variables" << endl;
   cerr << "  --conf FILE                 read configuration file with variables" << endl;
   cerr << "  -M, --load-module FILE      load extension module" << endl;
   cerr << "  -S, --search-dir DIR        directory with extension modules" << endl;
   cerr << endl;
}

/* ---------------------------------------------------------------------- */

void read_options (int argc, char ** argv, Project * prj, bool complete)
{
   assert (prj != NULL);

   try
   {
      // prj->AddVariable ("FINCH_BIN_DIR",  PlatformDir (bin_folder));
      // prj->AddVariable ("FINCH_INC_DIR",  PlatformDir (inc_folder));
      // prj->AddVariable ("FINCH_LIB_DIR",  PlatformDir (lib_folder));
      // prj->AddVariable ("FINCH_DATA_DIR", PlatformDir (data_folder));

      // prj->AddVariable ("FINCH_CONFIG_DIR", PlatformRealDir (inc_folder, "config"));
      // prj->AddVariable ("FINCH_SRC_DIR", PlatformDir (src_folder));

      // prj->AddIncludeDirectory ("$(FINCH_DATA_DIR)/src/data");
      prj->input.incl_directories.addInclDir (platformRealDir (inc_folder, "src/data")); // !?

      setupConfVariables (prj); // store values from path-conf.h

      readSetupFiles (prj, SetupFiles);

      if (SetupVariable != "")
         readSetupFiles (prj, get_env_variable (SetupVariable));

      int i = 1;
      while (i < argc)
      {
         string txt = argv [i];

         char mark = ' ';
         if (txt.length () > 2 && txt [0] == '-') // minus, letter and other characters
            mark = txt [1];

         string val = "";

         if (txt[0] != '-')
         {
            if (prj->inp_name != "")
               error ("Input file name already defined");
             prj->inp_name = txt;
         }

         else if (txt == "-h" || txt == "--header")
         {
            if (prj->header_name != "")
               error ("Header filename already defined");
            next_option (val, i, argc, argv); // read next option
            prj->header_name = val;
         }

         else if (txt == "-c" || txt == "--code")
         {
            if (prj->code_name != "")
               error ("Code filename already defined");
            next_option (val, i, argc, argv);
            prj->code_name = val;
         }

         else if (txt == "--desc-header" && complete)
         {
            if (prj->desc_header_name != "")
               error ("Description header filename already defined");
            next_option (val, i, argc, argv);
            prj->desc_header_name = val;
         }

         else if (txt == "--desc-code" && complete)
         {
            if (prj->desc_code_name != "")
               error ("Description code filename already defined");
            next_option (val, i, argc, argv);
            prj->desc_code_name = val;
         }

         else if (txt == "--project" && complete)
         {
            prj->compile_project = true;
         }

         else if (txt == "--makefile" && complete)
         {
            prj->only_makefile = true;
         }

         else if (txt == "--output-dir")
         {
            if (prj->output_directory != "")
               error ("Output directory already defined");
            next_option (val, i, argc, argv);
            prj->output_directory = val;
         }

         else if (txt == "--no-debug")
         {
            disable_debug_messages ();
         }

         else if (txt == "--no-warn")
         {
            disable_warnings ();
         }

         else if (txt == "--alt-list" && complete)
         {
            prj->list_alternatives = true;
         }

         else if (txt == "--alt-count" && complete)
         {
            prj->count_alternatives = true;
         }

         else if ((txt == "--alt-select" || txt == "--alt") && complete) // !?
         {
            int num;
            next_number (num, i, argc, argv);
            prj->selected_alternative = num;
            prj->select_alternative = true;
            info ("selected_alternative " + IntToStr (prj->selected_alternative));
         }

         else if (txt == "--gtk1" && complete)
         {
            prj->defineSymbol ("GTK1", "");
         }

         else if (txt == "--gtk2" && complete)
         {
            prj->defineSymbol ("GTK2", "");
         }

         else if (txt == "--gtk3" && complete)
         {
            prj->defineSymbol ("GTK3", "");
         }

         else if (txt == "--qt3" && complete)
         {
            prj->defineSymbol ("QT3", "");
         }

         else if (txt == "--qt4" && complete)
         {
            prj->defineSymbol ("QT4", "");
         }

         else if (txt == "--qt5" && complete)
         {
            prj->defineSymbol ("QT5", "");
         }

         else if (mark == 'D' || txt == "-D" || txt == "--define")
         {
            string name;
            string value;
            bool any_value;
            next_value (name, value, any_value, i, argc, argv, mark == 'D');
            if (any_value)
               prj->defineSymbol (name, value);
            else
               prj->defineSymbol (name, "");
         }

         else if (mark == 'I' || txt == "-I" || txt == "--include-dir")
         {
            next_option (val, i, argc, argv, mark == 'I');
            add_incl_dirs (prj, val);
         }

         else if (mark == 'M' || txt == "-M"  || txt == "--load-module")
         {
            next_option (val, i, argc, argv, mark == 'M');
            loadModule (val);
         }

         else if (mark == 'S' || txt == "-S"  || txt == "--search-dir" || txt == "--module-dir") // !?
         {
            next_option (val, i, argc, argv,  mark == 'S');
            add_module_dirs (val);
         }

         else if (mark == 'V' || txt == "-V"  || txt == "--variable" || txt == "--var") // !?
         {
            string name;
            string value;
            bool any_value;
            next_value (name, value, any_value, i, argc, argv, mark == 'D');
            if (any_value)
               prj->addVariable (name, value);
            else
               add_one_variable (prj, name);
         }

         else if (txt == "--env")
         {
            add_all_env_variables (prj);
         }

         else if (txt == "--conf")
         {
            next_option (val, i, argc, argv);
            readOneSetupFile (prj, val);
         }

         else if (txt == "--help")
         {
            usage (complete);
         }

         else
         {
            usage (complete);
            error ("Unknown option: " + txt);
         }

         i ++; // next argument
      }

      if (prj->inp_name == "")
         error ("Missing input filename");

      if (complete)
      {
         if (prj->list_alternatives)
         {
            check_options (prj, "--alt-list");

            if (prj->select_alternative)
                error ("Incompatible options --alt-select and --alt-list");

            if (prj->only_makefile)
                error ("Incompatible options --makefile and --alt-list");
         }
         else if (prj->count_alternatives)
         {
            check_options (prj, "--alt-count");

            if (prj->select_alternative)
                error ("Incompatible options --alt-select and --alt-count");

            if (prj->only_makefile)
                error ("Incompatible options --makefile and --alt-count");
         }
         else if (prj->only_makefile)
         {
            check_options (prj, "--makefile");
         }
         else if (prj->compile_project)
         {
            if (prj->header_name != "")
               error ("Incompatible options --project and --header");

            if (prj->code_name != "")
               error ("Incompatible options --project and --code");
         }
         else if (prj->header_name != "" || prj->code_name != "")
         {
            prj->compile_one_module = true;
         }
         else
         {
            prj->compile_project = true;
         }

         if (prj->compile_one_module)
         {
            if (prj->header_name == "")
               error ("Missing header filename");

            if (prj->code_name == "")
               error ("Missing code filename");
         }
      }

      /* program info */

      std::cerr << get_prog_name () << ": " << prj->inp_name; // !?

      if (! prj->compile_project)
         std::cerr << " -> " << prj->header_name << ", " << prj->code_name;

      std::cerr << std::endl;
   }
   catch (IOException & e)
   {
      show_error (e.getMessage (), e.getFilename (), e.getLine (), e.getColumn ());
      exit (1);
   }
   catch (std::exception & e)
   {
      show_error (e.what ());
      exit (1);
   }
   catch (...)
   {
      show_error ("unknown exception");
      exit (1);
   }
}

/* ---------------------------------------------------------------------- */

/*
#include <getopt.h> // function getopt_long

#ifdef GCC2
   #define STD(id) id
#else
   #define STD(id) std::id
#endif

   int option_index = 0;

   char * short_options = "h:c:pm"; // h and c option with argument

   STD (option) long_options [] =
   { {"header",   required_argument, NULL, 'h'},
     {"code",     required_argument, NULL, 'c'},
     {"project",  no_argument,       NULL, 'p'},
     {"makefile", no_argument,       NULL, 'm'},
     { 0, 0, 0, 0} } ;

   bool cont = true;
   while (cont)
   {
      int c = getopt_long (argc, argv,
                           short_options, long_options,
                           & option_index);

      switch (c)
      {
         case 'h':
            if (header_name != "")
               error ("Header filename already defined");
            header_name = STD (optarg);
            break;

         case 'c':
            if (code_name != "")
               error ("Code filename already defined");
            code_name = STD (optarg);
            break;

         case 'p':
            compile_project = true;
            break;

         case 'm':
            only_makefile = true;
            break;

         case -1:
            cont = false;
            break;
      }
   }

   if (STD (optind) > argc)
      error ("Missing input filenames");

   if (STD (optind) < argc-1)
      error ("Too many input filenames");

   inp_name = argv [STD (optind)];
*/

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE
