
/* queue.h */

#ifndef __PROG_QUEUE_H__
#define __PROG_QUEUE_H__

#include "std.h" // REDHAT 8.0
#include <queue>

OPEN_NAMESPACE

template <typename Data>
class Queue
{
   private:
      queue <Data> list;

   public:
      bool empty  ()          { return list.empty (); }
      int size ()             { return list.size (); }
      void add (Data value)   { list.push (value); }
      void store (Data value) { list.push (value); }
      Data recall ()          { Data value = list.front (); list.pop (); return value; }

      void clear ()
      {
         while (! list.empty ())
            list.pop ();
      }
};

typedef Queue <string> StringQueue;

CLOSE_NAMESPACE

#endif /* __PROG_QUEUE_H__ */
