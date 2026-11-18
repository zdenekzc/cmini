
/* fact.cc */

#include "fact.h"

#include "elem.h"

#include "inf.h" // function check
#include "str.h" // function HasPrefix used in debug messages
#include "textio.h"

OPEN_NAMESPACE

/* TFactory */

Factory::Factory ():
   Name (),
   Tooltip (),
   Alias (),
   AliasGroup (),
   Image (),
   SelectedImage (),
   PageName (),
   OnlyImage (false),
   ModuleName (),
   ParentName (),
   ParentFactory (NULL),
   LocalDict (NULL)
{ }

Factory::~Factory ()
{ }

/* TFactoryList */

void FactoryList::link (FactoryItem * item, FactoryItem * bef, FactoryItem * aft)
{
   assert (this != NULL);
   assert (item != NULL);

   assert (item->prev == NULL);
   assert (item->next == NULL);
   assert (item->factoryList == NULL);

   item->prev = bef;
   item->next = aft;
   item->factoryList = this;

   if (bef == NULL)
      this->first = item;
   else
      bef->next = item;

   if (aft == NULL)
      this->last = item;
   else
      aft->prev = item;
}

void FactoryList::insertFirst (FactoryItem * item)
{
   assert (this != NULL);
   link (item, NULL, first);
}

void FactoryList::insertLast (FactoryItem * item)
{
   assert (this != NULL);
   link (item, last, NULL);
}

void FactoryList::removeAll ()
{
   FactoryItem * p = first;
   while (p != NULL)
   {
      FactoryItem * t = p;
      p = p->next;
      t->remove ();
      delete t;
   }
   first = NULL;
   last = NULL;
}

FactoryList::~FactoryList ()
{
   removeAll ();
}

FactoryList::FactoryList ():
   first (NULL),
   last (NULL)
{ }

/* TFactoryItem */

void FactoryItem::insertBefore (FactoryItem * item)
{
   assert (this != NULL);
   assert (factoryList != NULL);
   factoryList->link (item, prev, this);
}

void FactoryItem::insertAfter (FactoryItem * item)
{
   assert (this != NULL);
   assert (factoryList != NULL);
   factoryList->link (item, this, next);
}

void FactoryItem::remove ()
{
   assert (this != NULL);
   assert (factoryList != NULL);

   if (prev == NULL)
      factoryList->first = next;
   else
      prev->next = next;

   if (next == NULL)
      factoryList->last = prev;
   else
      next->prev = prev;

   prev = NULL;
   next = NULL;
   factoryList = NULL;
}

FactoryItem::~FactoryItem ()
{
   if (factoryList != NULL)
      remove ();
}

FactoryItem::FactoryItem ():
   prev (NULL),
   next (NULL),
   factoryList (NULL),
   ref (NULL)
{ }

/****************************** DICTIONARIES ******************************/

// global variables
FactoryDict global_alias_dict; // alias -> factory
FactoryDict factory_dict;      // name -> factory
FactoryList factories;         // list of all factories

Factory * searchFactory (string name)
{
   Factory * factory = factory_dict.get (name);
   return factory;
}

Factory * findFactory (string name)
{
   Factory * factory = searchFactory (name);
   check (factory != NULL, "Unknown factory: " + name);
   return factory;
}

Factory * lookupFactory (Element * top, string name)
{
   Factory * factory = NULL;
   Element * elem = top;

   while (factory == NULL && elem != NULL)
   {
      Factory * repository = elem->getFactory ();
      while (factory == NULL && repository != NULL)
      {
         FactoryDict * dict = repository->LocalDict;
         if (dict != NULL)
            factory = dict->get (name);
         repository = repository->ParentFactory;
      }
      elem = elem->getUpperElem (); // upper element
   }

   if (factory == NULL)
      factory = global_alias_dict.get (name);

   if (factory == NULL)
      factory = factory_dict.get (name);

   return factory;
}

/******************************** FACTORY *********************************/

Element * Factory::createElement ()
{
   Element * result = newElement ();
   assert (result != NULL);

   result->setFactory (this);

   result->setTitle (getName ());
   result->setImage (getImage ());
   result->setSelectedImage (getSelectedImage ());

   result->Initialize ();

   return result;
}

void Factory::setParent (string parent_name)
{
   ParentName = parent_name;
}

void FactoryList::add (Factory * fact)
{
   FactoryItem * item = new FactoryItem;
   item->ref = fact;
   insertLast (item);
}

/****************************** REGISTRATION ******************************/

void check_not_present (FactoryDict & dict, string name)
{
   check (! dict.contains (name),
          "Identifier " + name + " already in dictionary");
}

void registerAlias (string alias_group,
                    string alias,
                    Factory * factory)
{
   // debug ("RegisterAlias " + alias);

   // store alias into dictionary
   assert (alias != "");
   assert (factory != NULL);

   FactoryDict * dict = NULL;

   if (alias_group == "")
   {
      // global dictionary
      dict = & global_alias_dict;
   }
   else
   {
      // find or create local dictionary
      Factory * f = factory_dict.get (alias_group);
      check (f != NULL, "Unknown factory: " + alias_group +
                        " (used by " + factory->getName () + ")" );

      if (f->LocalDict == NULL)
         f->LocalDict = new FactoryDict;

      dict = f->LocalDict;
   }

   check_not_present (*dict, alias);
   dict->store (alias, factory);
}

void registerFactory (Factory * f)
{
   assert (f != NULL);
   // debug ("RegisterFactory " + f->getName ());

   if (f->ParentName != "")
   {
      f->ParentFactory = searchFactory (f->ParentName); // !? %%
      // f->ParentFactory = FindFactory (f->ParentName);
   }

   // insert into list of factories
   factories.add (f);

   // store name into dictionary
   string name = f->getName ();


   // if (HasPrefix (f->getAlias (), "dupl"));
   //    debug ("RegisterFactory " + f->getAlias ());

   if (factory_dict.contains (name))
      info ("Factory " + name + " already registered"); // !? namespaces
   else
      factory_dict.store (name, f);

   // store alias into alternative dictionary
   string group = f->getAliasGroup ();
   string alias = f->getAlias ();
   if (alias != "") registerAlias (group, alias, f);
}

void registerFactories (FactoryList * list, string module_name)
{
   assert (list != NULL);
   // debug ("RegisterFactories " + module_name);

   FactoryItem * item = list->getFirst ();
   while (item != NULL)
   {
      registerFactory (item->ref);
      if (module_name != "")
         item->ref->setModuleName (module_name);
      item = item->getNext ();
   }
}

CLOSE_NAMESPACE
