
/* elem.h */

#ifndef __PROG_ELEM_H__
#define __PROG_ELEM_H__

#include "fact.h"
#include "access.h"
#include "inf.h" // info, warning, error, check
#include "envio.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

class Factory;
class Element;
class Field;

class EnvInput;
class EnvOutput;

class Project;
class Module;
class Class;
class Attr;
class List;
class EnumType;
class EnumItem;
class Expr;
class ExprList;
class Stat;
class StatList;
class ParamDecl;
class ParamList;
class SimpleDecl;
class Var;
class Func;

#if 1
class Element_Tree_Observer;
class Element_PropTable_Observer;
#endif

/* ---------------------------------------------------------------------- */

class Element
{
   public:
      string Name;
      string Comment;
      Access access;
      signed char Group;

   #if 1
   // attribute Title
   private:
      string Title;
   public:
      string getTitle () { return Title; }
      void setTitle (string p_value);

   // attribute Image, icon
   private:
      string Image;
   public:
      string getImage () { return Image; }
      void setImage (string p_value);

   // attribute SelectedImage, icon for opened tree branch
   private:
      string SelectedImage;
   public:
      string getSelectedImage () { return SelectedImage; }
      void setSelectedImage (string p_value);

   // attribute Factory
   private:
      Factory * factory;
   public:
      Factory * getFactory () { return factory; }
      void setFactory (Factory * p_value);
   #endif

   protected:
      Field * Name_Field;
   #if 0
      Field * Comment_Field;
      // TField * Group_Field;
      // TField * Access_Field;
   #endif

   #if 0
      // TField * Title_Field;
      Field * Image_Field;
      Field * SelectedImage_Field;
      // TField * Factory_Field;
   #endif

   // list of elements
   private:
      Element * firstElem;
      Element * lastElem;
      Element * prevElem;
      Element * nextElem;
      Element * upperElem;

   protected:
      virtual void linkElem (Element * item, Element * bef, Element * aft);

   public:
      Element * getFirstElem () { return firstElem; }
      Element * getLastElem () { return lastElem; }
      Element * getPrevElem () { return prevElem; }
      Element * getNextElem () { return nextElem; }
      Element * getUpperElem () { return upperElem; }

      void insertFirstElem (Element * item);
      void insertLastElem (Element * item);
      void insertPrevElem (Element * item);
      void insertNextElem (Element * item);

      virtual void removeElem (); // virtual is neccessary
      void removeAllElements ();

   #if 1
   // list of fields
   private:
      Field * firstField;
      Field * lastField;

   protected:
      void linkField (Field * item, Field * bef, Field * aft);

   public:
      Field * getFirstField () { return firstField; }
      Field * getLastField () { return lastField; }

      void insertFirstField (Field * item);
      void insertLastField (Field * item);

      void removeAllFields ();
   #endif

   public:
      // type info
      virtual string getTypeName () { return "Element"; }

   public:
      void add (Element * item) { insertLastElem (item); }
      Element * searchElement (string id);

   public:
      void sortItems ();

   public:
      void put (string txt);
      void putLn (string txt);

      void emptyLine ();
      void noEmptyLine ();

      void indent ();
      void unindent ();

      void incIndent () { indent (); }
      void decIndent () { unindent (); }

   public:
      void Location (StatList * & place);
      void addStatList (StatList * list);

   public:
      virtual bool isEmpty () { return false; }
      virtual bool isIfStat () { return false; }

      virtual void writePrefix (TextOutput & io) { }

      virtual bool hasInit () { return false; }
      virtual void writeInit (TextOutput & io) { }
      virtual void writeInitMembers (TextOutput & io) { }

      virtual void writeCode (TextOutput & io);
      virtual void writeIntf (TextOutput & io);
      virtual void writeImpl (TextOutput & io);

   public:
      virtual Project * toProject () { return NULL; }
      virtual Module * toModule () { return NULL; }
      virtual Class * toClass () { return NULL; }
      virtual EnumType * toEnumType () { return NULL; }
      virtual EnumItem * toEnumItem () { return NULL; }
      virtual Attr * toAttr () { return NULL; }
      virtual List * toList () { return NULL; }
      virtual ParamDecl * toParamDecl () { return NULL; }
      virtual SimpleDecl * toSimpleDecl () { return NULL; }
      virtual Var * toVar () { return NULL; }
      virtual Func * toFunc () { return NULL; }
      // virtual TSignal * toSignal () { return NULL; }
      // virtual TSlot * toSlot () { return NULL; }

