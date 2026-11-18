
/* flexible.h */

#ifndef __PROG_FLEXIBLE_H__
#define __PROG_FLEXIBLE_H__

#include "fact.h"
#include "elem.h"
#include "field.h"

OPEN_NAMESPACE

class FlexibleFactory;
class FlexibleElement;
class FlexibleField;

class FlexibleFactory : public Factory
{
   public:
      FlexibleFactory ();
};

class FlexibleElement : public Element
{
   // attribute TypeName
   private:
      string TypeName;
   public:
      string getTypeName () { return TypeName; }
      void setTypeName (string p_value);

   // user declarations
   public:
      virtual string getInfoName ();

   public:
      FlexibleElement ();
};

class FlexibleField : public Field
{
   // user declarations
   public:
      virtual string toStr (); /* new */
      virtual void fromStr (string s); /* new */
      virtual void recallValue (); /* new */
      virtual void storeValue (); /* new */
      virtual string getText ();
      virtual void setText (string s);

   public:
      FlexibleField ();
};

CLOSE_NAMESPACE

#endif /* __PROG_FLEXIBLE_H__ */
