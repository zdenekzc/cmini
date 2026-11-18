
/* stencil.cc */

#include "stencil.h"

#include "str.h"

OPEN_NAMESPACE

/********************************** CODE **********************************/

/* type classification (according to the attribute type name) */

static bool isBoolType (string type)
{
   return type == "bool";
}

static bool isIntType (string type)
{
   return type == "short" || type == "int" || type == "long";
}

static bool isStringType (string type)
{
   return type == "string";
}

static string baseTypeName (string type)
// type name without the pointer mark ("TItem *" -> "TItem")
{
   return trim (removeSuffixChar (type, '*'));
}

static bool isClassPointer (Module * module, string type)
{
   return type != baseTypeName (type) &&
          module->searchClass (baseTypeName (type)) != NULL;
}

/* ---------------------------------------------------------------------- */

static string propertyMethod (string type)
// PropertyIfc method for the given attribute type
{
   if (isBoolType (type))
      return "addBool";

   if (isIntType (type))
      return "addInt";

   if (isStringType (type))
      return "addStdString";

   if (type == "QString")
      return "addString";

   if (type == "QColor")
      return "addColor";

   return "";
}

static string browseMethod (string type)
// TreeIfc method for the given attribute type
{
   if (isBoolType (type))
      return "browseBool";

   if (isIntType (type))
      return "browseInt";

   if (isStringType (type))
      return "browseStdString";

   if (type == "QString")
      return "browseString";

   return "";
}

static string readMethod (Module * module, string type)
// Lexer method for reading the given attribute type
{
   if (isClassPointer (module, type))
      return "read" + baseTypeName (type);

   if (isBoolType (type))
      return "readBool";

   if (isIntType (type))
      return "readInt";

   if (isStringType (type))
      return "readString";

   if (type == "QString")
      return "readString";

   return "";
}

static string writeMethod (Module * module, string type)
// Output method for writing the given attribute type
{
   if (isClassPointer (module, type))
      return "write" + baseTypeName (type);

   if (isBoolType (type))
      return "writeBool";

   if (isIntType (type))
      return "writeInt";

   if (isStringType (type))
      return "writeString";

   if (type == "QString")
      return "writeString";

   return "";
}

static string fieldMethod (string type)
// field kind for the JSON and XML conversion
{
   if (isBoolType (type))
      return "bool";

   if (isIntType (type))
      return "int";

   if (type == "float" || type == "double")
      return "double";

   if (type == "QString")
      return "QString";

   if (isStringType (type))
      return "string";

   if (type == "QColor")
      return "QColor";

   return "";
}

/* ---------------------------------------------------------------------- */

static void storeAttr (Func * func, Attr * attr, string data, string expr)
// store the expression into the attribute (set method or direct assignment)
{
   if (attr->set_id != "")
      func->putLn (data + "->" + attr->set_id + " (" + expr + ");");
   else
      func->putLn (data + "->" + attr->Name + " = " + expr + ";");
}

static string loadAttr (Attr * attr, string data)
// expression for reading the attribute value (get method or direct member)
{
   if (attr->get_id != "")
      return data + "->" + attr->get_id + " ()";
   return data + "->" + attr->Name;
}

/********************************* BROWSE *********************************/

void classProperties (Class * cls)
{
   Module * module = currentModuleOrNull (cls);

   Func * func = new Func;
   func->Name = "properties";
   func->addParam ("PropertyIfc" + ptr_mark, "prop");
   func->access = PublicAccess;
   func->Group = browse_group;
   if (cls->Parent == "")
      func->Virtual = true; // base method of the browse interface
   cls->add (func);

   if (cls->Parent != "")
      func->putLn (cls->Parent + "::properties (prop);");

   Attr * attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "" && ! attr->no_variable)
      {
         string type = attr->Type;

         EnumType * enum_type = module->searchEnum (type);
         if (enum_type != NULL)
         {
            // enumeration values are converted into strings

            func->putLn ("prop->addStdString (" + quoteStr (attr->Name) + ", conv_" +
                         type + " (" + attr->Name + "));");
         }
         else if (isClassPointer (module, type))
         {
            func->putLn ("prop->addPointer (" + quoteStr (attr->Name) + ", " +
                         attr->Name + ");");
         }
         else
         {
            string meth = propertyMethod (type);
            if (meth != "")
               func->putLn ("prop->" + meth + " (" + quoteStr (attr->Name) + ", " +
                            attr->Name + ");");
            else
               func->putLn ("prop->addUnknown (" + quoteStr (attr->Name) + ");");
         }
      }
      attr = attr->getNextAttr ();
   }
}

void classSubitems (Class * cls)
{
   Module * module = currentModuleOrNull (cls);

   Func * func = new Func;
   func->Name = "subitems";
   func->addParam ("TreeIfc" + ptr_mark, "tree");
   func->access = PublicAccess;
   func->Group = browse_group;
   if (cls->Parent == "")
      func->Virtual = true; // base method of the browse interface
   cls->add (func);

   if (cls->Parent != "")
      func->putLn (cls->Parent + "::subitems (tree);");

   // scalar attributes

   Attr * attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "" && ! attr->no_variable)
      {
         string meth = browseMethod (attr->Type);
         if (meth != "")
            func->putLn ("tree->" + meth + " (" + quoteStr (attr->Name) + ", " +
                         attr->Name + ");");
      }
      attr = attr->getNextAttr ();
   }

   // pointer attributes as tree subitems

   attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "" && ! attr->no_variable &&
          isClassPointer (module, attr->Type))
      {
         func->putLn ("if (" + attr->Name + " != NULL)");
         func->incIndent ();
         func->putLn ("tree->add (" + quoteStr (attr->Name) + ", " + attr->Name + ");");
         func->decIndent ();
      }
      attr = attr->getNextAttr ();
   }

   // list members as tree subitems

   List * list = cls->getFirstList ();
   while (list != NULL)
   {
      if (list->Name != "" &&
          list->first_id != "" &&
          list->next_id != "" &&
          list->elementCls () != NULL)
      {
         func->putLn ("for (" + list->element + " * subitem = " + list->first_id +
                      "; subitem != NULL; subitem = subitem->" + list->next_id + ")");
         func->incIndent ();
         func->putLn ("tree->add (" + quoteStr ("item") + ", subitem);");
         func->decIndent ();
      }
      list = list->getNextList ();
   }
}

/* ---------------------------------------------------------------------- */

static void addConvFunction (Module * module, EnumType * type)
// conversion function from an enum value to its name
{
   Func * func = new Func;
   func->Type = "string";
   func->Name = "conv_" + type->Name;
   func->addParam (type->Name, "value");
   func->Group = norm_group;

   func->putLn ("switch (value)");
   func->putLn ("{");
   func->incIndent ();

   EnumItem * item = type->getFirstItem ();
   while (item != NULL)
   {
      func->putLn ("case " + item->Name + " :");
      func->incIndent ();
      func->putLn ("return " + quoteStr (item->Name) + ";");
      func->decIndent ();
      item = item->getNextItem ();
   }

   func->putLn ("default :");
   func->incIndent ();
   func->putLn ("return " + quoteStr ("?") + ";");
   func->decIndent ();

   func->decIndent ();
   func->putLn ("}");

   module->add (func);
}

static void addConvFunctions (Module * module)
// conversion functions for all enumeration types
{
   Element * item = module->getFirstElem ();
   while (item != NULL)
   {
      EnumType * type = item->toEnumType ();

      if (type != NULL && type->Name != "")
         addConvFunction (module, type);

      item = item->getNextElem ();
   }
}

/* ---------------------------------------------------------------------- */

static void ifcMethod (Class * cls, string name, string type)
// pure virtual method of a browse interface
{
   Func * func = new Func;
   func->Name = name;
   func->addParam ("string", "name");
   if (type != "")
      func->addParam (type, "value");
   func->access = PublicAccess;
   func->Virtual = true;
   func->Abstract = true;
   func->no_impl = true; // no body for a pure virtual method
   func->Group = browse_group;
   cls->add (func);
}

static void addBrowseInterfaces (Module * module)
// PropertyIfc and TreeIfc interface classes for the browse stencil
{
   Class * prop_ifc = new Class;
   prop_ifc->Name = "PropertyIfc";
   prop_ifc->Group = browse_group;
   prop_ifc->interface_class = true;
   module->add (prop_ifc);

   ifcMethod (prop_ifc, "addBool", "bool");
   ifcMethod (prop_ifc, "addInt", "int");
   ifcMethod (prop_ifc, "addStdString", "string");
   ifcMethod (prop_ifc, "addString", "QString");
   ifcMethod (prop_ifc, "addColor", "QColor");
   ifcMethod (prop_ifc, "addPointer", "void *");
   ifcMethod (prop_ifc, "addUnknown", "");

   Class * tree_ifc = new Class;
   tree_ifc->Name = "TreeIfc";
   tree_ifc->Group = browse_group;
   tree_ifc->interface_class = true;
   module->add (tree_ifc);

   ifcMethod (tree_ifc, "browseBool", "bool");
   ifcMethod (tree_ifc, "browseInt", "int");
   ifcMethod (tree_ifc, "browseStdString", "string");
   ifcMethod (tree_ifc, "browseString", "QString");
   ifcMethod (tree_ifc, "add", "void *");
}

/******************************** TEXT IO *********************************/

Func * readText (Class * cls)
{
   Module * module = currentModuleOrNull (cls);
   string cls_name = cls->Name;

   Func * func = new Func;
   func->Type = cls_name + ptr_mark;
   func->Name = "read" + cls_name;
   func->addParam ("Lexer" + ptr_mark, "io");
   func->Group = norm_group;

   func->putLn (cls_name + " * data = new " + cls_name + ";");
   func->emptyLine ();

   func->putLn ("io->checkIdent (" + quoteStr (cls_name) + ");");
   func->putLn ("io->checkSeparator (" + quoteChr ('{') + ");");
   func->emptyLine ();

   func->putLn ("while (! io->isSeparator (" + quoteChr ('}') + "))");
   func->putLn ("{");
   func->incIndent ();

   bool any = false;
   Attr * attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "")
      {
         string head;
         if (any)
            head = "else ";

         func->putLn (head + "if (io->isIdent (" + quoteStr (attr->Name) + "))");
         func->putLn ("{");
         func->incIndent ();

         func->putLn ("io->nextToken ();");
         func->putLn ("io->checkSeparator (" + quoteChr ('=') + ");");

         string meth = readMethod (module, attr->Type);
         if (meth != "")
         {
            storeAttr (func, attr, "data", "io->" + meth + " ()");

            func->putLn ("io->checkSeparator (" + quoteChr (';') + ");");
         }

         func->decIndent ();
         func->putLn ("}");

         any = true;
      }
      attr = attr->getNextAttr ();
   }

   string line = "io->error (" + quoteStr ("Unknown field name") + ");";
   if (any)
      line = "else " + line;
   func->putLn (line);

   func->decIndent ();
   func->putLn ("}");
   func->emptyLine ();

   func->putLn ("io->checkSeparator (" + quoteChr ('}') + ");");
   func->emptyLine ();

   func->putLn ("if (io->isSeparator (" + quoteChr (';') + "))");
   func->incIndent ();
   func->putLn ("io->nextToken ();");
   func->decIndent ();
   func->emptyLine ();

   func->putLn ("return data;");

   return func;
}

Func * writeText (Class * cls)
{
   Module * module = currentModuleOrNull (cls);
   string cls_name = cls->Name;

   Func * func = new Func;
   func->Name = "write" + cls_name;
   func->addParam ("Output" + ptr_mark, "io");
   func->addParam (cls_name + ptr_mark, "data");
   func->Group = norm_group;

   func->putLn ("io->send (" + quoteStr (cls_name) + ");");
   func->putLn ("io->send (" + quoteStr ("{") + ");");
   func->emptyLine ();

   Attr * attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "" && ! attr->no_variable)
      {
         string meth = writeMethod (module, attr->Type);
         if (meth != "")
         {
            string value = loadAttr (attr, "data");

            func->putLn ("io->send (" + quoteStr (attr->Name) + ");");
            func->putLn ("io->send (" + quoteStr ("=") + ");");
            func->putLn ("io->" + meth + " (" + value + ");");
            func->putLn ("io->send (" + quoteStr (";") + ");");
            func->emptyLine ();
         }
      }
      attr = attr->getNextAttr ();
   }

   func->putLn ("io->send (" + quoteStr ("}") + ");");
   func->putLn ("io->send (" + quoteStr (";") + ");");

   return func;
}

/****************************** CALL METHODS ******************************/

static bool readableParams (Module * module, Func * method)
// all method parameters must be readable by the lexer
{
   ParamDecl * param = method->getFirstParam ();
   while (param != NULL)
   {
      if (readMethod (module, param->Type) == "")
         return false;
      param = param->getNextParam ();
   }
   return true;
}

