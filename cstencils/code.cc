
/* code.cc */

#include "code.h"

#include "field.h"

#include "inf.h"
#include "str.h"
#include "conv.h" // IntToStr

#include "flexible.h"

#include "stencil.h"

#include "code-impl.cc"

OPEN_NAMESPACE

inline bool getExperimental (Element * elem)
{
    Module * module = currentModuleOrNull (elem);
    return module != NULL && module->experimental;
}

void readStore (EnvInput & io, Module * target_module);

void generateEnumDescription (EnumType * type);
void generateClassDescription (Class * cls);

#include "code-desc.cc"
#include "code-input.cc"
#include "code-output.cc"

/********************************* PROJECT *********************************/

Module * Project::getFirstModule ()
{
   Element * item = this->getFirstElem ();
   Module * result = NULL;
   if (item != NULL)
      result = item->toModule ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toModule ();
   }

   return result;
}

Module * Project::searchModule (string id)
{
   Element * item = searchElement (id);

   if (item == NULL)
      return NULL;
   else
      return item->toModule ();
}

/* ---------------------------------------------------------------------- */

void Project::getTypeSpec (string type, TypeKind & kind, Type * & spec )
{
   kind = unknown_type;
   spec = NULL;

   type = trim (type);

   if (type_table.contains (type))
      kind = type_table.get (type);

   else if (type == "bool")
      kind = bool_type;

   else if (type == "int")
      kind = int_type;

   else if (type == "long")
      kind = int_type;

   else if (type == "unsigned int")
      kind = int_type;

   else if (type == "unsigned long")
      kind = int_type;

   else if (type == "float")
      kind = float_type;

   else if (type == "double")
      kind = float_type;

   else if (type == "string")
      kind = string_type;

   else if (hasSuffixChar (type, '*'))
   {
      kind = pointer_type;

      string id = removeAsterisk (type);
      Module * m = getFirstModule ();
      while (m != NULL && spec == NULL)
      {
         spec = m->searchClass (id);
         m = m->getNextModule ();
      }
   }

   else
   {
      EnumType * e = NULL;
      Module * m = getFirstModule ();
      while (m != NULL && e == NULL)
      {
         e = m->searchEnum (type);
         m = m->getNextModule ();
      }

      if (e != NULL)
      {
         if (e->isBitSetType ())
            kind = bitset_type;
         else
            kind = enum_type;
         spec = e;
      }
      else
      {
         Class * c = NULL;
         Module * m = getFirstModule ();
         while (m != NULL && c == NULL)
         {
              c = m->searchClass (type);
              m = m->getNextModule ();
         }

         if (c != NULL)
         {
            kind = class_type;
            spec = c;
         }
      }
   }
}

TypeKind Project::getTypeKind (string type)
{
   TypeKind kind;
   Type * spec;
   getTypeSpec (type, kind, spec);
   return kind;
}

string Project::initValue (string type, bool construct)
{
   string result = "?"; // !?

   TypeKind kind;
   Type * spec;
   getTypeSpec (type, kind, spec);

   switch (kind)
   {
      case bool_type:
         result = "false";
         break;

      case int_type:
         result = "0";
         break;

      case float_type:
         result = "0.0";
         break;

      case string_type:
         if (construct)
            result = ""; // in constructor
         else
            result = two_quotes; // in assigment
         break;

      case pointer_type:
         result = "NULL";
         break;

      case enum_type:
         {
            assert (spec != NULL);
            EnumType * e = spec->toEnumType ();
            assert (e != NULL);
            EnumItem * i = e->getFirstItem ();
            if (i == NULL)
            {
                warning ("Empty enumeration type: " + type);
                result = "0";
            }
            else
            {
               // if (i->qualified_name != "")
               //    result = i->qualified_name;
               // else
                  result = i->Name;
            }
         }
         break;

      case bitset_type:
         result = type + " (0)";
         break;

      case class_type:
         if (construct)
            result = ""; // constructor without parameter
         break;

      default:
         result = "";
         if (construct && init_table.contains (type))
            result = init_table.get (type);
         else
         {
            if (! warning_table.contains (type))
            {
               warning_table.store (type, true);
               warning ("cannot find initialization value for " + type);
            }
         }
         break;
   }

   // info (type + " <- " + result);
   return result;
}

string Project::getInitValue (string type)
{
   return initValue (type, false);
}

string Project::getConstructorInitValue (string type)
{
   return initValue (type, true);
}

string Project::getProgramValue (string type, string value)
{
   string result = value;

   TypeKind kind;
   Type * spec;
   getTypeSpec (type, kind, spec);

   switch (kind)
   {
      case string_type:
         result = quoteStr (value);
         break;

      default:
         break;
   }

   return result;
}

void Project::storeTypeKind (string type, TypeKind kind)
{
   // debug ("StoreTypeKind " + type + " -> " + IntToStr (kind));
   assert (! type_table.contains (type));
   type_table.store (type, kind);
}

void Project::storeInitValue (string type, string value)
{
   // debug ("StoreInitValue " + type + " -> " + value);
   assert (! init_table.contains (type));
   init_table.store (type, value);
}

/********************************** MODULE *********************************/

Func * Module::getModuleInit ()
{
   if (init_func == NULL)
   {
      Func * f = new Func;
      f->Name = Name + "_init";
      f->Group = init_module_group;
      add (f);
      init_func = f;
   }

   return init_func;
}

void Module::addFactoryToInitList (string id, string cond_symbol)
{
   string fact_list = Name + "_factory_list";

   if (fact_point == NULL)
   {
      Func * f = new Func;
      f->Name = Name + "_factories";
      f->Type = "FactoryList" + ptr_mark;
      f->Group = init_module_group;
      f->Comment = "list of factories";
      f->putLn ("static TFactoryList * " + fact_list + " = NULL;");
      f->putLn ("if (" + fact_list + " == NULL)");
      f->putLn ("{");
      f->indent ();
      f->putLn (fact_list + " = new TFactoryList;");
      f->Location (fact_point);
      f->unindent ();
      f->putLn ("}");
      f->putLn ("return " + fact_list + ";");

      add (f);

      Func * m = getModuleInit ();
      m->putLn ("RegisterFactories (" + f->Name + " (), " + quoteStr (Name) + ");");
   }

   if (cond_symbol != "")
   {
       fact_point->putLn ("#ifdef " + cond_symbol);
       fact_point->indent ();
   }

   fact_point->putLn (fact_list + "->add (new " + id + ");");

   if (cond_symbol != "")
   {
       fact_point->unindent ();
       fact_point->putLn ("#endif");
   }
}

/* ---------------------------------------------------------------------- */

EnumType * Module::searchEnum (string id)
{
   Element * item = searchElement (id);

   if (item == NULL)
      return NULL;
   else
      return item -> toEnumType ();
}

/* ---------------------------------------------------------------------- */

Class * Module::searchClass (string id)
{
   Class * result = NULL;
   if (id != "")
   {
      Element * item = getFirstElem ();
      while (item != NULL && result == NULL)
      {
         Class * cls = item->toClass ();
         if (cls != NULL && (cls->Name == id || cls->Alias == id))
            result = cls;
         item = item->getNextElem ();
      }
   }
   return result;
}

Class * Module::findClass (string id)
{
   Class * result = searchClass (id);
   check (result != NULL, "Unknown class: " + id);
   return result;
}

Class * Module::seekClass (string id)
// search class by name or alias in current module or in other modules
{
   Class * cls = NULL;
   if (id != "")
   {
      cls = this->getFirstClass ();
      while (cls != NULL && cls->Name != id && cls->Alias != id)
         cls = cls->getNextClass ();

      if (cls == NULL)
      {
         Project * project = currentProjectOrNull (this);
         if (project != NULL)
         {
            Module * m = project->getFirstModule ();
            while (m != NULL && cls == NULL)
            {
                if (m != this)
                {
                   cls = m->getFirstClass ();
                   while (cls != NULL && cls->Name != id && cls->Alias != id)
                      cls = cls->getNextClass ();
                }
                m = m->getNextModule ();
            }
         }
      }
   }
   return cls;
}

