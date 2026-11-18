
/* code-desc.cc */

// OPEN_NAMESPACE

/***************************** WRITE FACTORY ******************************/

void setParameter (Func * func, string set_method, string value)
{
   if (value != "")
      func->putLn (set_method + " (" + quoteStr (value) + ");");
}

void setField (Func * func, string param_name, string value)
{
   if (value != "")
      func->putLn (param_name + " = " + value + "_Field;");
}

Class * createFactory (Class * cls,
                        Class * element_cls,
                        string factory_name,
                        string factory_base_class,
                        Module * target_module)
{
   /* factory class */

   Class * desc = new Class;
   desc->Name = factory_name;
   desc->Parent = factory_base_class;
   desc->Group = factory_group;
   desc->no_component = true; // no recursion

   /* factory initialization */

   {
      Func * func = desc->getInit ();

      setParameter (func, "setName", cls->Name);
      setParameter (func, "setParent", cls->Parent);

      setParameter (func, "setAlias", cls->Alias);
      setParameter (func, "setAliasGroup", cls->AliasGroup);
   }

   /* module initialization */

   if (target_module != NULL)
   {
      if (target_module->factory_list)
      {
         target_module->addFactoryToInitList (desc->Name, cls->variety_symbol);
      }
      else
      {
         Func * f = target_module->getModuleInit ();
         f->putLn ("RegisterFactory (new " + desc->Name + ");");
      }
   }

   /* NewElement */

   {
      Func * func = new Func;
      func->Name = "NewElement";
      func->Type = "Element" + ptr_mark;
      func->access = ProtectedAccess;
      func->Virtual = true;
      func->Group = super_group; // !?
      desc->add (func);

      func->putLn ("return new " + element_cls->Name + " ();");
   }

   return desc;
}

void generateAttrDescription (Class * cls)
{
   bool experimental = getExperimental (cls);

   Attr * attr = cls->getFirstAttr ();
   if (attr != NULL)
   {
      Func * create_func = NULL;
      if (experimental)
      {
         /*
         TVar * ready_var = new TVar;
         ready_var->Type = "bool";
         ready_var->Name = "fields_created"; // !?
         ready_var->Init = "false";
         ready_var->access = PrivateAccess;
         ready_var->Group = field_group;

         cls->add (ready_var);
         */

         create_func = new Func;
         create_func->Name = "CreateLocalFields"; // !?
         create_func->Virtual = true;
         create_func->access = ProtectedAccess;
         create_func->Group = field_group;

         /*
         create_func->PutLn ("if (! " + ready_var->Name + ")");
         create_func->PutLn ("{");
         create_func->Indent ();
         */

         if (cls->Parent != "")
            create_func->putLn (cls->Parent + "::" + create_func->Name + " ();");
         else
            create_func->noEmptyLine ();

         /*
         create_func->Location (create_point);
         create_func->EmptyLine ();
         create_func->PutLn (ready_var->Name + " = true;");
         create_func->Unindent ();
         create_func->PutLn ("}");
         */

         cls->add (create_func);
      }

      while (attr != NULL)
      {
         string field_type = "";
         string field_var = attr->Name + "_Field";

         string type = trim (attr->Type); // !?

         if (type == "bool")
            field_type = "BoolField";
         else if (type == "int")
            field_type = "IntField";
         else if (type == "string")
            field_type = "StrField";
         else if (hasSuffix (type, "*"))
         {
            string base_type = trim (removeSuffix (type, "*"));
            // field_type =  "TIndirectFieldTemplate <" + base_type + ">";
            field_type =  base_type + "PtrField";
         }
         else
         {
            field_type =  type + "Field"; // !?
         }

         if (attr->FieldType != "")
            field_type = attr->FieldType;

         bool dynamic = false; // true => without field variable
         if (experimental && ! attr->field_variable)
            dynamic = true;

         if (! dynamic)
         {
            Var * var = new Var;
            var->Type = field_type + ptr_mark;
            var->Name = field_var;
            var->access = ProtectedAccess;
            var->Group = field_group;

            cls->add (var);
         }

         Func * func = cls->getInit ();
         if (dynamic)
            func = create_func;

         func->emptyLine ();
         if (dynamic)
            func->put (field_type + ptr_mark + " "); // local variable
         func->putLn (field_var + " = new " + field_type + ";");
         func->putLn (field_var + "->setName (" + quoteStr (attr->Name) + ");");
         func->putLn (field_var + "->setVariable (" + attr->Name + ");");
         func->put ("this->");
         func->putLn ("insertLastField (" + field_var + ");");

         attr = attr->getNextAttr ();
      }
   }
}