   public:
      // virtual TFactory * getFactory ();

   #if 1
   public:
      Element_Tree_Observer * Tree_Observer;
      Element_PropTable_Observer * PropTable_Observer;
   #endif

   #if 1

   private:
      bool fields_created;
   protected:
      virtual void createLocalFields ();
   public:
      void createFields ();
      virtual bool readField (EnvInput & io);

   public:
      bool readInnerField (EnvInput & io);
      bool readInnerElement (EnvInput & io);

   public:
      bool skip_name;

   public:

      // identifier for error messages

      virtual string getInfoName ();

      // elements

      void addElem (Element * item);

      // fields

      Field * searchField (string p_name);
      Field * findField (string p_name);
      Field * createField (string p_name);

      // input / output

      Field * TypeParam; // parameter in < >
      Field * NameParam; // parameter after type name and < >
      Field * FromParam; // parameter after colon

      string OrigFileName; // !?
      int OrigLine;
      int OrigColumn;

      int StartLine;
      int StartColumn;

      int StopLine;
      int StopColumn;

      bool imported;
      bool ignore_subelements; // used in TMacro

      // %%
      string add_comment_before_forward_decl;
      bool add_space_after_forward_decl;

      // %%
      string variety_symbol; // conditional symbol for this declaration

      void setOrigin (EnvInput & io);
      void setStart (EnvInput & io);
      void setStop (EnvInput & io);

      virtual void readElement (EnvInput & io);
      virtual void writeElement (EnvOutput & io);

      virtual void readSubitem (EnvInput & io);

      virtual void onInsert () { }
      virtual void onRemove () { }

      virtual void connectSubitem (Element * item) { }
      virtual void disconnectSubitem (Element * item) { }

      virtual void Initialize () { }

      #if 1
      // user interface

      virtual void showDemo () { }
      virtual void showHelp () { }

      virtual void doubleClick () { showDemo (); }
      #endif

      // write program

      virtual void Prepare ();  // convert some fields
      virtual void basicProg () { }
      virtual void Prog () { }
      virtual void updateProg () { }
      virtual void completeProg () { }
      virtual void writeSource () { }

      void recursivePrepare ();
      void recursiveBasicProg ();
      void recursiveProg ();
      void recursiveUpdateProg ();
      void recursiveCompleteProg ();
      void recursiveWriteSource ();

      void Compile ();
   #endif

   public:
      Element ();
      virtual ~ Element ();

      friend class Field;
};

/* ---------------------------------------------------------------------- */

#if 1
class Element_Tree_Observer
{
   public:
      virtual void Tree_OnInsertPrev (Element * new_data) = 0;
      virtual void Tree_OnInsertLast (Element * new_data) = 0;
      virtual void Tree_OnRemove () = 0;
      virtual void Tree_TitleUpdate () = 0;

   public:
      Element_Tree_Observer () { }
      virtual ~ Element_Tree_Observer () { }
};

class Element_PropTable_Observer
{
   public:
      virtual void propTable_Disconnect () = 0;
      virtual void propItem_OnInsertLast (Field * new_data) = 0;

   public:
      Element_PropTable_Observer () { }
      virtual ~ Element_PropTable_Observer () { }
};
#endif

/* ---------------------------------------------------------------------- */

class Element_Factory : public Factory
{
   protected:
      virtual Element * newElement ();

   public:
      Element_Factory ();
};

/* ---------------------------------------------------------------------- */

void copyFields (Element * target, Element * source, bool include_name);
void copyElement (Element * target, Element * source); // copy element as target subelemets
void copyElements (Element * target, Element * source); // copy all subelements from source to target

/* ---------------------------------------------------------------------- */

#if 1
Element * lookupElement (Element * start, const string id);
Element * lookupElement (Field * start, const string id);
#endif

/* ---------------------------------------------------------------------- */

void info (string msg, Element * elem);
void info (string msg, Field * field);

void warning (string msg, Element * elem);
void warning (string msg, Field * field);

void error (string msg, Element * elem);
void error (string msg, Field * field);

void check (bool cond, string msg, Element * elem);
void check (bool cond, string msg, Field * field);

/* ---------------------------------------------------------------------- */

// extern TFactory * element_factory;

void elem_init ();

CLOSE_NAMESPACE

#endif /* __PROG_ELEM_H__ */
