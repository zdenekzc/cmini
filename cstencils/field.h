
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

template <typename T>
class FieldTemplate : public Field
{
   private:
      T * pointer;

   public:
      virtual T getValue () /* new */
         { return * pointer; }

      virtual void setValue (T val) /* new */
         { * pointer = val; }

      virtual string getText ();

      virtual void setText (string txt);

      virtual T defaultValue (); /* new */

      virtual bool isDefault ()
         { return getValue () == defaultValue (); }

      void setPointer (T * p_pointer)
         { pointer = p_pointer; }

      void setVariable (T & p_variable)
         { pointer = & p_variable; }

      FieldTemplate () :
         pointer (NULL)
         { }
};

typedef FieldTemplate <bool>   BoolField;
typedef FieldTemplate <int>    IntField;
typedef FieldTemplate <string> StrField;

/* ---------------------------------------------------------------------- */

template <typename E>
class IndirectFieldTemplate : public Field
{
   private:
      typedef E * T;

      T * pointer;
      string text;

   public:
      virtual T getValue ()
         { return * pointer; }

      virtual void setValue (T val)
         { * pointer = val; }

      virtual string getText ()
         { return text; }

      virtual void setText (string val)
         { text = val; }

      virtual T defaultValue ()
         { return NULL; }

      virtual bool isDefault ()
         { return getValue () == defaultValue (); }

      void setPointer (T * p_pointer)
         { pointer = p_pointer; }

      void setVariable (T & p_variable)
         { pointer = & p_variable; }

      IndirectFieldTemplate () :
         pointer (NULL),
         text ()
         { }
};

typedef IndirectFieldTemplate <Element> ElementField;

/* ---------------------------------------------------------------------- */

template <typename F, typename T>
class FieldHolder : public F
{
   private:
      T variable;

   public:
      virtual T getValue ()
         { return variable; }

      virtual void setValue (T val)
         { variable = val; }

      FieldHolder ()
         { this->setVariable (variable); }
};

typedef FieldHolder <BoolField, bool>   BoolFieldHolder;
typedef FieldHolder <IntField,  int>    IntFieldHolder;
typedef FieldHolder <StrField,  string> StrFieldHolder;

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_FIELD_H__ */
