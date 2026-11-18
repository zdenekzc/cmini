
/* flexible.cc */

#include "flexible.h"

OPEN_NAMESPACE

/* TFlexibleFactory */

FlexibleFactory::FlexibleFactory ()
{ }

/* TFlexibleElement */

void FlexibleElement::setTypeName (string p_value)
{
   if (p_value != TypeName)
   {
      TypeName = p_value;
   }
}

FlexibleElement::FlexibleElement () :
   TypeName ()
{ }

/* TFlexibleField */

string FlexibleField::toStr ()
{
   return "???";
}

void FlexibleField::fromStr (string s)
{ }

void FlexibleField::recallValue ()
{ }

void FlexibleField::storeValue ()
{ }

string FlexibleField::getText ()
{
   recallValue ();
   return toStr ();
}

void FlexibleField::setText (string s)
{
   fromStr (s);
   storeValue ();
}

FlexibleField::FlexibleField ()
{ }

#if 1
string FlexibleElement::getInfoName ()
{
   return "TFlexibleElement " + Element::getInfoName ();
}
#endif

CLOSE_NAMESPACE
