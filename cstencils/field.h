
/* field.h */

#ifndef __PROG_FIELD_H__
#define __PROG_FIELD_H__

#include "std.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

class Field;
class Element;

class EnvInput;
class EnvOutput;

#if 1
class Field_PropItem_Observer;
#endif

/* ---------------------------------------------------------------------- */

class Field
{
   private:
      string Name; // field identifier
   public:
      string getName () { return Name; }
      void setName (string value);

   private:
      string Alias; // alternate identifier
   public:
      string getAlias ();
      void setAlias (string value);

   private:
      string Note; // text for status line
   public:
      string getNote ();
      void setNote (string value);

   private:
      string Text; // text value
   public:
      virtual string getText ();
      virtual void setText (string value);

   // list of fields
   private:
      Field * prevField;
      Field * nextField;
      Element * upperElem;
   public:
      Field * getPrevField () { return prevField; }
      Field * getNextField () { return nextField; }
      Element * getUpperElem () { return upperElem; }
      void insertPrevField (Field * item);
      void insertNextField (Field * item);
      void removeField ();

   #if 1
   public:
      Field_PropItem_Observer * PropItem_Observer;
   #endif

   public:
      // input / output
      int OrigLine;
      int OrigColumn;

      bool user_value; // value from source file (used by mix.tt)

      // default value
      virtual bool isDefault () { return false; }

      // identifier for error messages
      virtual string getInfoName ();

      // input / output
      virtual void readField (EnvInput & io);
      virtual void writeField (EnvOutput & io);

      // initialization
      virtual void Prepare () { }

      // user interface
      virtual bool userAccessible () { return true; } // visible in property table

      // listbox
      virtual bool listEnabled () { return false; }
      virtual int listCount () { return 0; }
      virtual string listValue (int inx) { return ""; }
      virtual void listClick (int inx) { setText (listValue (inx)); }

      virtual bool dialogEnabled () { return false; }
      virtual void dialogExecute () { }

   public:
      Field ();
      virtual ~ Field ();

      friend class Element;
};

/* ---------------------------------------------------------------------- */

#if 1
class Field_PropItem_Observer
{
   public:
      virtual void propItem_OnInsertPrev (Field * new_data) = 0;
      virtual void propItem_OnRemove () = 0;
      virtual void propItem_NameUpdate () = 0;
      virtual void propItem_TextUpdate () = 0;

   public:
      Field_PropItem_Observer () { }
      virtual ~ Field_PropItem_Observer () { }
};
#endif

/* ---------------------------------------------------------------------- */

/* Field classes without templates (cmini has no templates): each macro
   declares one class for one value type; methods getText, setText and
   defaultValue of the value fields are defined in field.cc */

#define DECLARE_VALUE_FIELD(Name, T) \
class Name : public Field \
{ \
   private: \
      T * pointer; \
   public: \
      virtual T getValue () \
         { return * pointer; } \
      virtual void setValue (T val) \
         { * pointer = val; } \
      virtual string getText (); \
      virtual void setText (string txt); \
      virtual T defaultValue (); \
      virtual bool isDefault () \
         { return getValue () == defaultValue (); } \
      void setPointer (T * p_pointer) \
         { pointer = p_pointer; } \
      void setVariable (T & p_variable) \
         { pointer = & p_variable; } \
      Name () : \
         pointer (NULL) \
         { } \
};

DECLARE_VALUE_FIELD (BoolField, bool)
DECLARE_VALUE_FIELD (IntField,  int)
DECLARE_VALUE_FIELD (StrField,  string)

/* ---------------------------------------------------------------------- */

/* field of a pointer type (E * is stored in the field, text is kept as is) */

#define DECLARE_INDIRECT_FIELD(Name, E) \
class Name : public Field \
{ \
   private: \
      typedef E * T; \
      T * pointer; \
      string text; \
   public: \
      virtual T getValue () \
         { return * pointer; } \
      virtual void setValue (T val) \
         { * pointer = val; } \
      virtual string getText () \
         { return text; } \
      virtual void setText (string val) \
         { text = val; } \
      virtual T defaultValue () \
         { return NULL; } \
      virtual bool isDefault () \
         { return getValue () == defaultValue (); } \
      void setPointer (T * p_pointer) \
         { pointer = p_pointer; } \
      void setVariable (T & p_variable) \
         { pointer = & p_variable; } \
      Name () : \
         pointer (NULL), \
         text () \
         { } \
};

class Class;   /* forward declarations for the pointer fields (code.h) */
class Attr;
class List;

DECLARE_INDIRECT_FIELD (ElementField, Element)
DECLARE_INDIRECT_FIELD (IndirectField_Class, Class)
DECLARE_INDIRECT_FIELD (IndirectField_Attr,  Attr)
DECLARE_INDIRECT_FIELD (IndirectField_List,  List)

/* ---------------------------------------------------------------------- */

/* field that keeps its own value (variable) */

#define DECLARE_FIELD_HOLDER(Name, F, T) \
class Name : public F \
{ \
   private: \
      T variable; \
   public: \
      virtual T getValue () \
         { return variable; } \
      virtual void setValue (T val) \
         { variable = val; } \
      Name () \
         { this->setVariable (variable); } \
};

DECLARE_FIELD_HOLDER (BoolFieldHolder, BoolField, bool)
DECLARE_FIELD_HOLDER (IntFieldHolder,  IntField,  int)
DECLARE_FIELD_HOLDER (StrFieldHolder,  StrField,  string)

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_FIELD_H__ */