void generateReadAttr (Class * cls)
{
   Attr * attr = cls->getFirstAttr ();
   if (attr != NULL)
   {
      Func * func = new Func;
      func->Name = "ReadField";
      func->Type = "bool";
      func->addParam ("EnvInput" + ref_mark, "io");
      func->Virtual = true;
      func->access = ProtectedAccess;
      func->Group = field_group; // !?
      cls->add (func);

      bool first = true;
      while (attr != NULL)
      {
         if (! first) func->put ("else ");
         func->putLn ("if (io.isIdent (" + quoteStr (attr->Name) + "))");
         func->putLn ("{");
         func->indent ();

            TypeKind kind = attr->attrKind ();
            string read_func = "ReadValue"; // !?

            if (kind == bool_type)
               read_func = "ReadBool";
            else if (kind == int_type)
               read_func = "ReadInt";

            func->putLn ("io.getToken ();");
            func->putLn ("io.checkSeparator ('=');");
            func->putLn (attr->Name + " = io." + read_func + " ();");
            func->putLn ("io.checkSeparator (';');");
            func->putLn ("return true;");

         func->unindent ();
         func->putLn ("}");

         attr = attr->getNextAttr ();
         first = false;
      }

      if (! first) func->put ("else ");
      if (cls->Parent != "")
         func->putLn ("return " + cls->Parent + "::" + func->Name + " (io);");
      else
         func->putLn ("return false;");
   }
}

void generateWriteAttr (Class * cls)
{
   Attr * attr = cls->getFirstAttr ();
   if (attr != NULL)
   {
      Func * func = new Func;
      func->Name = "writeFields"; // !?
      func->addParam ("EnvOutput" + ref_mark, "io");
      func->Virtual = true;
      func->access = ProtectedAccess;
      func->Group = field_group; // !?
      cls->add (func);

      while (attr != NULL)
      {
         TypeKind kind = attr->attrKind();
         string write_func = "Put";

         if (kind == bool_type)
            write_func = "PutBool";
         else if (kind == int_type)
            write_func = "PutInt";

         func->putLn ("io.put (" + quoteStr (attr->Name) + ")");
         func->putLn ("io.put (" + quoteStr(" = ") + " );");
         func->putLn ("io." + write_func + " (" + attr->Name + ")");
         func->putLn ("io.PutChr (';');");
         func->putLn ("io.putEol ();");
         func->emptyLine ();

         attr = attr->getNextAttr ();
      }
   }
}

void generateClassDescription (Class * cls)
{
   /* add base TElement */

   if (cls->Parent == "" && cls->Name != "Element")
      cls->Parent = "Element";

   string factory_base_class = "Factory";

   Module * module = currentModule (cls);

   Class * desc = createFactory (cls, cls, cls->Name + "_Factory", factory_base_class, module);

   desc->no_forward = true; // forward class declaration is not necessary
   cls->getUpperElem()->add (desc);

   /* SuperType */

   if (cls->Parent != "")
   {
      SimpleDecl * decl = new SimpleDecl;
      decl->Kind = intf;
      decl->access = PublicAccess;
      decl->Group = super_group;
      cls->add (decl);

      decl->putLn ("typedef " + cls->Parent + " SuperType;");
   }

   /* getTypeName */

   {
      Func * func = new Func;
      func->Name = "getTypeName";
      func->Type = "string";
      func->Virtual = true;
      func->Inline = true;
      func->access = PublicAccess;
      func->Group = super_group;
      cls->add (func);

      func->put ("return " + quoteStr (cls->Name) + ";");
   }

   /* attribute fields */

   generateAttrDescription (cls);

   /* NameParam - after GenerateAttrDescription */

   {
      Func * func = cls->getInit ();
      func->emptyLine ();

      setField (func, "TypeParam", cls->TypeAttr);
      setField (func, "NameParam", cls->NameAttr);
      setField (func, "FromParam", cls->FromAttr);
   }

   bool experimental = getExperimental (cls);
   if (experimental)
   {
      generateReadAttr (cls);
      // not now: GenerateWriteAttr (cls);
   }
}

void generateEnumDescription (EnumType * type)
{
   /* field class */

   Class * desc = new Class;
   desc->Name = type->Name + "Field";
   desc->Parent = "Field";
   desc->Group = type->Group; // !?
   desc->no_component = true; // no recursion
   type->insertNextElem (desc);

   Var * v = new Var;
   v->Name = "pointer";
   v->Type = type->Name + ptr_mark;
   v->access = PrivateAccess;
   desc->add (v);

   Func * f;

   /* getValue */

   f = new Func;
   f->Name = "getValue";
   f->Type = type->Name;
   f->Virtual = true;
   f->access = PublicAccess;
   desc->add (f);

   f->putLn ("return * pointer;");

   /* setValue */

   f = new Func;
   f->Name = "setValue";
   f->addParam (type->Name, "val");
   f->Virtual = true;
   f->access = PublicAccess;
   desc->add (f);

   f->putLn ("* pointer = val;");

   /* getText */

   f = new Func;
   f->Name = "getText";
   f->Type = "string";
   f->Virtual = true;
   f->access = PublicAccess;
   desc->add (f);

   f->putLn (type->Name + " val = getValue ();");
   Element * item ;
   item = type->getFirstElem ();
   while (item != NULL)
   {
      f->putLn ("if (val == " + item->Name + ")");
      f->indent ();
         f->putLn ("return " + quoteStr (item->Name) + ";");
      f->unindent ();
      item = item->getNextElem ();
   }
   f->putLn ("assert (false);");

   /* setText */

   f = new Func;
   f->Name = "setText";
   f->addParam ("string", "val");
   f->Virtual = true;
   f->access = PublicAccess;
   desc->add (f);

   item = type->getFirstElem ();
   while (item != NULL)
   {
      f->putLn ("if (val == " + quoteStr (item->Name) + ")");
      f->putLn ("{");
      f->indent ();
         f->putLn ("setValue (" + item->Name + ");");
         f->putLn ("return;");
      f->unindent ();
      f->putLn ("}");
      item = item->getNextElem ();
   }
   f->putLn ("assert (false);");

   /* defaultValue */

   f = new Func;
   f->Name = "defaultValue";
   f->Type = type->Name;
   f->Virtual = true;
   f->access = PublicAccess;
   desc->add (f);

   item = type->getFirstElem ();
   if (item != NULL)
       f->putLn ("return " + item->Name + ";");

   /* isDefault */

   f = new Func;
   f->Name = "isDefault";
   f->Type = "bool";
   f->Inline = true;
   f->Virtual = true;
   f->access = PublicAccess;
   desc->add (f);
   f->put ("return getValue () == defaultValue ();");

   /* setPointer */

   f = new Func;
   f->Name = "setPointer";
   f->addParam (type->Name + ptr_mark, "p_pointer");
   f->Virtual = true;
   f->access = PublicAccess;
   desc->add (f);

   f->putLn ("pointer = p_pointer;");

   /* setVariable */

   f = new Func;
   f->Name = "setVariable";
   f->addParam (type->Name + ref_mark, "p_variable");
   f->Virtual = true;
   f->access = PublicAccess;
   desc->add (f);

   f->putLn ("pointer = & p_variable;");
}

/*************************** WRITE DESCRIPTION ****************************/

#if 0
void generateNewDescription (Class * cls)
{
   // NO
   // if (this->Parent == "")
   //    this->Parent = "Element";

   Class * desc = new Class;
   desc->Name = cls->Name + "_Factory";
   if (cls->Parent == "")
      desc->Parent = "Factory";
   else
      desc->Parent = cls->Parent + "_Factory";
   desc->Group = factory_group;
   desc->no_component = true; // no recursion
   desc->no_forward = true; // forward class declaration is not necessary
   cls->getUpperElem()->add (desc);

   /* module initialization */

   {
      Module * module = currentModule (cls);
      if (module != NULL)
      {
         if (module->factory_list)
         {
            module->addFactoryToInitList (desc->Name, cls->variety_symbol);
         }
         else
         {
            Func * f = module->getModuleInit ();
            f->putLn ("RegisterFactory (new " + desc->Name + ");");
         }
      }
   }

   /* factory variable */

   {
      Var * v = new Var;
      v->Name = "factory";
      v->Type = "Factory" + ptr_mark;
      v->Init = "new " + desc->Name;
      v->Static = true;
      v->access = PublicAccess;
      // v->Group = ctor_group;
      desc->add (v);
   }

   /* getFactory */

   {
      Func * func = new Func;
      func->Name = "getFactory";
      func->Type = "Factory" + ptr_mark;
      func->Virtual = true;
      cls->add (func);

      func->putLn ("return " + cls->Name + "_Factory::factory;");
   }

   /* getSuperFactory */

   {
      Func * func = new Func;
      func->Name = "getSuperFactory";
      func->Type = "Factory" + ptr_mark;
      func->Inline = true;
      func->Virtual = true;
      desc->add (func);

      if (cls->Parent == "" || cls->Parent == "Element") // !?
         func->put ("return NULL;");
      else
         func->put ("return " + cls->Parent + "_Factory::factory;");
   }

   /* getName */

   {
      Func * func = new Func;
      func->Name = "getName";
      func->Type = "string";
      func->Inline = true;
      func->Virtual = true;
      desc->add (func);
      func->put ("return " + quoteStr (cls->Name) + ";");
   }

   /* getAlias */

   {
      Func * func = new Func;
      func->Name = "getAlias";
      func->Type = "string";
      func->Inline = true;
      func->Virtual = true;
      desc->add (func);
      func->put ("return " + quoteStr (cls->Alias) + ";");
   }

   /* ReadSource */

   {
      Func * func = new Func;
      func->Name = "ReadSource";
      func->addParam ("TextInput" + ref_mark, "io");
      desc->add (func);

      func->putLn ("io.checkIdent (" + quoteStr (cls->Name) + ");");
      func->putLn ("io.checkSeparator ('{');");
      func->putLn ("io.checkSeparator ('}');");
   }

   /* ReadAttr */

   {
      Func * func = new Func;
      func->Name = "ReadAttr";
      func->addParam ("TextInput" + ref_mark, "io");
      func->addParam ("string", "id");
      func->Virtual = true;
      desc->add (func);

      bool any = false;
      Attr * attr = cls->getFirstAttr ();
      while (attr != NULL)
      {
         if (any) func->put ("else ");
         any = true;

         func->putLn ("if (id == " + quoteStr (attr->Name) + ")");
         func->putLn ("{");
         func->indent ();
         func->unindent ();
         func->putLn ("}");
         attr = attr->getNextAttr ();
      }

      if (cls->Parent != "")
      {
         if (any) func->put ("else ");
         func->putLn (cls->Parent + "_Factory::" + func->Name + " (io, id);");
      }
   }

   /* FindListIndex */

   {
      List * list = cls->getFirstList ();
      if (list != NULL)
      {
         Func * func = new Func;
         func->Name = "FindListIndex";
         func->Type = "int";
         func->addParam ("Factory" + ptr_mark, "fact");
         func->Virtual = true;
         desc->add (func);

         int cnt = 0;
         while (list != NULL)
         {
            cnt ++;
            if (cnt > 1) func->put ("else ");
            func->putLn ("if (fact->IsDerivedFrom (" + list->element + "_Factory::factory))");
            func->indent ();
            func->putLn ("return " + IntToStr (cnt) + ";");
            func->unindent ();
            list = list->getNextList ();
         }
         if (cnt != 0)
         {
             func->putLn ("else");
             func->indent ();
         }
         func->putLn ("return 0;");
         if (cnt != 0)
            func->unindent ();
      }
   }

   /* FindListFactory */

   {
      List * list = cls->getFirstList ();
      if (list != NULL)
      {
         Func * func = new Func;
         func->Name = "FindListFactory";
         func->Type = "Factory" + ptr_mark;
         func->addParam ("int", "inx");
         func->Virtual = true;
         desc->add (func);

         func->putLn ("switch (inx)");
         func->putLn ("{");
         func->indent ();

         int cnt = 0;
         while (list != NULL)
         {
            cnt ++;
            func->putLn ("case " + IntToStr (cnt) + ":");
            func->indent ();
            func->putLn ("return " + list->element + "_Factory::factory;");
            func->unindent ();
            list = list->getNextList ();
         }

         func->putLn ("default:");
         func->indent ();
         func->putLn ("return NULL;");
         func->unindent ();

         func->unindent ();
         func->putLn ("}");
      }
   }

   /* AddIntoList */

   {
      List * list = cls->getFirstList ();
      if (list != NULL)
      {
         Func * func = new Func;
         func->Name = "AddIntoList";
         func->addParam ("int", "inx");
         func->addParam ("void" + ptr_mark, "obj");
         func->addParam ("void" + ptr_mark, "item");
         func->Virtual = true;
         desc->add (func);

         func->putLn (cls->Name + ptr_mark + " target = (" + cls->Name + ptr_mark + ") obj;" );
         // func->EmptyLine ();
         func->putLn ("switch (inx)");
         func->putLn ("{");
         func->indent ();

         int cnt = 0;
         while (list != NULL)
         {
            cnt ++;
            func->putLn ("case " + IntToStr (cnt) + ":");
            func->indent ();
            func->putLn ("target->" + list->ins_last_id + " ((" + list->element + ptr_mark + ") item);");
            func->putLn ("break;");
            func->unindent ();
            list = list->getNextList ();
         }
         func->unindent ();
         func->putLn ("}");
      }
   }

   /* FirstItemFromList */

   {
      List * list = cls->getFirstList ();
      if (list != NULL)
      {
         Func * func = new Func;
         func->Name = "FirstItemFromList";
         func->Type = "void" + ptr_mark;
         func->addParam ("int", "inx");
         func->addParam ("void" + ptr_mark, "obj");
         func->Virtual = true;
         desc->add (func);

         func->putLn (cls->Name + ptr_mark + " target = (" + cls->Name + ptr_mark + ") obj;" );
         // func->EmptyLine ();
         func->putLn ("switch (inx)");
         func->putLn ("{");
         func->indent ();

         int cnt = 0;
         while (list != NULL)
         {
            cnt ++;
            func->putLn ("case " + IntToStr (cnt) + ":");
            func->indent ();
            func->putLn ("return target->" + list->get_first_id + " ();");
            func->unindent ();
            list = list->getNextList ();
         }

         func->putLn ("default:");
         func->indent ();
         func->putLn ("return NULL;");
         func->unindent ();

         func->unindent ();
         func->putLn ("}");
      }
   }

   /* NextItemFromList */

   {
      List * list = cls->getFirstList ();
      if (list != NULL)
      {
         Func * func = new Func;
         func->Name = "NextItemFromList";
         func->Type = "void" + ptr_mark;
         func->addParam ("int", "inx");
         func->addParam ("void" + ptr_mark, "item");
         func->Virtual = true;
         desc->add (func);

         func->putLn ("switch (inx)");
         func->putLn ("{");
         func->indent ();

         int cnt = 0;
         while (list != NULL)
         {
            cnt ++;
            func->putLn ("case " + IntToStr (cnt) + ":");
            func->indent ();
            func->putLn ("return ((" + list->element + ptr_mark + ") item) -> " + list->get_next_id + " ();");
            func->unindent ();
            list = list->getNextList ();
         }

         func->putLn ("default:");
         func->indent ();
         func->putLn ("return NULL;");
         func->unindent ();

         func->unindent ();
         func->putLn ("}");
      }
   }

   /* WriteSource */

   {
      Func * func = new Func;
      func->Name = "WriteSource";
      func->addParam ("TextOutput" + ref_mark, "io");
      func->addParam ("void" + ptr_mark, "obj");
      func->Virtual = true;
      desc->add (func);

      func->putLn ("io.putLn (" + quoteStr (cls->Name) + ");");
      func->putLn ("io.putLn (" + quoteStr ("{") + ");");
      func->putLn ("io.indent ();");

      func->putLn ("WriteAttributes (io, obj);");
      func->putLn ("WriteLists (io, obj);");

      func->putLn ("io.unindent ();");
      func->putLn ("io.putLn (" + quoteStr ("}") + ");");
   }

   /* WriteAttributes */

   {
      Attr * attr = cls->getFirstAttr ();
      if (attr != NULL)
      {
         Func * func = new Func;
         func->Name = "WriteAttributes";
         func->addParam ("TextOutput" + ref_mark, "io");
         func->addParam ("void" + ptr_mark, "obj");
         func->Virtual = true;
         desc->add (func);

         func->putLn (cls->Name + ptr_mark + " item = (" + cls->Name + ptr_mark + ") obj;");
         func->emptyLine ();

         while (attr != NULL)
         {
            func->putLn ("io.put (" + quoteStr (attr->Name) + ");");
            func->putLn ("io.put (" + quoteStr ("=") + ");");

            string ref = "";
            if (attr->only_variable)
               ref = attr->Name;
            else
               ref = "get" + attr->Name + " ()"; // !?

            if (attr->Type == "string")
               func->putLn ("io.put (item->" + ref + ");");
            else if (attr->Type == "bool")
               func->putLn ("io.PutBool (item->" + ref + ");");
            else if (attr->Type == "int")
               func->putLn ("io.PutInt (item->" + ref + ");");
            else
               func->putLn ("// io.put (item->" + ref + ");"); // !?

            func->putLn ("io.putLn (" + quoteStr (";") + ");");
            func->emptyLine ();

            attr = attr->getNextAttr ();
         }

         if (cls->Parent != "")
            func->putLn (cls->Parent + "_Factory::" + func->Name + " (io, item);");
         // else
         //    func->NoEmptyLine ();
      }
   }

   /* WriteLists */

   {
      List * list = cls->getFirstList ();
      if (list != NULL)
      {
         Func * func = new Func;
         func->Name = "WriteLists";
         func->addParam ("TextOutput" + ref_mark, "io");
         func->addParam ("void" + ptr_mark, "obj");
         func->Virtual = true;
         desc->add (func);

         func->putLn (cls->Name + ptr_mark + " item = (" + cls->Name + ptr_mark + ") obj;");
         func->emptyLine ();

         int cnt = 0;
         while (list != NULL)
         {
            cnt ++;
            string var = "p" + IntToStr (cnt);
            func->putLn (list->element + ptr_mark + " " + var + " = item->" + list->get_first_id + " ();");
            func->putLn ("while (" + var + " != NULL)");
            func->putLn ("{");
            func->indent ();
            func->putLn ("TFactory * fact = " + var + "->getFactory ();");
            func->putLn ("fact->WriteSource (io, " + var + ");");
            // func->PutLn ("((" + list->element + "_Factory*) " + list->element + "_Factory::factory)->WriteSource (io, " + var + ");");
            func->putLn (var + " = " + var + "->" + list->get_next_id + " ();");
            func->unindent ();
            func->putLn ("}");
            func->emptyLine ();

            list = list->getNextList ();
         }

         if (cls->Parent != "")
            func->putLn (cls->Parent + "_Factory::" + func->Name + " (io, item);");
         // else
         //    func->NoEmptyLine ();
      }
   }
}
#endif

