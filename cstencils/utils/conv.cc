
/* conv.cc */

#include "conv.h"
#include "str.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

string EnumToStr (int value, int count, const conv_table_t * table)
{
   int i = 0;
   while (i < count && value != table [i].index)
      i ++;

   if (i < count)
      return table [i].text;
   else
      return ""; // !?
}

void StrToEnum (string txt, int count, const conv_table_t * table,
                int & result, bool & ok)
{
   int i = 0;
   while (i < count && txt != table [i].text)
      i ++;

   if (i < count)
   {
      result = table[i].index;
      ok = true;
   }
   else
   {
      ok = false;
   }
}

bool StrToEnum (string txt, int count, const conv_table_t * table,
                int & result)
{
   bool ok;
   StrToEnum (txt, count, table, result, ok);
   return ok;
}

/* ---------------------------------------------------------------------- */

string BitsToStr (unsigned int value, int count, const conv_table_t * table)
{
   string result = "";

   for (int inx = 0; inx < count; inx ++)
      if ((value & table[inx].index) != 0)
      {
         if (result != "")
            result = result + '|';
         result = result + table[inx].text;
      }

   return result;
}

void StrToBits (string txt, int count, const conv_table_t * table,
                unsigned int & result, bool & ok)
{
   int inx = 0;
   int len = txt.length ();
   unsigned int value = 0;

   ok = true;
   while (ok && inx < len)
   {
      int start = inx;
      while (inx < count && txt[inx] != '|')
         inx ++;

      string item_name = txt.substr (start, inx-start-1);
      int item_value;
      StrToEnum (item_name, count, table, item_value, ok);
      if (ok)
         value = value | item_value;

      inx ++; // skip '|'
   }

   if (ok)
      result = value;
}

bool StrToBits (string txt, int count, const conv_table_t * table,
                unsigned int & result)
{
   bool ok;
   StrToBits (txt, count, table, result, ok);
   return ok;
}

/* ---------------------------------------------------------------------- */

string PtrToStr (void * p)
{
   return NumToHex (reinterpret_cast <long long> (p));
}

/* ---------------------------------------------------------------------- */

// #define TEST_CONV

#ifdef TEST_CONV

static void test ()
{
   string s;
   bool ok;

   bool b;
   int i;
   string t;

   s = TConv <int> :: toStr (i);
   TConv <int> :: fromStr (s, i, ok);
   ok = TConv <int> :: fromStr (s, i);

   s = TConv <bool> :: toStr (b);
   TConv <bool> :: fromStr (s, b, ok);
   ok = TConv <bool> :: fromStr (s, b);

   s = TConv <string> :: toStr (t);
   TConv <string> :: fromStr (s, t, ok);
   ok = TConv <string> :: fromStr (s, t);

   enum Color { red, blue, green, yellow };
   const int color_count = 4;
   const conv_table_t color_table [] =
   { { red, "red" },
     { blue, "blue" },
     { green, "green" },
     { yellow, "yellow" } };

   Color c;
   s = EnumToStr (c, color_count, color_table);
   StrToEnum (s, color_count, color_table, i, ok);
}

#endif

CLOSE_NAMESPACE