static void callOneMethod (Func * func, Func * method, Module * module, bool any)
{
   string head;
   if (any)
      head = "else ";

   func->putLn (head + "if (io->isIdent (" + quoteStr (method->Name) + "))");
   func->putLn ("{");
   func->incIndent ();

   func->putLn ("io->nextToken ();");
   func->putLn ("io->checkSeparator (" + quoteChr ('(') + ");");

   bool first = true;
   string params = "";
   ParamDecl * param = method->getFirstParam ();
   while (param != NULL)
   {
      func->emptyLine ();
      if (! first)
         func->putLn ("io->checkSeparator (" + quoteChr (',') + ");");

      string meth = readMethod (module, param->Type);
      func->putLn (param->Type + " param_" + param->Name + " = io->" + meth + " ();");

      if (first)
         params = "param_" + param->Name;
      else
         params = params + ", param_" + param->Name;
      first = false;

      param = param->getNextParam ();
   }
   if (! first)
      func->emptyLine ();

   func->putLn ("io->checkSeparator (" + quoteChr (')') + ");");
   func->putLn ("io->checkSeparator (" + quoteChr (';') + ");");
   func->emptyLine ();

   func->putLn ("data->" + method->Name + " (" + params + ");");

   func->decIndent ();
   func->putLn ("}");
}

Func * callMethods (Class * cls)
{
   Module * module = currentModuleOrNull (cls);

   Func * func = new Func;
   func->Name = "call" + cls->Name;
   func->addParam ("Lexer" + ptr_mark, "io");
   func->addParam (cls->Name + ptr_mark, "data");
   func->Group = norm_group;

   bool any = false;
   Element * item = cls->getFirstElem ();
   while (item != NULL)
   {
      Func * method = item->toFunc ();
      if (method != NULL &&
          ! method->Constructor &&
          ! method->Destructor &&
          method->access == PublicAccess &&
          method->Name != "" &&
          method->Name != "properties" &&
          method->Name != "subitems" &&
          readableParams (module, method))
      {
         callOneMethod (func, method, module, any);
         any = true;
      }
      item = item->getNextElem ();
   }

   string line = "io->error (" + quoteStr ("Unknown field name") + ");";
   if (any)
      line = "else " + line;
   func->putLn (line);

   return func;
}

/********************************** JSON **********************************/

static string jsonCheckMethod (string meth)
// QJsonValue check method for the given field kind
{
   if (meth == "bool")
      return "isBool";

   if (meth == "int" || meth == "double")
      return "isDouble";

   return "isString"; // QString, string, QColor
}

static string jsonReadExpr (string meth, string value)
// conversion of the JSON value into the attribute value
{
   if (meth == "bool")
      return value + ".toBool ()";

   if (meth == "int")
      return value + ".toInt ()";

   if (meth == "double")
      return value + ".toDouble ()";

   if (meth == "QString")
      return value + ".toString ()";

   if (meth == "string")
      return value + ".toString ().toStdString ()";

   if (meth == "QColor")
      return "QColor (" + value + ".toString ())";

   return "";
}

static string jsonWriteExpr (string meth, string value)
// conversion of the attribute value into a JSON value
{
   if (meth == "string")
      return "QString::fromStdString (" + value + ")";

   if (meth == "QColor")
      return value + ".name ()";

   return value; // bool, int, double, QString
}

/* ---------------------------------------------------------------------- */

static void jsonList (Func * func, List * list, bool read)
// JSON code for the list member (array of member objects)
{
   if (list->Name == "" ||
       list->elementCls () == NULL ||
       list->get_first_func == NULL ||
       list->get_next_func == NULL ||
       list->ins_last_func == NULL)
      return;

   string array_name = list->Name + "_array";

   if (read)
   {
      string value = "io.value (" + quoteStr (list->Name) + ")";

      func->putLn ("if (io.contains (" + quoteStr (list->Name) + ") && " +
                   value + ".isArray ())");
      func->putLn ("{");
      func->incIndent ();

      func->putLn ("QJsonArray " + array_name + " = " + value + ".toArray ();");
      func->putLn ("for (int inx = 0; inx < " + array_name + ".size (); inx++)");
      func->putLn ("{");
      func->incIndent ();
      func->putLn (list->element + " * item = new " + list->element + ";");
      func->putLn ("read" + list->element + " (" + array_name + " [inx].toObject (), item);");
      func->putLn ("data->" + list->ins_last_id + " (item);");
      func->decIndent ();
      func->putLn ("}");

      func->decIndent ();
      func->putLn ("}");
      func->emptyLine ();
   }
   else
   {
      func->putLn ("{");
      func->incIndent ();

      func->putLn ("QJsonArray " + array_name + ";");
      func->putLn ("for (" + list->element + " * item = data->" + list->get_first_id +
                   " (); item != NULL; item = item->" + list->get_next_id + " ())");
      func->putLn ("{");
      func->incIndent ();
      func->putLn ("QJsonObject item_obj;");
      func->putLn ("write" + list->element + " (item_obj, item);");
      func->putLn (array_name + ".append (item_obj);");
      func->decIndent ();
      func->putLn ("}");

      func->putLn ("io [" + quoteStr (list->Name) + "] = " + array_name + ";");

      func->decIndent ();
      func->putLn ("}");
      func->emptyLine ();
   }
}

Func * readJson (Class * cls)
// readXxx (const QJsonObject & io, Cls * data)
{
   Func * func = new Func;
   func->Type = cls->Name + ptr_mark;
   func->Name = "read" + cls->Name;
   func->addParam ("const QJsonObject &", "io");
   func->addParam (cls->Name + ptr_mark, "data");
   func->Group = norm_group;

   Attr * attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "" && ! attr->no_variable)
      {
         string meth = fieldMethod (attr->Type);
         if (meth != "")
         {
            string value = "io.value (" + quoteStr (attr->Name) + ")";

            func->putLn ("if (io.contains (" + quoteStr (attr->Name) + ") && " +
                         value + "." + jsonCheckMethod (meth) + " ())");
            func->putLn ("{");
            func->incIndent ();
            storeAttr (func, attr, "data", jsonReadExpr (meth, value));
            func->decIndent ();
            func->putLn ("}");
            func->emptyLine ();
         }
      }
      attr = attr->getNextAttr ();
   }

   List * list = cls->getFirstList ();
   while (list != NULL)
   {
      jsonList (func, list, true);
      list = list->getNextList ();
   }

   func->putLn ("return data;");

   return func;
}

Func * writeJson (Class * cls)
// writeXxx (QJsonObject & io, Cls * data)
{
   Func * func = new Func;
   func->Name = "write" + cls->Name;
   func->addParam ("QJsonObject &", "io");
   func->addParam (cls->Name + ptr_mark, "data");
   func->Group = norm_group;

   Attr * attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "" && ! attr->no_variable)
      {
         string meth = fieldMethod (attr->Type);
         if (meth != "")
         {
            func->putLn ("io [" + quoteStr (attr->Name) + "] = " +
                         jsonWriteExpr (meth, loadAttr (attr, "data")) + ";");
            func->emptyLine ();
         }
      }
      attr = attr->getNextAttr ();
   }

   List * list = cls->getFirstList ();
   while (list != NULL)
   {
      jsonList (func, list, false);
      list = list->getNextList ();
   }

   return func;
}

/* file level functions for the first class */