Class * Module::lookupClass (string id)
{
   Class * cls = seekClass (id);
   check (cls != NULL, "Unknown class: " + id);
   return cls;
}

Class * Module::getFirstClass ()
{
   Element * item = this->getFirstElem ();
   Class * result = NULL;
   if (item != NULL)
      result = item->toClass ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toClass ();
   }

   return result;
}

/* ---------------------------------------------------------------------- */

Module * Module::getNextModule ()
{
   Element * item = this->getNextElem ();
   Module * result = NULL;
   if (item != NULL)
      result = item->toModule ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toModule ();
   }

   return result;
}

/* ---------------------------------------------------------------------- */

SimpleDecl * Module::addSimpleDecl (DeclKind kind, string code, int group)
{
   SimpleDecl * item = new SimpleDecl;
   item->Kind = kind;
   item->Group = group;
   if (code == "")
      item->emptyLine ();
   else
      item->putLn (code);
   add (item);
   return item;
}

void Module::defineSymbol (DeclKind kind, string id, int group)
{
   addSimpleDecl (kind, "#define " + id, group);
}

void Module::includeFile (DeclKind kind, string file_name)
{
   addSimpleDecl (kind, "#include " + quoteStr (file_name), include_group);
}

void Module::includeSysFile (DeclKind kind, string file_name)
{
   addSimpleDecl (kind, "#include <" + file_name + ">", include_group);
}

void Module::Using (DeclKind kind, string object_name)
{
   addSimpleDecl (kind, "using " + object_name + ";", using_group);
}

void Module::usingNamespace (DeclKind kind, string namespace_name)
{
   addSimpleDecl (kind, "using namespace " + namespace_name + ";", using_group);
}

/* ---------------------------------------------------------------------- */

void Module::Prog ()
{

}

void Module::completeProg ()
{
   addInfo (intf); // add information about source and target file names
   addInfo (impl);
   addCondition (); // add conditional symbol into header
   addNamespaces (intf); // add automatic or module namespace
   addNamespaces (impl);

   // also used in viewprog, necessary CompleteProg

   // conditional symbols
   if (export_symbols || export_symbols_to_impl)
   {
      Project * prj = currentProject (this);
      CondSymbol * item = prj->input.symbols.first_symbol;
      StringCollection symbols;
      while (item != NULL)
      {
         if (! symbols.contains (item->name))
         {
            symbols.store (item->name, true);

            if (export_symbols)
               defineSymbol (intf, item->name, symbol_group); // add into header file

            if (export_symbols_to_impl)
               defineSymbol (impl, item->name, symbol_group); // add into implementation file
         }
         item = item->next;
      }
   }

   if (! no_class_modif)
   {
      addClasses (); // add forward class declarations
      addConstructors ();
   }

   textIO (this); // add browse, text IO and call methods stencils

   jsonIO (this); // add JSON IO stencils (dependent on Qt)
   xmlIO (this); // add XML IO stencils (dependent on Qt)

   sortItems (); // after generation, before comments

   addComments ();
}

/* --------------------------- modifications ----------------------------- */

void Module::addInfo (DeclKind kind)
{
   string TargetFileName =
      (kind == intf) ? header_name : code_name;

   if (source_file_name != "" && TargetFileName != "")
   {
      string txt = "/* " +
                   get_prog_name () + ": " +
                   removePath (source_file_name) + " -> " +
                   removePath (TargetFileName) +
                   " */";

      addSimpleDecl (kind, "",  title_group); // empty line
      addSimpleDecl (kind, txt, title_group);
      addSimpleDecl (kind, "",  title_group); // empty line
   }
}

/* ---------------------------------------------------------------------- */

void Module::addCondition ()
{
   string id = Name;

   if (header_name != "")
      id = header_name;

   id = removePath (id);
   id = headerIdentifier ("PROG" , id); // !?

   addSimpleDecl (intf, "#ifndef " + id, open_condition_group);
   addSimpleDecl (intf, "#define " + id, open_condition_group);
   addSimpleDecl (intf, "", open_condition_group);

   addSimpleDecl (intf, "", close_condition_group);
   addSimpleDecl (intf, "#endif /* " + id + " */", close_condition_group);
   addSimpleDecl (intf, "", close_condition_group);
}

/* ---------------------------------------------------------------------- */

void Module::addNamespaces (DeclKind kind)
{
   addSimpleDecl (kind, "", open_namespace_group); // empty line

   if (automatic_namespace)
   {
      addSimpleDecl (kind, "OPEN_NAMESPACE", open_namespace_group);
      addSimpleDecl (kind, "", open_namespace_group);
   }

   if (module_namespace != "")
   {
      addSimpleDecl (kind, "namespace " + module_namespace + " {", open_namespace_group);
      addSimpleDecl (kind, "} // end of namespace " + module_namespace, close_namespace_group);
   }

   if (automatic_namespace)
   {
      addSimpleDecl (kind, "", close_namespace_group);
      addSimpleDecl (kind, "CLOSE_NAMESPACE", close_namespace_group);
   }

   if (Comment != "" && kind == intf)
   {
      addSimpleDecl (kind, "", open_namespace_group); // empty line
      addSimpleDecl (kind, "/* " + Comment + " */", open_namespace_group);
   }

   addSimpleDecl (kind, "", open_namespace_group); // empty line
}

/* ---------------------------------------------------------------------- */

void Module::addClasses ()
{
   bool any = false;
   Element * item = this->getFirstElem ();
   while (item != NULL)
   {
      Class * cls = item->toClass ();
      if (cls != NULL && ! cls->no_forward)
      {
         SimpleDecl * decl =
            this->addSimpleDecl (intf, "class " + cls->Name + ";", class_forward_group);
         decl->variety_symbol = cls->variety_symbol;
         decl->Comment = cls->add_comment_before_forward_decl;

         if (cls->add_space_after_forward_decl)
            this->addSimpleDecl (intf, "", class_forward_group); // empty line

         if (cls->add_comment_before_forward_decl != "")
         {
            // also add comment before class definition
            SimpleDecl * decl = new SimpleDecl;
            decl->Kind = intf;
            decl->Group = cls->Group;
            decl->variety_symbol = cls->variety_symbol;
            decl->putLn ("/* " + cls->add_comment_before_forward_decl + " */");
            decl->emptyLine ();
            cls->insertPrevElem (decl);
         }

         any = true;
      }
      item = item->getNextElem ();
   }

   if (any)
      addSimpleDecl (intf, "", class_forward_group); // empty line
}

/* ---------------------------------------------------------------------- */

void Module::addConstructors ()
{
   Element * item = this->getFirstElem ();
   while (item != NULL)
   {
      Class * cls = item->toClass ();
      if (cls != NULL && ! cls->no_automatic_constructor)
      {
         cls->getInit (); // create constructor
      }
      item = item->getNextElem ();
   }
}

/* ---------------------------------------------------------------------- */

void Module::addComments ()
{
   int grp = min_group;

   Element * item = this->getFirstElem ();
   while (item != NULL)
   {
      if (item->Group > grp) // comment only for first item
      {
         grp = item->Group;

         string txt = "";

         if (grp == type_alias_group)
            txt = "type aliases";

         if (txt != "")
         {
            SimpleDecl * p = new SimpleDecl;
            p->Kind = intf;
            p->Group = grp;
            p->access = OnlyComment;
            p->putLn ("// " + txt);
            item->insertPrevElem (p);
         }
         else
         {
            EmptyLineItem * p = new EmptyLineItem;
            item->insertPrevElem (p);
         }
      }
      item = item->getNextElem ();
   }
}

/******************************* ENUM TYPE ********************************/

EnumItem * EnumType::getFirstItem ()
{
   Element * item = this->getFirstElem ();
   EnumItem * result = NULL;
   if (item != NULL)
      result = item->toEnumItem ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toEnumItem ();
   }

   return result;
}

/* ---------------------------------------------------------------------- */

void EnumType::Prog ()
{
   Module * module = currentModule (this);

   bool no_component = false;
   if (module == NULL || ! module->generate_components)
      no_component = true; // set no_component before attribute and list generation

   Element::Prog ();

   if (! no_component)
      generateEnumDescription (this);
}

/******************************* ENUM ITEM ********************************/

EnumItem * EnumItem::getNextItem ()
{
   Element * item = this->getNextElem ();
   EnumItem * result = NULL;
   if (item != NULL)
      result = item->toEnumItem ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toEnumItem ();
   }

   return result;
}

/********************************** CLASS *********************************/

Func * Class::getInit ()
{
   bool experimental = getExperimental (this);

   if (init_func == NULL)
   {
      init_func = new Func;
      init_func->Constructor = true;
      init_func->Name = this->Name;
      init_func->Group = experimental ? attr_group-1 : ctor_group;
      init_func->basic_method = true; // generate when only_basic_methods
      add (init_func);
   }

   Element * e = init_func->getFirstElem ();
   TextItem * t = dynamic_cast <TextItem *> (e);
   if (t != NULL)
      assert (t->Code != " ");

   return init_func;
}

Func * Class::getCleanup ()
{
   if (cleanup_func == NULL)
   {
      cleanup_func = new Func;
      cleanup_func->Virtual = true;
      cleanup_func->Destructor = true;
      cleanup_func->Name = this->Name;
      cleanup_func->Group = dtor_group;
      add (cleanup_func);
   }

   return cleanup_func;
}

/* ---------------------------------------------------------------------- */

Class * Class::parentCls ()
{
   if (parent_cls == NULL && Parent != "")
   {
      Module * module = currentModuleOrNull (this);
      if (module != NULL)
         parent_cls = module->seekClass (Parent);
   }
   return parent_cls;
}

/* ---------------------------------------------------------------------- */

Attr * Class::searchAttr (string id)
{
   Element * item = searchElement (id);

   if (item == NULL)
      return NULL;
   else
      return item->toAttr ();
}

Attr * Class::findAttr (string id)
{
   Attr * result = searchAttr (id);
   check (result != NULL, "Unknown attribute: " + id + " (in class " + Name + ")");
   return result;
}

Attr * Class::seekAttr (string id)
// search attribute in this class or base classes
{
   Class * cls = this;
   Attr * attr = NULL;

   while (attr == NULL && cls != NULL)
   {
      attr = cls->searchAttr (id);
      if (attr == NULL)
         cls = cls->parentCls ();
   }

   return attr;
}

Attr * Class::lookupAttr (string id)
{
   Attr * result = seekAttr (id);
   check (result != NULL, "Unknown attribute: " + id + " (in class " + Name + ")");
   return result;
}

/* ---------------------------------------------------------------------- */

List * Class::searchList (string id)
{
   Element * item = searchElement (id);

   if (item == NULL)
      return NULL;
   else
      return item->toList ();
}

List * Class::findList (string id)
{
   List * result = searchList (id);
   check (result != NULL, "Unknown list: " + id + "(in class " + Name + ")");
   return result;
}

List * Class::seekList (string id)
// search list in this class or base classes
{
   Class * cls = this;
   List * list = NULL;

   while (list == NULL && cls != NULL)
   {
      list = cls->searchList (id);
      if (list == NULL)
         cls = cls->parentCls ();
   }

   return list;
}

List * Class::lookupList (string id)
{
   List * result = seekList (id);
   check (result != NULL, "Unknown list: " + id + "(in class " + Name + ")");
   return result;
}

/* ---------------------------------------------------------------------- */

Func * Class::searchFunc (string id)
{
   Element * item = searchElement (id);

   if (item == NULL)
      return NULL;
   else
      return item->toFunc ();
}

/* ---------------------------------------------------------------------- */

Class * Class::getNextClass ()
{
   Element * item = this->getNextElem ();
   Class * result = NULL;
   if (item != NULL)
      result = item->toClass ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toClass ();
   }

   return result;
}

Attr * Class::getFirstAttr ()
{
   Element * item = this->getFirstElem ();
   Attr * result = NULL;
   if (item != NULL)
      result = item->toAttr ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toAttr ();
   }

   return result;
}

List * Class::getFirstList ()
{
   Element * item = this->getFirstElem ();
   List * result = NULL;
   if (item != NULL)
      result = item->toList ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toList ();
   }

   return result;
}

Func * Class::getFirstSignal ()
{
   Element * item = this->getFirstElem ();

   Func * func = NULL;
   if (item != NULL)
      func = item->toFunc ();

   Func * result = NULL;
   if (func != NULL && func->Signal)
      result = func;

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();

      if (item != NULL)
         func = item->toFunc ();

      if (func != NULL && func->Signal)
         result = func;
   }

   return result;
}

Func * Class::getFirstSlot ()
{
   Element * item = this->getFirstElem ();

   Func * func = NULL;
   if (item != NULL)
      func = item->toFunc ();

   Func * result = NULL;
   if (func != NULL && func->Slot)
      result = func;

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();

      if (item != NULL)
         func = item->toFunc ();

      if (func != NULL && func->Slot)
         result = func;
   }

   return result;
}

/* ---------------------------------------------------------------------- */

void Class::Prog ()
{
   Module * module = currentModule (this);
   if (module == NULL || ! module->generate_components)
      no_component = true; // set no_component before attribute and list generation

   Element::Prog ();

   if (! no_component && ! foreign_decl)
   {
      // if (module != NULL && module->keep_decl_sequence)
      //    GenerateNewDescription (this);
      // else
         generateClassDescription (this);
   }
}

/* ---------------------------------------------------------------------- */

static void setDefaultVarietySymbol (Element * container)
{
  Element * item = container->getFirstElem ();
   while (item != NULL)
   {
      if (item->variety_symbol == "")
          item->variety_symbol = container->variety_symbol; // use class variety_symbol

      setDefaultVarietySymbol (item); // recursion with variety_symbol from current item

      item = item->getNextElem ();
   }
}

void Class::completeProg ()
{
   sortItems (); // after generation, before comments

   addComments ();

   if (variety_symbol != "")
      setDefaultVarietySymbol (this);
}

/* --------------------------- modifications ----------------------------- */

void Class::addComments ()
{
   int grp = min_group;

   Element * item = this->getFirstElem ();
   while (item != NULL)
   {
      if (item->Group > grp) // comment only for first item
      {
         grp = item->Group;

         string txt = "";

         if (grp == view_group)
            txt = "view";

         else if (grp == view_type_group)
            txt = "view types";

         else if (grp == view_changed_flags_group)
            txt = "view 'changed' flags";

         else if (grp == view_updated_flags_group)
            txt = "view 'updated' flags";

         else if (grp == view_connected_flags_group)
            txt = "view 'connected' flags";

         // else if (grp == signal_group)
         //     txt = "view signals";

         else if (grp == slot_group)
            txt = "view slots";

         else if (grp == alias_group)
            txt = "view aliases";

         else if (grp == browse_group)
            txt = "browse";

         else if (grp == gram_location_group)
            txt = "location and notes";

         else if (grp == gram_conv_group)
            txt = "conversion functions";

         else if (grp == gram_code_group)
            txt = "code";

         else if (grp == gram_tree_group)
            txt = "tree";

         else if (grp == gram_send_group)
            txt = "send";

         else if (grp == gram_parse_group)
            txt = "parse";

         else if (grp == gram_syntactic_predicate_group)
            txt = "syntactic predicates";

         else if (grp == gram_semantic_predicate_group)
            txt = "semantic predicates";

         else if (grp == gram_execute_group)
            txt = "execute directives";

         else if (grp == norm_group)
            txt = "user declarations";

         else if (grp == user_group)
            txt = "user declarations";

         else if (grp == init_module_group)
            txt = "module initialization";

         if (txt != "")
         {
            SimpleDecl * p = new SimpleDecl;
            p->Kind = intf;
            p->Group = grp;
            p->access = OnlyComment;
            p->putLn ("// " + txt);
            item->insertPrevElem (p);
         }
      }
      item = item->getNextElem ();
   }
}