/******************************* FACTORIES ********************************/

#define FACTORY(name) \
   class name##_Factory : public Factory \
   { \
      protected: \
         virtual Element * newElement (); \
         \
      public: \
         name##_Factory (); \
   }; \
   \
   Element * name##_Factory::newElement () \
   { \
      return new name (); \
   } \
   \
   name##_Factory::name##_Factory () \
   { \
      setName (#name); \
   }

#define FACTORY_WITH_ALIAS(name, alias) \
   class name##_Factory : public Factory \
   { \
      protected: \
         virtual Element * newElement (); \
         \
      public: \
         name##_Factory (); \
   }; \
   \
   Element * name##_Factory::newElement () \
   { \
      return new name (); \
   } \
   \
   name##_Factory::name##_Factory () \
   { \
      setName (#name); \
      setAlias (alias); \
   }

FACTORY_WITH_ALIAS (Project, "project")
FACTORY_WITH_ALIAS (Module, "module")
FACTORY (EnumType)
FACTORY (EnumItem)
FACTORY (Class)
FACTORY (Attr)
FACTORY (List)
FACTORY (SimpleDecl)
FACTORY (Var)
FACTORY (Func)
FACTORY (ParamDecl)

FACTORY_WITH_ALIAS (ExportSymbol, "export_symbol")
FACTORY (TypeInfo)

FACTORY (Styler)

/* initialization */

void code_init ()
{
   registerFactory (new Project_Factory);
   registerFactory (new Module_Factory);
   registerFactory (new EnumType_Factory);
   registerFactory (new EnumItem_Factory);
   registerFactory (new Class_Factory);
   registerFactory (new Attr_Factory);
   registerFactory (new List_Factory);
   registerFactory (new ParamDecl_Factory);
   registerFactory (new ExportSymbol_Factory);
   registerFactory (new TypeInfo_Factory);
}

// CLOSE_NAMESPACE

