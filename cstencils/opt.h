
/* opt.h */

#ifndef __PROG_OPT_H__
#define __PROG_OPT_H__

#include "std.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

/* command line options */

class Project;

void read_options (int argc, char ** argv, Project * prj, bool complete = true);

/* ---------------------------------------------------------------------- */

string substituteConfVariables (Project * prj, string values); // substitute @{name}

/* ---------------------------------------------------------------------- */

/* directories from configure */

enum FolderType
{
   bin_folder,
   inc_folder,
   lib_folder,
   data_folder,

   src_folder // now equivalent to data_folder
};

string platformVar (FolderType folder); // variable identifier
string platformDir (FolderType folder); // variable value

string platformRealDir (FolderType folder, const string sub_dir);
// real path

string platformMakeDir (FolderType folder, const string sub_dir);
// expression used in makefiles

/* ---------------------------------------------------------------------- */

/* loadable modules */

void addModuleDirectory (string name); // store search directory for loadable modules
void registerModule (string name);
void loadModule (string name);

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_OPT_H__ */
