
/* field.cc */

#include "field.h"
#include "elem.h"

#include "str.h"
#include "conv.h" // IntToStr, StrToInt
#include "code.h" // ReadNormalField

OPEN_NAMESPACE

/* TField */

Field::Field ():
   Name (),
   Alias (),
   Note (),
   Text (),
   prevField (NULL),
   nextField (NULL),
   upperElem (NULL),
   PropItem_Observer (NULL),
   OrigLine (0),
   OrigColumn (0),
   user_value (false)
{ }

Field::~Field ()
{
   if (upperElem != NULL)
      removeField ();

   #if 1
   if (PropItem_Observer != NULL)
   {
      delete PropItem_Observer;
      assert (PropItem_Observer == NULL);
   }
   #endif
}

/* TField */

void Field::setName (string p_value)
{
   if (p_value != Name)
   {
      Name = p_value;
      #if 1
      if (PropItem_Observer != NULL)
         PropItem_Observer->propItem_NameUpdate ();
      #endif
   }
}

void Field::setAlias (string p_value)
{
   if (p_value != Alias)
   {
      Alias = p_value;
   }
}

void Field::setNote (string p_value)
{
   if (p_value != Note)
   {
      Note = p_value;
   }
}

#if 0
void Field::updateText (string p_value)
{
   if (PropItem_Observer != NULL)
      PropItem_Observer->propItem_TextUpdate ();
}
#endif

void Field::insertPrevField (Field * item)
{
   assert (this != NULL);
   assert (upperElem != NULL);
   upperElem->linkField (item, prevField, this);
}

void Field::insertNextField (Field * item)
{
   assert (this != NULL);
   assert (upperElem != NULL);
   upperElem->linkField (item, this, nextField);
}

void Field::removeField ()
{
   assert (this != NULL);
   assert (upperElem != NULL);

   #if 1
   if (PropItem_Observer != NULL)
      PropItem_Observer->propItem_OnRemove ();
   #endif

   if (prevField == NULL)
      upperElem->firstField = nextField;
   else
      prevField->nextField = nextField;

   if (nextField == NULL)
      upperElem->lastField = prevField;
   else
      nextField->prevField = prevField;

   prevField = NULL;
   nextField = NULL;
   upperElem = NULL;
}

/********************************* FIELD **********************************/

string Field::getText ()
{
   // assert (false); // abstract getText
   return "???";
}

void Field::setText (string txt)
{
   assert (false); // abstract setText
}

/***************************** BOOLEAN FIELD ******************************/

string BoolField::getText ()
{
   return BoolToStr (getValue ());
}

void BoolField::setText (string txt)
{
   bool result;
   bool ok;

   StrToBool (txt, result, ok);

   check_field (ok, "Invalid boolean field value", this);
   if (ok)
      setValue (result);
}

bool BoolField::defaultValue ()
{
   return false;
}

/***************************** INTEGER FIELD ******************************/

string IntField::getText ()
{
   return IntToStr (getValue ());
}

void IntField::setText (string txt)
{
   int result;
   bool ok;

   StrToInt (txt, result, ok);

   check_field (ok, "Invalid integer field value", this);
   if (ok)
      setValue (result);
}

int IntField::defaultValue ()
{
   return 0;
}

/****************************** STRING FIELD ******************************/

string StrField::getText ()
{
   return getValue ();
}

void StrField::setText (string txt)
{
   setValue (txt);
}

string StrField::defaultValue ()
{
   return "";
}

CLOSE_NAMESPACE