static void jsonFileFunctions (Module * module, Class * cls)
{
   string cls_name = cls->Name;

   /* readJsonFile */

   Func * func = new Func;
   func->Type = cls_name + ptr_mark;
   func->Name = "readJsonFile";
   func->addParam ("QString", "fileName");
   func->Group = norm_group;

   func->putLn (cls_name + " * result = NULL;");
   func->emptyLine ();
   func->putLn ("QFile f (fileName);");
   func->putLn ("if (f.open (QFile::ReadOnly))");
   func->putLn ("{");
   func->incIndent ();
   func->putLn ("QJsonDocument doc = QJsonDocument::fromJson (f.readAll ());");
   func->putLn ("QJsonObject obj = doc.object ();");
   func->emptyLine ();
   func->putLn ("result = new " + cls_name + ";");
   func->putLn ("read" + cls_name + " (obj, result);");
   func->decIndent ();
   func->putLn ("}");
   func->putLn ("else");
   func->putLn ("{");
   func->incIndent ();
   func->putLn ("QMessageBox::warning (NULL, " + quoteStr ("Open File Error") + ", " +
                quoteStr ("Cannot read file: ") + " + fileName);");
   func->decIndent ();
   func->putLn ("}");
   func->emptyLine ();
   func->putLn ("return result;");

   module->add (func);

   /* writeJsonFile */

   func = new Func;
   func->Name = "writeJsonFile";
   func->addParam ("QString", "fileName");
   func->addParam (cls_name + ptr_mark, "data");
   func->Group = norm_group;

   func->putLn ("QFile f (fileName);");
   func->putLn ("if (f.open (QFile::WriteOnly))");
   func->putLn ("{");
   func->incIndent ();
   func->putLn ("QJsonObject obj;");
   func->putLn ("write" + cls_name + " (obj, data);");
   func->putLn ("QJsonDocument doc (obj);");
   func->putLn ("f.write (doc.toJson ());");
   func->decIndent ();
   func->putLn ("}");
   func->putLn ("else");
   func->putLn ("{");
   func->incIndent ();
   func->putLn ("QMessageBox::warning (NULL, " + quoteStr ("Save File Error") + ", " +
                quoteStr ("Cannot write file: ") + " + fileName);");
   func->decIndent ();
   func->putLn ("}");

   module->add (func);

   /* openJsonFile */

   func = new Func;
   func->Type = cls_name + ptr_mark;
   func->Name = "openJsonFile";
   func->addParam ("QWidget" + ptr_mark, "window");
   func->Group = norm_group;

   func->putLn (cls_name + " * result = NULL;");
   func->emptyLine ();
   func->putLn ("QString fileName = QFileDialog::getOpenFileName (window, " +
                quoteStr ("Open JSON file") + ");");
   func->putLn ("if (fileName != \"\")");
   func->incIndent ();
   func->putLn ("result = readJsonFile (fileName);");
   func->decIndent ();
   func->emptyLine ();
   func->putLn ("return result;");

   module->add (func);

   /* saveJsonFile */

   func = new Func;
   func->Name = "saveJsonFile";
   func->addParam ("QWidget" + ptr_mark, "window");
   func->addParam (cls_name + ptr_mark, "data");
   func->Group = norm_group;

   func->putLn ("QString fileName = QFileDialog::getSaveFileName (window, " +
                quoteStr ("Save JSON file") + ");");
   func->putLn ("if (fileName != \"\")");
   func->incIndent ();
   func->putLn ("writeJsonFile (fileName, data);");
   func->decIndent ();

   module->add (func);
}

/********************************** XML **********************************/

static string xmlReadExpr (string meth, string value)
// conversion of the XML attribute value into the attribute value
{
   if (meth == "bool")
      return value + ".toString () == \"true\"";

   if (meth == "int")
      return value + ".toString ()"; // int needs the ok-check

   if (meth == "double")
      return value + ".toString ().toDouble ()";

   if (meth == "QString")
      return value + ".toString ()";

   if (meth == "string")
      return value + ".toString ().toStdString ()";

   if (meth == "QColor")
      return "QColor (" + value + ".toString ())";

   return "";
}

static string xmlWriteExpr (string meth, string value)
// conversion of the attribute value into an XML attribute value
{
   if (meth == "bool")
      return value + " ? \"true\" : \"false\"";

   if (meth == "int" || meth == "double")
      return "QString::number (" + value + ")";

   if (meth == "string")
      return "QString::fromStdString (" + value + ")";

   if (meth == "QColor")
      return value + ".name ()";

   return value; // QString
}

/* ---------------------------------------------------------------------- */

static bool xmlList (Func * func, List * list, bool read)
// XML code for the list member (child elements)
// returns true when the code has been generated
{
   if (list->Name == "" ||
       list->elementCls () == NULL ||
       list->get_first_func == NULL ||
       list->get_next_func == NULL ||
       list->ins_last_func == NULL)
      return false;

   if (read)
   {
      func->putLn ("while (reader.readNextStartElement ())");
      func->putLn ("{");
      func->incIndent ();

      func->putLn ("if (reader.name ().toString () == " + quoteStr (list->element) + ")");
      func->putLn ("{");
      func->incIndent ();
      func->putLn (list->element + " * item = read" + list->element + " (reader);");
      func->putLn ("data->" + list->ins_last_id + " (item);");
      func->decIndent ();
      func->putLn ("}");
      func->putLn ("else");
      func->incIndent ();
      func->putLn ("reader.skipCurrentElement ();");
      func->decIndent ();

      func->decIndent ();
      func->putLn ("}");
      func->emptyLine ();

      return true;
   }
   else
   {
      func->putLn ("for (" + list->element + " * item = data->" + list->get_first_id +
                   " (); item != NULL; item = item->" + list->get_next_id + " ())");
      func->incIndent ();
      func->putLn ("write" + list->element + " (writer, item);");
      func->decIndent ();
      func->emptyLine ();

      return true;
   }

   return false;
}