/********************************** ATTR **********************************/

Attr * Attr::getNextAttr ()
{
   Element * item = this->getNextElem ();
   Attr * result = NULL;
   if (item != NULL)
      result = item->toAttr ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toAttr ();
   }

   return result;
}

Class * Attr::sourceCls ()
{
   if (source_cls == NULL)
      source_cls = currentClassOrNull (this);
   return source_cls;
}

TypeKind Attr::attrKind ()
{
   return currentProject (this)->getTypeKind (Type);
}

/* ---------------------------------------------------------------------- */

void Attr::add_following (Element * item)
{
   if (following_item == NULL)
      following_item = this;
   following_item->insertNextElem (item);
   following_item = item;

   item->variety_symbol = variety_symbol;
}

void Attr::Prog ()
{
   bool experimental = getExperimental (this);

   if (Comment == "" && Name != "" && ! no_comment && ! experimental)
      Comment = "attribute " + Name;

   // NO Access = OnlyComment;

   // if (Group == 0)
      Group = attr_group;

   if (Comment != "" && ! no_comment && ! experimental)
   {
      SimpleDecl * decl = new SimpleDecl;
      decl->access = OnlyComment;
      decl->Group = Group;
      decl->emptyLine ();
      decl->putLn ("// " + Comment);
      add_following (decl);
   }

   /* variable */

   if (! no_variable)
   {
      Var * v = new Var;
      v->Type = Type;
      v->Name = Name;
      v->Init = Init;
      if (public_variable || only_variable)
         v->access = PublicAccess;
      else if (protected_variable)
         v->access = ProtectedAccess;
      else
         v->access = PrivateAccess;
      v->Group = Group;
      field_id = v->Name;
      add_following (v);
   }

   if (! only_variable)
   {
      /* get */

      if ( ! no_get_method)
      {
          Func * f = new Func ();
          f->Name = joinIdentifiers ("get", Name);
          f->Type = Type;
          f->access = PublicAccess;
          f->Group = Group;
          f->Virtual = virtual_get;
          f->Abstract = abstract_get;
          add_following (f);
          get_id = f->Name;
          get_func = f;

          if (no_get_impl)
          {
             f->no_impl = true;
          }
          else if (! abstract_get)
          {
             f->Inline = ! no_inline_get;

             f->Location (get_point);
             f->modif_point = get_point;

             if (! empty_get_impl)
             {
                f->put ("return " + Name + ";");
             }
          }

      }

      /* set */

      if (! no_set_method)
      {
         Func * f = new Func ();
         f->Name = joinIdentifiers ("set", Name);
         f->addParam (Type, "p_value");
         f->access = PublicAccess;
         f->Group = Group;
         f->Virtual = virtual_set;
         f->Abstract = abstract_set;
         add_following (f);
         set_id = f->Name;
         set_func = f;

         if (no_set_impl)
         {
            f->no_impl = true;
             f->skip_impl = true;
         }
         else if (! abstract_set)
         {
            f->Inline = inline_set;
            if (! empty_set_impl)
            {
                bool braces = false;

               if (no_compare || no_variable)
               {
               }
               else if (compare_flag_name != "" && compare_flag_value != "")
               {
                  f->putLn ("if (! (" + compare_flag_name + " & " + compare_flag_value + ") || p_value != " + Name + ")");
                  braces = true;
               }
               else
               {
                  f->putLn ("if (p_value != " + Name + ")");
                  braces = true;
               }

               if (braces)
               {
                  f->putLn ("{");
                  f->indent ();
               }

               if (! no_variable)
                  f->putLn (Name + " = p_value;");

               f->Location (set_point);
               f->modif_point = set_point;

               if (braces)
               {
                  f->unindent ();
                  f->putLn ("}");
               }

            }
         }
      }
   }
}

/********************************** LIST **********************************/

List * List::getNextList ()
{
   Element * item = this->getNextElem ();
   List * result = NULL;
   if (item != NULL)
      result = item->toList ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toList ();
   }

   return result;
}

Class * List::sourceCls ()
{
   if (source_cls == NULL)
      source_cls = currentClassOrNull (this);
   return source_cls;
}

Class * List::elementCls ()
{
   if (element_cls == NULL && element != "")
   {
      Module * module = currentModuleOrNull (this);
      if (module != NULL)
         element_cls = module->seekClass (element);
   }
   return element_cls;
}

/* ---------------------------------------------------------------------- */

const string private_mark = "private_";

void List::setNames ()
{
   if (Get == "")
      Get = "get";

   if (First == "")
      First = "first" + Suffix1;

   if (Last == "")
      Last = "last" + Suffix1;

   if (Prev == "")
      Prev = "prev" + Suffix1;

   if (Next == "")
      Next = "next" + Suffix1;

   if (Up == "")
      Up = "up" /* + Suffix1 */;

   if (InsertFirst == "")
      InsertFirst = "insertFirst" + Suffix1;

   if (InsertLast == "")
      InsertLast = "insertLast" + Suffix1;

   if (InsertBefore == "")
   {
      if (Suffix1 == "")
         InsertBefore = "insertBefore";
      else
         InsertBefore = "insertPrev" + Suffix1;
   }

   if (InsertAfter == "")
   {
      if (Suffix1 == "")
         InsertAfter = "insertAfter";
      else
         InsertAfter = "insertNext" + Suffix1;
   }

   if (Link == "")
      Link = "link" + Suffix1;

   if (Remove == "")
      Remove = "remove" + Suffix1;

   if (RemoveAll == "")
      RemoveAll = "removeAll" + Suffix2;

   // source_cls and element_cls are not set
   Class * cls = currentClassOrNull (this);
   Module * module = currentModuleOrNull (cls);
   if (module != NULL && module->generate_components && cls != NULL && ! cls->no_component && cls->Name != "Element") // !?
   {
      component_list = true;
   }

   // necessary for redefine_list
   first_id = Prefix + First;
   last_id = Prefix + Last;
   prev_id = Prefix + Prev;
   next_id = Prefix + Next;
   up_id = Prefix + Up;

   get_first_id = Prefix + Get + firstUpperCase (First);
   get_last_id = Prefix + Get + firstUpperCase (Last);
   get_prev_id = Prefix + Get + firstUpperCase (Prev);
   get_next_id = Prefix + Get + firstUpperCase (Next);
   get_up_id = Prefix + Get + firstUpperCase (Up);

   ins_first_id = Prefix + InsertFirst;
   ins_last_id = Prefix + InsertLast;
   ins_before_id = Prefix + InsertBefore;
   ins_after_id = Prefix + InsertAfter;
   link_id = Prefix + Link;
   remove_id = Prefix + Remove;
   remove_all_id = Prefix + RemoveAll;
}

/* ---------------------------------------------------------------------- */

void List::add_following (Element * item)
{
   if (following_item == NULL)
      following_item = this;
   following_item->insertNextElem (item);
   following_item = item;
}

void List::add_element (Element * item)
{
   if (element_cls == source_cls)
      add_following (item);
   else
      element_cls->add (item);
}

void List::addField (Class * cls, string result_type, string field_name, Var * & prod)
{
   Var * v = new Var;
   v->Type = result_type + ptr_mark;
   v->Name = field_name;
   v->Init = "NULL";
   v->access = public_variables ? PublicAccess : PrivateAccess;
   v->Group = Group;

   prod = v;

   if (cls == source_cls)
      add_following (v);
   else
      add_element (v);
}

void List::getFunc (Class * cls, string result_type, string func_name, string field_name, Func * & prod)
{
   Func * f = new Func;
   f->Type = result_type + ptr_mark;
   f->Name = func_name;
   f->access = PublicAccess;
   f->Group = Group;
   if (cls == source_cls)
      add_following (f);
   else
      add_element (f);

   prod = f;

   f->Inline = true;
   f->put ("return " + field_name + ";");
}

