
/* elem.cc */

#include "elem.h"

#include "field.h"
#include "groups.h"
#include "inf.h" /* func check */
#include "str.h"

#include "elem-impl.cc"

OPEN_NAMESPACE

/* Element_Factory */

Element * Element_Factory::newElement ()
{
   return new Element ();
}

Element_Factory::Element_Factory ()
{
   setName ("Element");
}

/* elem_init */

void elem_init ()
{
   registerFactory (new Element_Factory);
}

/******************************** ELEMENT *********************************/

// TFactory * element_factory = NULL;

// TFactory * TElement::getFactory () { return element_factory; }

Element::~Element ()
{
   if (upperElem != NULL)
      removeElem ();

   removeAllElements ();
   removeAllFields ();

   #if 1
   if (Tree_Observer != NULL)
   {
      delete Tree_Observer;
      assert (Tree_Observer == NULL);
   }

   if (PropTable_Observer != NULL)
   {
      PropTable_Observer->propTable_Disconnect ();
      assert (PropTable_Observer == NULL);
   }
   #endif
}

/* ---------------------------------------------------------------------- */

#if 1
void Element::setTitle (string p_value)
{
   if (p_value != Title)
   {
      Title = p_value;
      #if 1
      if (Tree_Observer != NULL)
         Tree_Observer->Tree_TitleUpdate ();
      #endif
   }
}

void Element::setImage (string p_value)
{
   if (p_value != Image)
   {
      Image = p_value;
   }
}

void Element::setSelectedImage (string p_value)
{
   if (p_value != SelectedImage)
   {
      SelectedImage = p_value;
   }
}

void Element::setFactory (Factory * p_value)
{
   if (p_value != factory)
   {
      factory = p_value;
   }
}
#endif

/* ---------------------------------------------------------------------- */

void Element::linkField (Field * item, Field * bef, Field * aft)
{
   assert (this != NULL);
   assert (item != NULL);

   assert (item->prevField == NULL);
   assert (item->nextField == NULL);
   assert (item->upperElem == NULL);

   item->prevField = bef;
   item->nextField = aft;
   item->upperElem = this;

   if (bef == NULL)
      this->firstField = item;
   else
      bef->nextField = item;

   if (aft == NULL)
      this->lastField = item;
   else
      aft->prevField = item;

   #if 1
   if (aft == NULL)
   {
      if (PropTable_Observer != NULL)
         PropTable_Observer->propItem_OnInsertLast (item);
   }
   else
   {
      if (aft->PropItem_Observer != NULL)
         aft->PropItem_Observer->propItem_OnInsertPrev (item);
   }
   #endif
}

void Element::insertFirstField (Field * item)
{
   assert (this != NULL);
   linkField (item, NULL, firstField);
}

void Element::insertLastField (Field * item)
{
   assert (this != NULL);
   linkField (item, lastField, NULL);
}

void Element::removeAllFields ()
{
   Field * p = firstField;
   while (p != NULL)
   {
      Field * t = p;
      p = p->nextField;
      t->removeField ();
      delete t;
   }
   firstField = NULL;
   lastField = NULL;
}

/* ---------------------------------------------------------------------- */

void Element::linkElem (Element * item, Element * bef, Element * aft)
{
   assert (this != NULL);
   assert (item != NULL);

   assert (item->prevElem == NULL);
   assert (item->nextElem == NULL);
   assert (item->upperElem == NULL);

   Element * tmp = this;
   while (tmp != NULL)
   {
      assert (tmp != item);
      tmp = tmp->upperElem;
   }

   item->prevElem = bef;
   item->nextElem = aft;
   item->upperElem = this;

   if (bef == NULL)
      this->firstElem = item;
   else
      bef->nextElem = item;

   if (aft == NULL)
      this->lastElem = item;
   else
      aft->prevElem = item;

   #if 1
   item->onInsert ();
   connectSubitem (item);

   if (aft == NULL)
   {
      if (Tree_Observer != NULL)
         Tree_Observer->Tree_OnInsertLast (item);
   }
   else
   {
      if (aft->Tree_Observer != NULL)
         aft->Tree_Observer->Tree_OnInsertPrev (item);
   }
   #endif
}

void Element::insertFirstElem (Element * item)
{
   assert (this != NULL);
   linkElem (item, NULL, firstElem);
}

void Element::insertLastElem (Element * item)
{
   assert (this != NULL);
   linkElem (item, lastElem, NULL);
}

void Element::insertPrevElem (Element * item)
{
   assert (this != NULL);
   assert (upperElem != NULL);
   upperElem->linkElem (item, prevElem, this);
}

void Element::insertNextElem (Element * item)
{
   assert (this != NULL);
   assert (upperElem != NULL);
   upperElem->linkElem (item, this, nextElem);
}