Func * readXml (Class * cls)
// readXxx (QXmlStreamReader & reader)
{
   Func * func = new Func;
   func->Type = cls->Name + ptr_mark;
   func->Name = "read" + cls->Name;
   func->addParam ("QXmlStreamReader &", "reader");
   func->Group = norm_group;

   func->putLn (cls->Name + " * data = new " + cls->Name + ";");
   func->emptyLine ();
   func->putLn ("QXmlStreamAttributes a = reader.attributes ();");
   func->emptyLine ();

   Attr * attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "" && ! attr->no_variable)
      {
         string meth = fieldMethod (attr->Type);
         if (meth != "")
         {
            string value = "a.value (" + quoteStr (attr->Name) + ")";

            func->putLn ("if (a.hasAttribute (" + quoteStr (attr->Name) + "))");
            func->putLn ("{");
            func->incIndent ();

            if (meth == "int")
            {
               func->putLn ("bool ok = false;");
               func->putLn ("int val = " + value + ".toString ().toInt (& ok);");
               func->putLn ("if (ok)");
               func->incIndent ();
               storeAttr (func, attr, "data", "val");
               func->decIndent ();
            }
            else
               storeAttr (func, attr, "data", xmlReadExpr (meth, value));

            func->decIndent ();
            func->putLn ("}");
            func->emptyLine ();
         }
      }
      attr = attr->getNextAttr ();
   }

   List * list = cls->getFirstList ();
   bool consumed = false; // the list loop consumes the element's end tag
   while (list != NULL)
   {
      if (xmlList (func, list, true))
         consumed = true;
      list = list->getNextList ();
   }

   if (! consumed)
   {
      // no list loop — consume the remaining content and the end tag

      func->putLn ("while (reader.readNextStartElement ())");
      func->incIndent ();
      func->putLn ("reader.skipCurrentElement ();");
      func->decIndent ();
      func->emptyLine ();
   }

   func->putLn ("return data;");

   return func;
}

Func * writeXml (Class * cls)
// writeXxx (QXmlStreamWriter & writer, Cls * data)
{
   Func * func = new Func;
   func->Name = "write" + cls->Name;
   func->addParam ("QXmlStreamWriter &", "writer");
   func->addParam (cls->Name + ptr_mark, "data");
   func->Group = norm_group;

   func->putLn ("writer.writeStartElement (" + quoteStr (cls->Name) + ");");

   Attr * attr = cls->getFirstAttr ();
   while (attr != NULL)
   {
      if (attr->Name != "" && ! attr->no_variable)
      {
         string meth = fieldMethod (attr->Type);
         if (meth != "")
            func->putLn ("writer.writeAttribute (" + quoteStr (attr->Name) + ", " +
                         xmlWriteExpr (meth, loadAttr (attr, "data")) + ");");
      }
      attr = attr->getNextAttr ();
   }

   List * list = cls->getFirstList ();
   while (list != NULL)
   {
      xmlList (func, list, false);
      list = list->getNextList ();
   }

   func->putLn ("writer.writeEndElement ();");

   return func;
}

/* file level functions for the first class */

