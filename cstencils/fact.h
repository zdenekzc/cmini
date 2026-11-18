
/* fact.h */

#ifndef __PROG_FACT_H__
#define __PROG_FACT_H__

#include "std.h"
#include "dictionary.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

class Factory;
class FactoryList;
class FactoryItem;

class Element;

typedef Dictionary < string, Factory * > FactoryDict;

/* ---------------------------------------------------------------------- */

class Factory
{
   private:
      string Name;
   public:
      string getName () { return Name; }
      void setName (string value) { Name = value; }

   private:
      string Tooltip;
   public:
      string getTooltip () { return Tooltip; }
      void setTooltip (string value) { Tooltip = value; }

   private:
      string Alias; // source file alias
   public:
      string getAlias () { return Alias; }
      void setAlias (string value) { Alias = value; }

   private:                                  //
      string AliasGroup; // name of target factory
   public:
      string getAliasGroup () { return AliasGroup; }
      void setAliasGroup (string value) { AliasGroup = value; }

   private:
      string Image;
   public:
      string getImage () { return Image; }
      void setImage (string value) { Image = value; }

   private:
      string SelectedImage;
   public:
      string getSelectedImage () { return SelectedImage; }
      void setSelectedImage (string value) { SelectedImage = value; }

   private:
      string PageName; // toolbar page
   public:
      string getPageName () { return PageName; }
      void setPageName (string value) { PageName = value; }

   private:
      bool OnlyImage; // show only icon !?
   public:
      bool getOnlyImage () { return OnlyImage; }
      void setOnlyImage (bool value) { OnlyImage = value; }

   private:
      string ModuleName; // source module !?
   public:
      string getModuleName () { return ModuleName; }
      void setModuleName (string value) { ModuleName = value; }

   public:
      string ParentName;
      Factory * ParentFactory;
   public:
      void setParent (string parent_name);

   public:
      FactoryDict * LocalDict; // local dictionary

   public:
      virtual Element * newElement () = 0;
      virtual Element * createElement ();

   public:
      #if 0
      bool isDerivedFrom (Factory * fact);
      void readAttr (TextInput & io, string id);
      void writeSource (TextOutput & io, void * obj);
      void writeAttributes (TextOutput & io, void * obj);
      void writeLists (TextOutput & io, void * obj);
      #endif

   public:
      Factory ();
      virtual ~ Factory ();
};

class FactoryList
{
   // list of referencies to factories
   public:
      FactoryItem * first;
      FactoryItem * last;
   protected:
      void link (FactoryItem * item, FactoryItem * bef, FactoryItem * aft);
   public:
      FactoryItem * getFirst () { return first; }
      FactoryItem * getLast () { return last; }
      void insertFirst (FactoryItem * item);
      void insertLast (FactoryItem * item);
      void removeAll ();

   public:
      void add (Factory * fact);

   public:
      FactoryList ();
      virtual ~FactoryList ();

      friend class FactoryItem;
};

class FactoryItem
{
   // list item
   public:
      FactoryItem * prev;
      FactoryItem * next;
      FactoryList * factoryList;

      FactoryItem * getPrev () { return prev; }
      FactoryItem * getNext () { return next; }
      FactoryList * getFactoryList () { return factoryList; }

      void insertBefore (FactoryItem * item);
      void insertAfter (FactoryItem * item);
      void remove ();

   public:
      Factory * ref;

   public:
      FactoryItem ();
      virtual ~FactoryItem ();

      friend class FactoryList;
};

/* ---------------------------------------------------------------------- */

/* dictionaries */

// global variables
extern FactoryList factories; // list of all factories
Factory * searchFactory (string name);
Factory * findFactory (string name);
Factory * lookupFactory (Element * top, string name);

void registerFactory (Factory * f);
void registerFactories (FactoryList * list, string module_name = "");

void add_components_into_toolbox (string title, FactoryList * list); // !? forward declaration

CLOSE_NAMESPACE

#endif /* __PROG_FACT_H__ */
