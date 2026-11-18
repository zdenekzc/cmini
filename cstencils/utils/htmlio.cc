
/* htmlio.cc */

#include "htmlio.h"

#include "str.h"

OPEN_NAMESPACE

/********************************* HTML STYLE **********************************/

void HtmlOutput::Style ()
{
   putLn (".namespace { color: Gold; }");
   putLn (".type { color: Chocolate; }");
   putLn (".class { color: Blue; }");
   putLn (".enum { color: DarkOrange; }");
   putLn (".template { color: DarkViolet; }");
   putLn (".variable { color: SaddleBrown; }");
   putLn (".function { color: Green; }");

   putLn (".class_group { color: #3399ff; }");
   putLn (".function_group { color: Lime; }");
   putLn (".overloaded_function { color: Olive; }");
   putLn (".overloaded_function_group { color: Chartreuse; }");

   putLn (".template_instance { color: #780078; }");
   putLn (".template_specialization { color: BlueViolet; }");

   putLn (".template_param { color: Magenta; }");
   putLn (".nested_template_param { color: Coral; }");

   putLn (".function_template { color: Turquoise; }");
   putLn (".function_template_instance { color: LightSeaGreen; }");
   putLn (".function_template_specialization { color: Aquamarine; }");

   putLn (".directive { color: LimeGreen; }");
   putLn (".include_directive { color: LimeGreen; }");
   putLn (".branch_directive { color: LimeGreen; }");
   putLn (".macro_param { color: LimeGreen; }");
   putLn (".macro_detail { color: LimeGreen; }");
   putLn (".macro_use { color: LimeGreen; }");
   putLn (".macro_expansion { color: LimeGreen; }");

   putLn (".keyword { color: FireBrick; }");
   putLn (".ident { color: Navy; }");
   putLn (".number { color: RoyalBlue; }");
   putLn (".float { color: Purple; }");
   putLn (".string { color: Blue; }");
   putLn (".character { color: CornflowerBlue; }");
   putLn (".separator { color: Navy; }");

   putLn (".rule { color: OrangeRed; }");
   putLn (".terminal { color: Blue; }");
   putLn (".nonterminal { color: SaddleBrown; }");
   putLn (".grammar { color: CornflowerBlue; }");

   putLn (".comment { color: Grey; /* font-style:italic; */ }");
   putLn (".property { color: CornFlowerBlue; }");

   putLn (".line_number { color: Grey; }");
   putLn (".address { color: #3399ff; }");
   putLn (".jump { color: CornflowerBlue; }");

   putLn (".key { color: Blue; }");
   putLn (".value { color: Orange; }");

   putLn (".other { color: Orange; }");
   putLn (".unknown { color: Red; }");

   putLn (".info { color: Blue; }");
   putLn (".warning { color: Orange; }");
   putLn (".error { color: Red; }");

   putLn ("h1 {");
   incIndent ();
   putLn ("margin: 0.5em auto 0.5em auto;");
   putLn ("padding: 15px 10px 15px 55px;");
   putLn ("color: Blue;");
   putLn ("background: #ffff70;");
   putLn ("border: 1px solid #c0c0e0;");
   decIndent ();
   putLn ("}");

   putLn ("h2 {");
   incIndent ();
   putLn ("margin: 0.5em auto 0.5em auto;");
   putLn ("padding: 10px 10px 10px 55px;");
   putLn ("color: Blue;");
   putLn ("background: #ffff90;");
   putLn ("border: 1px solid #c0c0e0;");
   decIndent ();
   putLn ("}");

   putLn ("h3 {");
   incIndent ();
   putLn ("margin: 0.5em auto 0.5em auto;");
   putLn ("padding: 5px 5px 5px 55px;");
   putLn ("color: Blue;");
   putLn ("background: #ffffb0;");
   putLn ("border: 1px solid #c0c0e0;");
   decIndent ();
   putLn ("}");

}

/********************************* HTML HELPER *********************************/

HtmlOutput::HtmlOutput () :
      fUseHtml (true)
{
}

HtmlOutput::~HtmlOutput ()
{
}

inline string FceAttr (const string name, const string value)
{
   return name + "=" + quote2 + quoteStrContent (value) + quote2;
}

void HtmlOutput::Attr (const string name, const string value)
// name = "value"
{
   if (fUseHtml)
   {
      put (name);
      putChr ('=');
      putChr (quote2);
      put (quoteStrContent (value));
      putChr (quote2);
   }
}

void HtmlOutput::HRef (const string url, const string style, const string text)
// <a href="url" class="style"> text </a>
{
   if (fUseHtml)
   {
      put ("<a ");
      Attr ("href", url);

      if (style != "")
      {
         put (" ");
         Attr ("class", style);
      }

      put (">");

      put (text);
      put ("</a>");
   }
}

void HtmlOutput::AName (const string name, const string style, const string text)
// <a name="name"> text </a>
{
   if (fUseHtml)
   {
      put ("<a ");
      Attr ("name", name);

      if (style != "")
      {
         put (" ");
         Attr ("class", style);
      }

      if (text == "")
      {
         put ("/>");
      }
      else
      {
         put (">");
         put (text);
         put ("</a>");
      }
   }
   else
   {
      put (text);
   }
}

void HtmlOutput::StyleBegin (const string style)
{
   if (fUseHtml)
   {
      put ("<span ");
      Attr ("class", style);
      put (">");
   }
}

void HtmlOutput::StyleEnd ()
{
   if (fUseHtml)
      put ("</span>");
}

void HtmlOutput::StyledText (const string text, const string style)
{
   if (fUseHtml)
   {
      put ("<span ");
      Attr ("class", style);
      put (">");
      put (text);
      put ("</span>");
   }
   else
   {
      put (text);
   }
}

void HtmlOutput::PreBegin ()
{
   if (fUseHtml)
      putLn ("<pre>");
}

void HtmlOutput::PreEnd ()
{
   if (fUseHtml)
      putLn ("</pre>");
}

void HtmlOutput::PutHtml (const string s)
{
   if (fUseHtml)
      put (s);
}

void HtmlOutput::PutHtmlLn (const string s)
{
   if (fUseHtml)
      putLn (s);
}

void HtmlOutput::PutNorm (const string s)
{
   if (! fUseHtml)
      put (s);
}

void HtmlOutput::PutNormLn (const string s)
{
   if (! fUseHtml)
      putLn (s);
}

string HtmlOutput::HtmlEscape (const string s)
{
   int len = s.length ();

   bool ok = true;

   for (int i = 0; i < len; i++)
   {
      char c = s[i];

      if (c == '<' || c == '>' || c == '&')
         ok = false;
   }

   if (ok)
   {
      return s;
   }
   else
   {
      string result = "";

      for (int i = 0; i < len; i++)
      {
         char c = s[i];

         if (c == '<')
            result += "&lt;";
         else if (c == '>')
            result += "&gt;";
         else if (c == '&')
            result += "&amp;";
         else
            result += c;
      }

      return result;
   }
}

void HtmlOutput::PutHtmlEscape (const string s)
// Replace < > & characters
{
   if (fUseHtml)
   {
      int len = s.length ();

      bool ok = true;

      for (int i = 0; i < len; i++)
      {
         char c = s[i];

         if (c == '<' || c == '>' || c == '&')
            ok = false;
      }

      if (ok)
      {
         put (s);
      }
      else
      {
         for (int i = 0; i < len; i++)
         {
            char c = s[i];

            if (c == '<')
               put ("&lt;");
            else if (c == '>')
               put ("&gt;");
            else if (c == '&')
               put ("&amp;");
            else
               putChr (c);
         }
      }
   }
   else
   {
      put (s);
   }
}

void HtmlOutput::PutHead (const string title)
{
   if (fUseHtml)
   {
      string charset = "ISO-8859-1";

      putLn ("<?xml " + FceAttr ("version", "1.0") + "?>");

      putLn ("<!DOCTYPE html PUBLIC " +
             quoteStr ("-//W3C//DTD XHTML 1.0 Transitional//EN") + " " +
             quoteStr ("http://www.w3.org/R/xhtml1/DTD/xhtml1-transitional.dtd") + ">");

      putLn ("<html " + FceAttr ("xmlns", "http://www.w3.org/1999/xhtml") + " " +
             FceAttr ("xml:lang", "en") + " " +
             FceAttr ("lang", "en") + ">");

      putLn ("<head>");

      putLn ("<meta " + FceAttr ("http-equiv", "Content-Type") + " " +
             FceAttr ("content", "text/html; charset=" + charset) + " />");

      putLn ("<title>" + title + "</title>");

      if (fStyleFileName != "")
      {
         putLn ("<link " + FceAttr ("rel", "stylesheet") + " " +
                FceAttr ("type", "text/css") + " " +
                FceAttr ("href", fStyleFileName) + " />");
      }
      else
      {
         putLn ("<style " + FceAttr ("type", "text/css") + ">");
         incIndent ();
         Style ();
         decIndent ();
         putLn ("</style>");
      }

      putLn ("</head>");
      putLn ("<body>");
   }
}


void HtmlOutput::PutTail ()
{
   if (fUseHtml)
   {
      putLn ("</body>");
      putLn ("</html>");
   }
}

CLOSE_NAMESPACE