/* ---------------------------------------------------------------------- */

void List::linkMethod ()
{
   Func * f = new Func ();
   f->Name = link_id;
   f->addParam (element + ptr_mark, "item");
   f->addParam (element + ptr_mark, "bef");
   f->addParam (element + ptr_mark, "aft");
   f->Virtual = virtual_link;
   f->access = ProtectedAccess;
   f->Group = Group;
   add_following (f);

   link_func = f;

   f->putLn ("assert (this != NULL);");
   f->putLn ("assert (item != NULL);");
   f->putLn ("");
   f->putLn ("assert (item->" + prev_id + " == NULL);");
   f->putLn ("assert (item->" + next_id + " == NULL);");
   f->putLn ("assert (item->" + up_id + " == NULL);");
   f->putLn ("");

   f->Location (check_point);
   f->emptyLine ();

   if (source_cls == element_cls)
   {
      f->putLn (element + " * tmp = this;");
      f->putLn ("while (tmp != NULL)");
      f->putLn ("{");
      f->incIndent ();
         f->putLn ("assert (tmp != item);");
         f->putLn ("tmp = tmp->" + up_id + ";");
      f->decIndent ();
      f->putLn ("}");
      f->putLn ("");
   }

   f->putLn ("item->" + prev_id + " = bef;");
   f->putLn ("item->" + next_id + " = aft;");
   f->putLn ("item->" + up_id + " = this;");
   f->putLn ("");
   f->putLn ("if (bef == NULL)");
      f->incIndent ();
      f->putLn ("this->" + first_id + " = item;");
      f->decIndent ();
   f->putLn ("else");
      f->incIndent ();
      f->putLn ("bef->" + next_id + " = item;");
      f->decIndent ();
   f->putLn ("");
   f->putLn ("if (aft == NULL)");
      f->incIndent ();
      f->putLn ("this->" + last_id + " = item;");
      f->decIndent ();
   f->putLn ("else");
      f->incIndent ();
      f->putLn ("aft->" + prev_id + " = item;");
      f->decIndent ();

   f->emptyLine ();
   f->Location (ins_point);
   f->modif_point = ins_point;

   if (insert_notification != "")
   {
      f->emptyLine ();
      f->putLn ("item->" + insert_notification + " ();");
   }

   if (attach_notification != "")
   {
      f->emptyLine ();
      f->putLn (attach_notification + " (item);");
   }

   #if 0
   f->putLn ("");
   f->putLn ("item->OnInsert ();");
   f->putLn ("");
   f->putLn ("ConnectSubitem (item);");
   f->putLn ("");
   f->putLn ("if (aft == NULL)");
   f->putLn ("{");
      f->incIndent ();
      f->putLn ("if (Tree_Observer != NULL)");
         f->incIndent ();
         f->putLn ("Tree_Observer->Tree_OnInsertLast (item);");
         f->decIndent ();
      f->decIndent ();
   f->putLn ("}");
   f->putLn ("else");
   f->putLn ("{");
      f->incIndent ();
      f->putLn ("if (aft->Tree_Observer != NULL)");
         f->incIndent ();
         f->putLn ("aft->Tree_Observer->Tree_OnInsertPrev (item);");
         f->decIndent ();
      f->decIndent ();
   f->putLn ("}");
   #endif
}

void List::insertMethods ()
{
   Access access = component_list ? PrivateAccess : PublicAccess;
   string prefix = component_list ? private_mark : "";
   int grp = Group;

   /* InsertFirst */

   Func * f = new Func ();
   f->Name = prefix + ins_first_id;
   f->addParam (element + ptr_mark, "item");
   f->access = access;
   f->Group = grp;
   add_following (f);

   ins_first_func = f;

   f->putLn ("assert (this != NULL);");
   f->putLn (link_id + " (item, NULL, " + first_id + ");");

   /* InsertLast */

   f = new Func ();
   f->Name = prefix + ins_last_id;
   f->addParam (element + ptr_mark, "item");
   f->access = access;
   f->Group = grp;
   add_following (f);

   ins_last_func = f;

   f->putLn ("assert (this != NULL);");
   f->putLn (link_id + " (item, " + last_id + ", NULL);");

   if (! combined)
   {
      /* InsertBefore */

      f = new Func ();
      f->Name = prefix + ins_before_id;
      f->addParam (element + ptr_mark, "item");
      f->access = access;
      f->Group = grp;
      add_element (f);

      ins_before_func = f;

      f->putLn ("assert (this != NULL);");
      f->putLn ("assert (" + up_id + " != NULL);");
      f->putLn (up_id + "->" + link_id + " (item, " + prev_id + ", this);");

      /* InsertAfter */

      f = new Func ();
      f->Name = prefix + ins_after_id;
      f->addParam (element + ptr_mark, "item");
      f->access = access;
      f->Group = grp;
      add_element (f);

      ins_after_func = f;

      f->putLn ("assert (this != NULL);");
      f->putLn ("assert (" + up_id + " != NULL);");
      f->putLn (up_id + "->" + link_id + " (item, this, " + next_id + ");");
   }
}

void List::removeMethods ()
{
   // TAccess access = component_list ? PrivateAccess : PublicAccess;
   // string prefix = component_list ? private_mark : "";
   int grp = Group;

   if (combined)
   {
      rem_point = new StatList;
      final_rem_point = new StatList;
   }

   if (! combined)
   {
      /* Remove */

      Func * f = new Func ();
      f->Name = remove_id;
      f->Virtual = virtual_remove;
      f->access = PublicAccess;
      f->Group = grp;
      if (element_cls == source_cls)
         add_following (f);
      else
         element_cls->add (f);

      remove_func = f;

      f->putLn ("assert (this != NULL);");
      f->putLn ("assert (" + up_id + " != NULL);");

      f->emptyLine ();
      f->Location (rem_point);
      f->modif_point = rem_point;

      if (detach_notification != "")
      {
         f->emptyLine ();
         f->putLn (up_id + "->" + detach_notification + " (this);");
      }

      if (remove_notification != "")
      {
         f->emptyLine ();
         f->putLn (remove_notification + " ();");
      }

      f->emptyLine ();
      #if 0
      f->putLn (up_id + "->DisconnectSubitem (this);");
      f->putLn ("");
      f->putLn ("OnRemove ();");
      f->putLn ("");
      f->putLn ("if (Tree_Observer != NULL)");
         f->incIndent ();
         f->putLn ("Tree_Observer->Tree_OnRemove ();");
         f->decIndent ();
      f->putLn ("");
      #endif
      f->putLn ("if (" + prev_id + " == NULL)");
         f->incIndent ();
         f->putLn (up_id + "->" + first_id + " = " + next_id + ";");
         f->decIndent ();
      f->putLn ("else");
         f->incIndent ();
         f->putLn (prev_id + "->" + next_id + " = " + next_id + ";");
         f->decIndent ();
      f->putLn ("");
      f->putLn ("if (" + next_id + " == NULL)");
         f->incIndent ();
         f->putLn (up_id + "->" + last_id + " = " + prev_id + ";");
         f->decIndent ();
      f->putLn ("else");
         f->incIndent ();
         f->putLn (next_id + "->" + prev_id + " = " + prev_id + ";");
         f->decIndent ();
      f->putLn ("");
      f->putLn (prev_id + " = NULL;");
      f->putLn (next_id + " = NULL;");
      f->putLn (up_id + " = NULL;");

      f->emptyLine ();
      f->Location (final_rem_point);

      if (component_list)
      {
         f->emptyLine ();
         f->putLn ("if (getUpperElem () != NULL)");
         f->incIndent ();
         f->putLn ("removeElem ();");
         f->decIndent ();
      }
   }

   /* RemoveAll */

   Func * f = new Func ();
   f->Name = remove_all_id;
   f->access = PublicAccess;
   f->Group = grp;
   add_following (f);

   remove_all_func = f;

   f->putLn (element + " * p = " + first_id + ";");
   f->putLn ("while (p != NULL)");
   f->putLn ("{");
      f->incIndent ();
      f->putLn (element + " * t = p;");
      f->putLn ("p = p->" + next_id + ";");
      f->putLn ("t->" + remove_id + " ();");
      if (! no_release)
         f->putLn ("delete t;");
      f->decIndent ();
   f->putLn ("}");
   f->putLn (first_id + " = NULL;");
   f->putLn (last_id + " = NULL;");
}

/* ---------------------------------------------------------------------- */

void List::componentLinkMethod ()
{
   if (source_cls->component_link == NULL)
   {
      Func * f = new Func ();
      f->Name = "linkElem";
      f->addParam ("Element" + ptr_mark, "item");
      f->addParam ("Element" + ptr_mark, "bef");
      f->addParam ("Element" + ptr_mark, "aft");
      f->Virtual = true;
      f->access = ProtectedAccess;
      f->Group = Group;
      add_following (f);

      string above = source_cls->Parent;
      if (above == "")
         above = "Element"; // !?
      f->putLn (above + "::" + f->Name + " (item, bef, aft);");
      f->emptyLine ();

      source_cls->component_link = f;
   }

   Func * f =  source_cls->component_link;

   f->putLn ("{");
   f->indent ();

      f->putLn (element + " * tmp_item = dynamic_cast <" + element + " *> (item);");
      f->putLn ("if (tmp_item != NULL)");
      f->putLn ("{");
      f->indent ();
         f->putLn ("if (bef == NULL)");
         f->indent ();
            f->putLn (private_mark + ins_first_id + " (tmp_item);");
         f->unindent ();
         f->putLn ("else if (aft == NULL)");
         f->indent ();
            f->putLn (private_mark + ins_last_id + " (tmp_item);");
         f->unindent ();
         f->putLn ("else");
         f->putLn ("{");
         f->indent ();
            f->putLn (element + " * tmp_bef = dynamic_cast <" + element + " *> (bef);");
            f->putLn ("if (tmp_bef != NULL)");
            f->indent ();
               f->putLn ("tmp_bef->" + private_mark + ins_after_id + " (tmp_item);");
            f->unindent ();
            f->putLn ("else");
            f->putLn ("{");
            f->indent ();
               f->putLn ("TElement * tmp = aft;"); // !?
               f->putLn (element + " * tmp_aft = dynamic_cast <" + element + " *> (tmp);");
               f->putLn ("while (tmp_aft == NULL && tmp != NULL)");
               f->putLn ("{");
               f->indent ();
                  f->putLn ("tmp = tmp->getNextElem ();"); // !?
                  f->putLn ("tmp_aft = dynamic_cast <" + element + " *> (tmp);");
               f->unindent ();
               f->putLn ("}");
               f->putLn ("if (tmp_aft != NULL)");
               f->indent ();
                  f->putLn ("tmp_aft->" + private_mark + ins_before_id + " (tmp_item);");
               f->unindent ();
               f->putLn ("else");
               f->indent ();
                  f->putLn (private_mark + ins_last_id + " (tmp_item);");
               f->unindent ();
            f->unindent ();
            f->putLn ("}");
         f->unindent ();
         f->putLn ("}");
      f->unindent ();
      f->putLn ("}");

   f->unindent ();
   f->putLn ("}");

   f->emptyLine ();
}

void List::componentRemoveMethod ()
{
   if (element_cls->component_remove == NULL)
   {
      Func * f = new Func ();
      f->Name = "removeElem";
      f->access = PublicAccess;
      f->Group = Group;
      add_element (f);

      string above = element_cls->Parent;
      if (above == "")
         above = "Element";
      f->putLn (above + "::" + f->Name + " ();");

      element_cls->component_remove = f;
   }

   Func * f = element_cls->component_remove;

   f->putLn ("if (" + up_id + " != NULL)");
   f->indent ();
      f->putLn (remove_id + " ();");
   f->unindent ();
}

void List::componentInsertMethods ()
{
   Func * f = new Func ();
   f->Name = ins_first_id;
   f->addParam (element + ptr_mark, "item");
   f->access = PublicAccess;
   f->Group = Group;
   add_following (f);
   f->putLn ("insertFirstElem (item);");

   f = new Func ();
   f->Name = ins_last_id;
   f->addParam (element + ptr_mark, "item");
   f->access = PublicAccess;
   f->Group = Group;
   add_following (f);
   f->putLn ("insertLastElem (item);");

   f = new Func ();
   f->Name = ins_before_id;
   f->addParam (element + ptr_mark, "item");
   f->access = PublicAccess;
   f->Group = Group;
   add_element (f);
   f->putLn ("insertPrevElem (item);");

   f = new Func ();
   f->Name = ins_after_id;
   f->addParam (element + ptr_mark, "item");
   f->access = PublicAccess;
   f->Group = Group;
   add_element (f);
   f->putLn ("insertNextElem (item);");
}

/* ---------------------------------------------------------------------- */

void List::Prog ()
{
   if (Comment == "")
   {
      Comment = "list";
      if (Name != "")
        Comment = Comment + " "  + Name;
   }

   // if (Group == 0)
      Group = list_group;

   // TModule * module = CurrentModuleOrNull (SourceCls ());
   // if (module != NULL && module->keep_decl_sequence)
   //    Group = norm_group;

   access = OnlyComment;

   sourceCls (); // set source_cls varaiable
   elementCls (); // set element_cls varaiable

   assert (source_cls != NULL);
   assert (element_cls != NULL);

   if (source_cls != element_cls && Comment != "")
   {
      SimpleDecl * decl = new SimpleDecl;
      decl->access = OnlyComment;
      decl->Group = Group;
      decl->putLn ("// " + Comment);
      add_element (decl);
   }

   if (Comment != "")
   {
      SimpleDecl * decl = new SimpleDecl;
      decl->access = OnlyComment;
      decl->Group = Group;
      decl->emptyLine ();
      decl->putLn ("// " + Comment);
      add_following (decl);
   }

   string source = "";
   if (source_cls != NULL)
      source = source_cls->Name;

   if (source_cls != NULL && element_cls != NULL)
   {
      addField (source_cls, element, first_id, first_var);
      addField (source_cls, element, last_id,  last_var);

      if (! restricted)
      {
         addField (element_cls, element, prev_id, prev_var);
         addField (element_cls, element, next_id, next_var);
      }
      addField (element_cls, source, up_id, up_var);

      linkMethod ();

      if (! public_variables)
      {
         getFunc (source_cls, element, get_first_id, first_id, get_first_func);
         getFunc (source_cls, element, get_last_id,  last_id, get_last_func);

         if (! restricted)
         {
            getFunc (element_cls, element, get_prev_id, prev_id, get_prev_func);
            getFunc (element_cls, element, get_next_id, next_id, get_next_func);
         }
         getFunc (element_cls, source, get_up_id, up_id, get_up_func);
      }

      insertMethods ();

      removeMethods ();

      if (component_list)
      {
         componentLinkMethod ();
         componentRemoveMethod ();
         componentInsertMethods ();
      }

      if (element_cls != source_cls)
      {
         SimpleDecl * d = new SimpleDecl;
         d->Kind = intf;
         d->Group = friend_group;
         d->putLn ("friend class " + element + ";");
         source_cls->add (d);

         d = new SimpleDecl;
         d->Kind = intf;
         d->Group = friend_group;
         d->putLn ("friend class " + source + ";");
         element_cls->add (d);
      }

      Func * f = source_cls->getCleanup ();
      f->putLn (remove_all_id + " ();");

      f = element_cls->getCleanup ();
      f->putLn ("if (" + up_id + " != NULL)");
      f->incIndent ();
      f->putLn (remove_id + " ();");
      f->decIndent ();
   }

   if (source_cls == NULL)
      putLn ("#error List without enclosing class");

   if (element_cls == NULL)
      putLn ("#error Missing " + element + "class");
}

/************************** VARIABLE DECLARATION **************************/

/************************** FUNCTION DECLARATION **************************/

Func::~Func ()
{
   delete Params;
}

Class * Func::sourceCls ()
{
   if (source_cls == NULL)
      source_cls = currentClassOrNull (this);
   return source_cls;
}

void Func::addParam (string type, string name, string init)
{
   ParamDecl * param = new ParamDecl;
   param->Type = type;
   param->Name = name;
   param->Init = init;
   Params->add (param);
}

void Func::addParams (ParamList * list)
{
   Element * item = list->getFirstElem ();
   while (item != NULL)
   {
      Element * nxt = item->getNextElem ();
      item->removeElem ();

      if (item->toParamDecl () != NULL) // !?
         Params->add (item);

      item = nxt;
   }
}

ExprList * Func::getParamExpr ()
{
   // create list of values
   ExprList * result = new ExprList;

   ParamDecl * param = getFirstParam ();
   bool any = false;
   while (param != NULL)
   {
      if (any) result->put (", ");
      any = true;

      result->put (param->Name);
      param = param->getNextParam ();
   }

   return result;
}

Func * Func::getNextSignal ()
{
   Element * item = this->getNextElem ();

   Func * func = NULL;
   if (item != NULL)
      func = item->toFunc ();

   Func * result = NULL;
   if (func != NULL && func->Signal)
      result = func;

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();

      if (item != NULL)
         func = item->toFunc ();

      if (func != NULL && func->Signal)
         result = func;
   }

   return result;
}

Func * Func::getNextSlot ()
{
   Element * item = this->getNextElem ();

   Func * func = NULL;
   if (item != NULL)
      func = item->toFunc ();

   Func * result = NULL;
   if (func != NULL && func->Slot)
      result = func;

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();

      if (item != NULL)
         func = item->toFunc ();

      if (func != NULL && func->Slot)
         result = func;
   }

   return result;
}

/************************** PARAMETER DECLARATION *************************/

ParamDecl * Func::getFirstParam ()
{
   assert (this != NULL);
   ParamDecl * result = NULL;

   if (Params != NULL)
   {
      Element * item = Params->getFirstElem ();
      if (item != NULL)
         result = item->toParamDecl ();

      while (result == NULL && item != NULL)
      {
         item = item->getNextElem ();
         if (item != NULL)
            result = item->toParamDecl ();
      }
   }

   // !?
   if (result == NULL)
   {
      Element * item = getFirstElem ();
      if (item != NULL)
         result = item->toParamDecl ();

      while (result == NULL && item != NULL)
      {
         item = item->getNextElem ();
         if (item != NULL)
            result = item->toParamDecl ();
      }
   }

   return result;
}

ParamDecl * ParamDecl::getNextParam ()
{
   Element * item = this->getNextElem ();
   ParamDecl * result = NULL;
   if (item != NULL)
      result = item->toParamDecl ();

   while (result == NULL && item != NULL)
   {
      item = item->getNextElem ();
      if (item != NULL)
         result = item->toParamDecl ();
   }

   return result;
}

/***************************** EXPORT SYMBOL ******************************/

void ExportSymbol::updateProg ()
{
   Module * module = currentModule (this);
   Project * prj = currentProject (module);
   string id = Name;
   if (prj->isDefinedSymbol (id))
      module->defineSymbol (intf, id, symbol_group);
}

/******************************** TYPE INFO *******************************/

void TypeInfo::basicProg ()
{
   currentProject (this) -> storeTypeKind (Name, kind);
}

/********************************** STYLE *********************************/

Class * Styler::sourceCls ()
{
   if (source_cls == NULL)
      source_cls = currentClassOrNull (this);
   return source_cls;
}

Class * Styler::elementCls ()
{
   if (element_cls == NULL && element != "")
   {
      Module * module = currentModuleOrNull (this);
      if (module != NULL)
         element_cls = module->seekClass (element);
   }
   return element_cls;
}

void Styler::attrMethod ()
{
   string attr_cache = "attr_cache";

   Var * v = new Var;
   v->Name = attr_cache;
   v->Type = "Dictionary < TAttr *, TStyle * >";
   v->access = PrivateAccess;
   v->Group = follow_group;
   sourceCls ()->add (v);

   Func * f = new Func ();
   f->Name = "getAttrStyle";
   f->addParam ("Attr" + ptr_mark, "item");
   f->Type = "Style" + ptr_mark;
   f->access = PublicAccess;
   f->Group = follow_group;
   sourceCls ()->add (f);

   f->putLn ("assert (item != NULL);");
   f->putLn ("TStyle * result = " + attr_cache + ".get (item);");

   f->putLn ("if (result == NULL)");
   f->putLn ("{");
   f->indent ();

      f->putLn ("TElement * elem = item->getFirstElem ();");
      f->putLn ("while (result == NULL && elem != NULL)");
      f->putLn ("{");
      f->indent ();

         f->putLn ("if (elem.isStyle ())");
         f->indent ();
            f->putLn ("result = elem;");
         f->unindent ();

         f->putLn ("elem = elem->getNextElem ();");

      f->unindent ();
      f->putLn ("}");

      f->putLn ("if (result != NULL)");
      f->indent ();
         f->putLn (attr_cache + ".store (item, result);");
      f->unindent ();


   f->unindent ();
   f->putLn ("}");

   f->putLn ("return result;");
}

void Styler::Prog ()
{
    attrMethod ();
}

/********************************** STORE *********************************/

Class * findClass (EnvInput & io, Module * module, string id)
{
   Class * cls = module->seekClass (id);

   if (cls == NULL)
      io.error ("Cannot find class " + id);

   return cls;
}

Attr * findAttr (EnvInput & io, Class * cls, string id)
{
   Attr * attr = cls->seekAttr (id);

   if (attr == NULL)
      io.error ("Cannot find attribute " + id);

   return attr;
}

string seekNameAttr (EnvInput & io, Class * cls)
{
   string result = "";

   while (result == "" && cls != NULL)
   {
      result = cls->NameAttr;

      if (result == "")
         cls = cls->parentCls ();
   }

   return result;
}

string findNameAttr (EnvInput & io, Class * cls)
{
   string result = seekNameAttr (io, cls);

   if (result == "")
      io.error ("Cannot find 'name' attribute");

   return result;
}

bool isDerived (Module * module, Class * above, Class * below)
{
   bool result = false;
   bool cont = true;

   while (cont)
   {
      if (above == below)
      {
         result = true;
         cont = false;
      }
      else if (below->Parent == "" || below->Parent == "Element") // !? !!
      {
         result = false;
         cont = false;
      }
      else
      {
         below = below->parentCls ();
      }
   }

   return result;
}

List * findCompatibleList (EnvInput & io, Module * module, Class * cls, Class * item_cls) // !?
{
   // debug ("FIND " + item_cls->Name + " list in " + cls->Name);
   List * result = NULL;

   while (cls != NULL)
   {
      List * list = cls->getFirstList ();
      while (list != NULL && result == NULL)
      {
         // debug ("CHECK " + item_cls->Name + " is derived from " + list->element);
         if (isDerived (module, findClass (io, module, list->element), item_cls))
            result = list;
         list = list->getNextList ();
      }

      if (cls->Parent != "" && cls->Parent != "Element") // !? !!
         cls = cls->parentCls ();
      else
         cls = NULL;
   }

   return result;
}

void setAttr (Func * func, string var, Class * cls, string attr_name, string value)
{
   Attr * attr = cls->seekAttr (attr_name);
   if (attr != NULL)
   {

      if (attr->Type == "string") // !?
         value = quoteStr (value);

      if (attr->only_variable)
         func->putLn (var + "->" + attr->Name + " = " + value + ";");
      else
         func->putLn (var + "->set" + attr->Name + " (" + value + ");");
    }
    else
    {
       func->putLn (var + "->" + attr_name + " = " + quoteStr (value) + ";");
    }
}

string readItem (EnvInput & io, Module * module, Func * func, int & cnt, string cls_name, bool top_level, string obj_name)
{
   Class * source_cls = findClass (io, module, cls_name);

   string title = "";
   if (! io.isSeparator (';') && ! io.isSeparator ('{'))
   {
      title = io.readValue ();
   }

   string var = "";
   func->emptyLine ();
   if (top_level)
   {
      if (title != "")
      {
         var = title;
      }
      else
      {
         if (obj_name != "")
            var = obj_name;
         else
            io.error ("Missing identifier for top level 'store' item");
      }
      func->putLn (var + " = new " + source_cls->Name + ";");

      Var * v = new Var;
      v->Type = source_cls->Name + ptr_mark;
      v->Name = var;
      v->Init = "NULL";
      v->Extern = true;
      module->add (v);
   }
   else
   {
      cnt ++;
      var = "p" + IntToStr (cnt);
      func->putLn (source_cls->Name + ptr_mark + " " + var + " = new " + source_cls->Name + ";");
   }

   if (title != "")
   {
      if (top_level)
      {
         string attr_ident = seekNameAttr (io, source_cls);
         if (attr_ident != "")
            setAttr (func, var, source_cls, attr_ident, title);
      }
      else
      {
         string attr_ident = findNameAttr (io, source_cls);
         setAttr (func, var, source_cls, attr_ident, title);
      }
   }

   if (io.isSeparator ('{'))
   {
      io.checkSeparator ('{');

      while (! io.isSeparator ('}'))
      {
          string id = io.readIdent ();
          if (io.isSeparator ('='))
          {
             io.getToken ();
             string val = io.readValue ();
             io.checkSeparator (';');

             setAttr (func, var, source_cls, id, val);
          }
          else
          {
             // debug ("ReadItem searching compatible list for " + id);
             List * list = findCompatibleList (io, module, source_cls, findClass (io, module, id));
             string list_var = readItem (io, module, func, cnt, id, false, "");
             // debug ("ReadItem list variable " + list_var);
             // debug ("ReadItem list " + NumToHex (list));
             // debug ("ReadItem list name " + list->Name);
             func->putLn (var + "->" + list->ins_last_id + " (" + list_var + ");");
          }
      }

      io.checkSeparator ('}');

      if (io.isSeparator (';'))
         io.getToken ();
   }
   else io.checkSeparator (';');

   return var;
}

void readStore (EnvInput & io, Module * target_module)
{
   io.checkIdent ("store");
   string store_name = io.readValue (); // !?

   bool CallInit = false;
   bool MultiIdent = false;
   string RegistrationFunction = "";
   string RegistrationMethod = "";

   Func * func = new Func;
   func->Name = store_name + "_init";
   target_module->add (func);

   int cnt = 0;

   io.checkSeparator ('{');

   while (! io.isSeparator ('}'))
   {
      string id = io.readIdent ();
      // debug ("ReadStore " + id);
      if (! io.isSeparator ('='))
      {
         string var = readItem (io, target_module, func, cnt, id, /* MultiIdent */ true, store_name);

         if (RegistrationMethod != "")
         {
            func->emptyLine ();
            func->putLn (var + "->" +  RegistrationMethod + " ();");
         }

         if (RegistrationFunction != "")
         {
            func->emptyLine ();
            func->putLn (RegistrationFunction + " (" + var + ");");
         }

         func->emptyLine ();
      }
      else
      {
         io.getToken ();

         if (id == "CallInit") // call initialization function during module initialization
         {
             CallInit = io.readBool ();
         }
         else if (id == "MultiIdent") // assign variable identifiers for top level structures
         {
             MultiIdent = io.readBool ();
         }
         else if (id == "RegistrationFunction") // identifier for registration function
         {
             RegistrationFunction = io.readValue ();
         }
         else if (id == "RegistrationMethod") // identifier for registration method
         {
             RegistrationMethod = io.readValue ();
         }
         else io.error ("Unknown parameter name: " + id);

         io.checkSeparator (';');
      }
   }

   func->noEmptyLine (); // suppress last emoty line

   io.checkSeparator ('}');
   if (io.isSeparator (';'))
      io.getToken ();

   if (CallInit)
   {
       Func * init_func = target_module->getModuleInit ();
       init_func->putLn (func->Name + " ();");
   }
}

/******************************* UTILITIES ********************************/

Project * currentProjectOrNull (Element * elem)
{
    while (elem != NULL && elem->toProject () == NULL)
       elem = elem->getUpperElem ();

    if (elem == NULL)
       return NULL;
    else
       return elem->toProject ();
}

Project * currentProject (Element * elem)
{
   Project * result = currentProjectOrNull (elem);
   assert (result != NULL);
   return result;
}

Module * currentModuleOrNull (Element * elem)
{
    while (elem != NULL && elem->toModule () == NULL)
       elem = elem->getUpperElem ();

    if (elem == NULL)
       return NULL;
    else
       return elem->toModule ();
}

Module * currentModule (Element * elem)
{
   Module * result = currentModuleOrNull (elem);
   assert (result != NULL);
   return result;
}

Class * currentClassOrNull (Element * elem)
{
    while (elem != NULL && elem->toClass () == NULL)
       elem = elem->getUpperElem ();

    if (elem == NULL)
       return NULL;
    else
       return elem->toClass ();
}

Class * currentClass (Element * elem)
{
   Class * result = currentClassOrNull (elem);
   assert (result != NULL);
   return result;
}

/* ---------------------------------------------------------------------- */

string exprToStr (Expr * expr)
{
   TextOutput io;
   io.open ("");
   io.suspendEol ();
   expr->writeCode (io);
   return io.toString ();
}

string exprListToStr (ExprList * expr)
{
   TextOutput io;
   io.open ("");
   expr->writeCode (io);
   return io.toString ();
}

Expr * joinExpr (Expr * a, string txt, Expr * b)
{
   Expr * result = new Expr;
   result->add (a);

   TextItem * item = new TextItem;
   item->Code = " " + txt + " ";
   result->add (item);

   result->add (b);
   return result;
}

string paramListToStr (ParamList * list)
{
   TextOutput io;
   io.open ("");
   list->writeCode (io);
   return io.toString ();
}

void insertMethod (Class * cls, Access acs, Func * func, int group)
{
   if (group != 0)
      func->Group = group;
   func->access = acs;
   cls->add (func);
}


/********************** CLASS, ATTR and LIST FIELDS ***********************/

void split_name (string name, string & a, string & b)
{
   size_t inx = name.rfind ('.');
   if (inx == string::npos)
   {
      a = "";
      b = name;
   }
   else
   {
      a = Head (name, inx);
      b = Tail (name, inx+1);
   }
}

Class * find_class (Field * field, string name)
{
   Element * elem = field->getUpperElem ();

   // !? try local module, then project
   Class * cls = currentModule (elem) -> seekClass (name);

   check_field (cls != NULL, "Unknown class: " + name, field);

   return cls;
}

void ClassPtrField::Prepare ()
{
   if (getValue () == NULL) // if not set by TGtkList !?
   {
      string txt = getText ();

      if (txt.find ('*') != string::npos)
         check_field (false, "Class identifier expected: " + txt + " (not pointer)", this);

      if (txt == "")
         setValue (NULL);
      else
      {
         Class * cls = find_class (this, txt);
         setValue (cls);
      }
   }
}

void AttrPtrField::Prepare ()
{
   string txt = getText ();
   if (txt == "")
      setValue (NULL);
   else
   {
      string a;
      string b;
      split_name (txt, a, b);

      Class * cls = find_class (this, a);

      Attr * attr = cls->seekAttr (b);
      check_field (attr != NULL, "Unknown attribute: " + txt, this);

      setValue (attr);
   }
}

void ListPtrField::Prepare ()
{
   string txt = getText ();

   if (getValue () == NULL) // if not set by TGtkList !?
   {
      if (txt == "")
         setValue (NULL);
      else
      {
         string a;
         string b;
         split_name (txt, a, b);

         Class * cls = find_class (this, a);

         List * list = cls->seekList (b); // search in current class or in base classes
         check_field (list != NULL, "Unknown list: " + txt, this);

         setValue (list);
      }
   }
}

CLOSE_NAMESPACE