static void xmlFileFunctions (Module * module, Class * cls)
{
   string cls_name = cls->Name;

   /* readXmlFile */

   Func * func = new Func;
   func->Type = cls_name + ptr_mark;
   func->Name = "readXmlFile";
   func->addParam ("QString", "fileName");
   func->Group = norm_group;

   func->putLn (cls_name + " * result = NULL;");
   func->emptyLine ();
   func->putLn ("QFile file (fileName);");
   func->putLn ("if (file.open (QFile::ReadOnly | QFile::Text))");
   func->putLn ("{");
   func->incIndent ();
   func->putLn ("QXmlStreamReader reader (& file);");
   func->emptyLine ();
   func->putLn ("if (reader.readNextStartElement () &&");
   func->putLn ("    reader.name ().toString () == " + quoteStr (cls_name) + ")");
   func->incIndent ();
   func->putLn ("result = read" + cls_name + " (reader);");
   func->decIndent ();
   func->decIndent ();
   func->putLn ("}");
   func->emptyLine ();
   func->putLn ("return result;");

   module->add (func);

   /* writeXmlFile */

   func = new Func;
   func->Name = "writeXmlFile";
   func->addParam ("QString", "fileName");
   func->addParam (cls_name + ptr_mark, "data");
   func->Group = norm_group;

   func->putLn ("QFile file (fileName);");
   func->putLn ("if (file.open (QFile::WriteOnly | QFile::Text))");
   func->putLn ("{");
   func->incIndent ();
   func->putLn ("QXmlStreamWriter writer (& file);");
   func->putLn ("writer.setAutoFormatting (true);");
   func->emptyLine ();
   func->putLn ("writer.writeStartDocument ();");
   func->putLn ("write" + cls_name + " (writer, data);");
   func->putLn ("writer.writeEndDocument ();");
   func->decIndent ();
   func->putLn ("}");

   module->add (func);

   /* openXmlFile */

   func = new Func;
   func->Type = cls_name + ptr_mark;
   func->Name = "openXmlFile";
   func->addParam ("QWidget" + ptr_mark, "window");
   func->Group = norm_group;

   func->putLn (cls_name + " * result = NULL;");
   func->emptyLine ();
   func->putLn ("QString fileName = QFileDialog::getOpenFileName (window, " +
                quoteStr ("Open XML file") + ");");
   func->putLn ("if (fileName != \"\")");
   func->incIndent ();
   func->putLn ("result = readXmlFile (fileName);");
   func->decIndent ();
   func->emptyLine ();
   func->putLn ("return result;");

   module->add (func);

   /* saveXmlFile */

   func = new Func;
   func->Name = "saveXmlFile";
   func->addParam (cls_name + ptr_mark, "data");
   func->addParam ("QWidget" + ptr_mark, "window");
   func->Group = norm_group;

   func->putLn ("QString fileName = QFileDialog::getSaveFileName (window, " +
                quoteStr ("Save XML file") + ");");
   func->putLn ("if (fileName != \"\")");
   func->incIndent ();
   func->putLn ("writeXmlFile (fileName, data);");
   func->decIndent ();

   module->add (func);
}

/********************************* DRIVER *********************************/

void textIO (Module * module)
{
   addConvFunctions (module); // enum to string conversion functions
   addBrowseInterfaces (module); // PropertyIfc and TreeIfc interface classes

   Class * cls = module->getFirstClass ();
   while (cls != NULL)
   {
      if (! cls->foreign_decl && ! cls->interface_class &&
          cls->getFirstElem () != NULL)
      {
         classSubitems (cls);
         classProperties (cls);

         module->add (readText (cls));
         module->add (writeText (cls));
         module->add (callMethods (cls));
      }
      cls = cls->getNextClass ();
   }
}

/* ---------------------------------------------------------------------- */

static void addInclude (Module * module, DeclKind kind, string file_name)
// add an include file declaration
{
   module->addSimpleDecl (kind, "#include " + file_name, include_group);
}

void jsonIO (Module * module)
{
   // Qt includes

   addInclude (module, intf, "<QString>");
   addInclude (module, intf, "<QJsonObject>");
   addInclude (module, intf, "<QWidget>");

   addInclude (module, impl, "<QJsonDocument>");
   addInclude (module, impl, "<QJsonArray>");
   addInclude (module, impl, "<QFile>");
   addInclude (module, impl, "<QColor>");
   addInclude (module, impl, "<QMessageBox>");
   addInclude (module, impl, "<QFileDialog>");

   Class * cls = module->getFirstClass ();
   Class * first = NULL;
   while (cls != NULL)
   {
      if (! cls->foreign_decl && ! cls->interface_class &&
          cls->getFirstElem () != NULL)
      {
         if (first == NULL)
            first = cls;

         module->add (readJson (cls));
         module->add (writeJson (cls));
      }
      cls = cls->getNextClass ();
   }

   if (first != NULL)
      jsonFileFunctions (module, first);
}

void xmlIO (Module * module)
{
   // Qt includes (QFile, QColor, QString come with the JSON IO)

   addInclude (module, intf, "<QXmlStreamReader>");
   addInclude (module, intf, "<QXmlStreamWriter>");

   Class * cls = module->getFirstClass ();
   Class * first = NULL;
   while (cls != NULL)
   {
      if (! cls->foreign_decl && ! cls->interface_class &&
          cls->getFirstElem () != NULL)
      {
         if (first == NULL)
            first = cls;

         module->add (readXml (cls));
         module->add (writeXml (cls));
      }
      cls = cls->getNextClass ();
   }

   if (first != NULL)
      xmlFileFunctions (module, first);
}

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE
