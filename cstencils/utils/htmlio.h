
/* htmlio.h */

/* HTML output */

#ifndef __HTMLIO_H__
#define __HTMLIO_H__

#include "textio.h"

OPEN_NAMESPACE

/* -------------------------------------------------------------------------- */

const string namespace_style = "namespace";
const string type_style = "type";
const string class_style = "class";
const string enum_style = "enum";
const string template_style = "template";
const string variable_style = "variable";
const string function_style = "function";

const string class_group_style = "class_group_style";
const string function_group_style = "function_group";
const string overloaded_function_style = "overloaded_function";
const string overloaded_function_group_style = "overloaded_function_group";

const string template_instance_style = "template_instance";
const string template_specialization_style = "template_specialization";

const string template_param_style = "template_param";
const string nested_template_param_style = "nested_template_param";

const string function_template_style = "function_template";
const string function_template_instance_style = "function_template_instance";
const string function_template_specialization_style = "function_template_specialization";

const string directive_style = "directive";
const string keyword_style = "keyword";
const string ident_style = "ident";
const string number_style = "number";
const string float_style = "float";
const string character_style = "character";
const string string_style = "string";
const string separator_style = "separator";

const string rule_style = "rule";
const string terminal_style = "terminal";
const string nonterminal_style = "nonterminal";
const string grammar_style = "grammar";

const string comment_style = "comment";
const string property_style = "property";

const string line_number_style = "line_number";
const string address_style = "address";
const string jump_style = "jump";

const string key_style = "key";
const string value_style = "value";

const string other_style = "other";
const string unknown_style = "unknown";

const string info_style = "info";
const string warning_style = "warning";
const string error_style = "error";

/* -------------------------------------------------------------------------- */

class HtmlOutput : public TextOutput
{

protected:
   bool fUseHtml;
   string fStyleFileName;

public:
   void EnableHtml (bool param_use_html) { fUseHtml = param_use_html; }
   bool IsHtmlEnabled () { return fUseHtml; }

   void SetStyleFileName (const string fileName) { fStyleFileName = fileName; }
   virtual void Style ();

   void PutHead (const string title);
   void PutTail ();

   static string HtmlEscape (const string s);
   void Attr (const string name, const string value);

   void HRef (const string url, const string style, const string text);
   void AName (const string name, const string style, const string text);

   void StyleBegin (const string style);
   void StyleEnd ();
   void StyledText (const string text, const string style);

   void PreBegin (); // pre-formated text
   void PreEnd ();

   void PutHtml (const string s); // only when Html is enabled
   void PutHtmlLn (const string s);

   void PutNorm (const string s); // only when Html is disabled
   void PutNormLn (const string s);

   void PutHtmlEscape (const string s);

   HtmlOutput ();
   virtual ~ HtmlOutput ();
};

/* -------------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __HTMLIO_H__ */
