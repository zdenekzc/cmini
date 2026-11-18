
/* collection.h */

#ifndef __PROG_COLLECTION_H__
#define __PROG_COLLECTION_H__

#include "std.h"

#include <set>

OPEN_NAMESPACE

/* Collection */

template <typename Key>
class Collection
{
   private:
      set <Key> data; // standard set, not SET_TYPE, for types without hash<Key>

   public:
      bool contains (Key inx) { return data.find (inx) != data.end (); }
      void add (Key inx)      { data.insert (inx); }
      void remove (Key inx)   { data.erase (inx); }
      void clear ()           { data.clear (); }
};

template <typename Key>
class Collection <Key *>
{
   private:
      set <size_t> data;

   public:
      bool contains (Key * inx) { return data.find (reinterpret_cast <size_t> (inx)) != data.end (); }
      void add (Key * inx)      { data.insert (reinterpret_cast <size_t> (inx)); }
      void remove (Key * inx)   { data.erase (reinterpret_cast <size_t> (inx)); }
      void clear ()             { data.clear (); }
};

#if 0
template < >
class Collection <string>
{
   private:
      set <string> data;

   public:
      bool contains (string inx) { return data.find (inx) != data.end (); }
      void add (string inx)      { data.insert (inx); }
      void remove (string inx)   { data.erase (inx); }
      void clear ()              { data.clear (); }
};
#endif

/* ---------------------------------------------------------------------- */

#if 0
template <typename Key>
class Collection
{
   private:
      Dictionary <Key, bool> data;

   public:
      // bool contains (Key inx) { return data.contains (inx) && data.get (inx); }
      bool contains (Key inx) { return data.get (inx); }
      void add (Key inx)      { data.store (inx, true); }
      void remove (Key inx)   { data.store (inx, false); }
      void clear ()           { data.clear (); }
};
#endif

/* ---------------------------------------------------------------------- */
typedef Collection <string> StringCollection;

CLOSE_NAMESPACE

#endif /* __PROG_COLLECTION_H__ */
