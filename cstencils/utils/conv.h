
#ifndef __PROG_CONV_H__
#define __PROG_CONV_H__

#include "std.h"
#include <sstream> // class istringstream

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

template <typename T>
inline string NumToStr (T num)
{
   ostringstream stream;
   stream << num;
   return stream.str ();
}

template <typename T>
inline void StrToNum (string txt, T & result, bool & ok)
// txt ... input value
// ok == true ... conversion succeded
// result ... converted value, assigned only when ok == true
{
   T n;
   istringstream stream (txt);

   #ifdef GCC2
      stream.unsetf (ios::skipws); // noskipws is not supported by GCC 2.96
   #else
      stream >> noskipws;
   #endif

   if (txt.length () > 2 && txt[0] == '0' && (txt[1] == 'x' || txt[1] == 'X'))
   {
      char c1;
      char c2;
      stream >> c1 >> c2 >> hex; // skip two characters and set hex
   }

   stream >> n;

   #ifdef GCC2
      ok = ! stream.fail (); // !?
   #else
      ok = (stream.eof () && ! stream.fail ());
   #endif

   if (ok)
      result = n;

   // ok == false, nothing is assigned to result
}

template <typename T>
inline bool StrToNum (string txt, T & result)
{
   bool ok;
   StrToNum (txt, result, ok);
   return ok;
}

/* ---------------------------------------------------------------------- */

inline string BoolToStr (bool value) // convert bool to string
{
   return value ? "true" : "false";
}

inline void StrToBool (string txt, bool & result, bool & ok) // convert string to boolean
{
   if (txt == "true")
   {
      result = true;
      ok = true;
   }
   else if (txt == "false")
   {
      result = false;
      ok = true;
   }
   else
   {
      ok = false;
   }
}

inline bool StrToBool (string txt, bool & result)
{
   bool ok;
   StrToBool (txt, result, ok);
   return ok;
}

/* ---------------------------------------------------------------------- */

template <typename T>
inline string NumToHex (T num)
{
   ostringstream stream;
   // stream << "0x" << hex << num;
   stream << hex << num;
   return stream.str ();
}

/* ---------------------------------------------------------------------- */

inline string IntToStr (int value)
// convert int to string
{
   return NumToStr (value);
}

inline void StrToInt (string txt, int & result, bool & ok)
// convert string to int
// txt ... input value
// ok == true ... conversion succeded
// result ... converted value, assigned only when ok == true
{
   StrToNum (txt, result, ok);
}

inline bool StrToInt (string txt, int & result)
// returns ok from previous function
{
   bool ok;
   StrToInt (txt, result, ok);
   return ok;
}

/* ---------------------------------------------------------------------- */

struct conv_table_t
{
   int index;
   const char * text;
};

string EnumToStr (int value, int count, const conv_table_t * table);

void StrToEnum (string txt,
                int count, const conv_table_t * table,
                int & result,
                bool & ok);

bool StrToEnum (string txt,
                int count, const conv_table_t * table,
                int & result);

/* ---------------------------------------------------------------------- */

string BitsToStr (unsigned int value, int count, const conv_table_t * table);

void StrToBits (string txt,
                int count, const conv_table_t * table,
                unsigned int & result,
                bool & ok);

bool StrToBits (string txt,
                int count, const conv_table_t * table,
                unsigned int & result);

/* ---------------------------------------------------------------------- */

string PtrToStr (void * p);

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_CONV_H__ */