void Element::removeElem ()
{
   assert (this != NULL);
   assert (upperElem != NULL);

   #if 1
   upperElem->disconnectSubitem (this);
   onRemove ();

   if (Tree_Observer != NULL)
      Tree_Observer->Tree_OnRemove ();
   #endif

   if (prevElem == NULL)
      upperElem->firstElem = nextElem;
   else
      prevElem->nextElem = nextElem;

   if (nextElem == NULL)
      upperElem->lastElem = prevElem;
   else
      nextElem->prevElem = prevElem;

   prevElem = NULL;
   nextElem = NULL;
   upperElem = NULL;
}

void Element::removeAllElements ()
{
   Element * p = firstElem;
   while (p != NULL)
   {
      Element * t = p;
      p = p->nextElem;
      t->removeElem ();
      delete t;
   }
   firstElem = NULL;
   lastElem = NULL;
}

/* ---------------------------------------------------------------------- */

void Element::sortItems ()
{
   const int num_groups = max_group - min_group + 1;
   Element * start [num_groups]; // table of empty lists
   Element * stop [num_groups]; // table of empty lists
   for (int inx = 0; inx < num_groups; inx++)
   {
       start [inx] = NULL;
       stop [inx] = NULL;
   }

   // store items into table
   Element * item = getFirstElem ();
   while (item != NULL)
   {
      // group number
      int grp = item->Group;
      if (grp < min_group || grp > max_group)
         grp = min_group;

      if (grp == 0)
         grp = norm_group; // change zero to norm_group

      item->Group = grp; // store modified group number

      int inx = grp - min_group;

      Element * nxt = item->getNextElem ();

      // store section into table
      item->prevElem = stop [inx];
      item->nextElem = NULL;
      // do not change uppperElem

      if (start [inx] == NULL)
         start [inx] = item;

      if (stop [inx] != NULL)
         stop [inx]->nextElem = item;

      stop [inx] = item;

      item = nxt;
   }

   // clear original list
   firstElem = NULL;
   lastElem = NULL;

   // join lists from table
   for (int inx = 0; inx < num_groups; inx++)
   {
      Element * item = start [inx];
      if (item != NULL)
      {
         if (lastElem != NULL)
            lastElem->nextElem = item;

         item->prevElem = lastElem;

         if (firstElem == NULL)
            firstElem = item;

         lastElem = stop [inx];
      }
   }
}

/* ---------------------------------------------------------------------- */

void Element::createFields ()
{
   if (! fields_created)
   {
      createLocalFields ();
      fields_created = true;
   }
}

/* ---------------------------------------------------------------------- */

Field * Element::searchField (string name)
{
   Field * result;
   assert (this != NULL);

   if (name == "")
   {
      result = NULL; // when Alias == ""
   }
   else
   {
      result = firstField;
      while (result != NULL &&
             result->Name != name &&
             result->Alias != name)
         result = result->nextField;
   }

   return result;
}

Field * Element::findField (string name)
{
   Field * result;
   result = searchField (name);
   check (result != NULL, "Unknown field: " + getInfoName () + "." + name);
   return result;
}

Field * Element::createField (string p_name)
{
   StrFieldHolder * fld = new StrFieldHolder;
   fld->setName (p_name);
   this->insertLastField (fld);

   return fld;
}

/* ---------------------------------------------------------------------- */

Element * Element::searchElement (string id)
{
   Element * result = NULL;
   if (id != "")
   {
      result = firstElem;
      while (result != NULL && result->Name != id)
      {
         result = result->nextElem;
      }
   }
   return result;
}

/* ---------------------------------------------------------------------- */

#if 1
static Element * searchElement (Element * top, const string id)
{
   assert (top != NULL);
   Element * result = NULL;

   Element * item = top->getFirstElem ();
   while (item != NULL && result == NULL)
   {
      Field * field = item->searchField ("Name");
      if (field != NULL && field->getText () == id)
         result = item;
      else
         result = searchElement (item, id); // subitems
      item = item->getNextElem ();
   }

   return result;
}

Element * lookupElement (Element * start, const string id)
{
   assert (start != NULL);

   Element * top = start;
   while (top->getUpperElem () != NULL)
      top = top->getUpperElem ();

   return searchElement (top, id);
}

Element * lookupElement (Field * start, const string id)
{
   assert (start != NULL);
   return lookupElement (start->getUpperElem (), id);
}
#endif

/********************************** COPY **********************************/

#if 1
void copyFields (Element * target, Element * source, bool include_name)
{
   Field * field = source->getFirstField ();
   while (field != NULL)
   {
      if (field != source->NameParam || include_name)
      {
         string name = field->getName ();
         Field * new_field = target->searchField (name);
         if (new_field == NULL)
            new_field = target->createField (name);

         check (new_field != NULL, "Cannot copy fields", field);
         new_field->setText (field->getText ());
         // target->insertLastField (new_field); already done by CreateField
      }
      field = field->getNextField ();
   }
}

void copyElements (Element * target, Element * source);

void copyElement (Element * target, Element * elem)
{
   Element * new_elem = NULL;

   Factory * factory = lookupFactory (target, elem->getTypeName ());
   if (factory != NULL)
      new_elem = factory->createElement ();

   check (new_elem != NULL, "Cannot copy element", elem);
   target->insertLastElem (new_elem);

   copyFields (new_elem, elem, true); // include name field
   copyElements (new_elem, elem);
}

void copyElements (Element * target, Element * source)
{
   Element * elem = source->getFirstElem ();
   while (elem != NULL)
   {
      copyElement (target, elem);
      elem = elem->getNextElem ();
   }
}
#endif

/************************** INFORMATION MESSAGES **************************/

string Element::getInfoName ()
{
   string result = "";

   if (NameParam != NULL)
      result = NameParam -> getText ();

   if (result == "")
      result = '<' + getTypeName () + '>';

   if (upperElem != NULL)
      result = upperElem->getInfoName () + '.' + result;

   return result;
}

string Field::getInfoName ()
{
   string result = Name;

   if (upperElem != NULL)
      result = upperElem->getInfoName () + '.' + result;

   return result;
}

/* ---------------------------------------------------------------------- */

void element_info (Element * elem,
                   string & note,
                   string & file_name, int & line, int & column)
{
   if (elem != NULL)
   {
      note = " [ " + elem -> getInfoName () + " ]";
      file_name = elem->OrigFileName;
      line = elem->OrigLine;
      column = elem->OrigColumn;
   }
   else
   {
      note = " [ element==NULL ]";
      file_name = "";
      line = 0;
      column = 0;
   }
}

void field_info (Field * field,
                 string & note,
                 string & file_name, int & line, int & column)
{
   if (field != NULL)
   {
      note = " [ " + field -> getInfoName () + " ]";

      Element * elem = field->getUpperElem ();
      string tmp_note;
      element_info (elem, tmp_note, file_name, line, column);

      if (field->OrigLine != 0)
      {
         line = field->OrigLine;
         column = field->OrigColumn;
      }
   }
   else
   {
      note = " [ field==NULL ]";
      file_name = "";
      line = 0;
      column = 0;
   }
}

/* ---------------------------------------------------------------------- */

void info (string msg, Element * elem)
{
   string note;
   string file_name;
   int line;
   int column;
   element_info (elem, note, file_name, line, column);

   info (msg + note, file_name, line, column);
}

void info (string msg, Field * field)
{
   string note;
   string file_name;
   int line;
   int column;
   field_info (field, note, file_name, line, column);

   info (msg + note, file_name, line, column);
}

/* ---------------------------------------------------------------------- */

void warning (string msg, Element * elem)
{
   string note;
   string file_name;
   int line;
   int column;
   element_info (elem, note, file_name, line, column);

   warning (msg + note, file_name, line, column);
}

void warning (string msg, Field * field)
{
   string note;
   string file_name;
   int line;
   int column;
   field_info (field, note, file_name, line, column);

   warning (msg + note, file_name, line, column);
}

/* ---------------------------------------------------------------------- */

void error (string msg, Element * elem)
{
   string note;
   string file_name;
   int line;
   int column;
   element_info (elem, note, file_name, line, column);

   error (msg + note, file_name, line, column);
}

void error (string msg, Field * field)
{
   string note;
   string file_name;
   int line;
   int column;
   field_info (field, note, file_name, line, column);

   error (msg + note, file_name, line, column);
}

/* ---------------------------------------------------------------------- */

void check (bool cond, string msg, Element * elem)
{
   if (!cond)
      error (msg, elem);
}

void check (bool cond, string msg, Field * field)
{
   if (!cond)
      error (msg, field);
}

/****************************** ELEMENT TREE ******************************/

void Element::addElem (Element * item)
{
   insertLastElem (item);
}

/* ---------------------------------------------------------------------- */

void Element::Prepare ()
{
   Field * field = getFirstField ();
   while (field != NULL)
   {
      field->Prepare ();
      field = field->getNextField ();
   }
}

/* ---------------------------------------------------------------------- */

#define RECURSIVE(name, call) \
   void Element::recursive##name () \
   { \
      assert (this != NULL); \
      call (); \
      if (! ignore_subelements) \
      { \
         Element * item = getFirstElem (); \
         while (item != NULL) \
         { \
            item->recursive##name (); \
            item = item->getNextElem (); \
         } \
      } \
   }

RECURSIVE (Prepare, Prepare)

RECURSIVE (BasicProg, basicProg)
RECURSIVE (Prog, Prog)
RECURSIVE (UpdateProg, updateProg)
RECURSIVE (CompleteProg, completeProg)

RECURSIVE (WriteSource, writeSource)

/*
   void TElement::RecursiveProg ()
   {
      assert (this != NULL);
      // info ("RecursiveProg " + getInfoName ());
      Prog ();
      if (! ignore_subelements)
      {
         TElement * item = getFirstElem ();
         while (item != NULL)
         {
            // info ("before RecursiveProg " + item->getInfoName ());
            item->RecursiveProg ();
            // info ("after RecursiveProg " + item->getInfoName ());
            item = item->getNextElem ();
         }
      }
   }
*/

/* ---------------------------------------------------------------------- */

void Element::Compile ()
{
   recursivePrepare ();

   recursiveBasicProg ();
   recursiveProg ();
   recursiveUpdateProg ();

   recursiveCompleteProg (); // %%
}

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE
