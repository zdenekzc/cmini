
/* code-input.cc */

// OPEN_NAMESPACE

string readType (EnvInput & io);
bool readInnerField (EnvInput & io);
bool readInnerElement (EnvInput & io);
Element * readCompleteElement (EnvInput & io, Element * top, string name);

/***************************** CREATE ELEMENT *****************************/

Class * searchClassInProgModule (Module * prog_module, string name)
// search for class by name or alias
{
   // info ("Searching in " + prog_module->Name);
   Class * cls = NULL;

   Element * e = prog_module->getFirstElem ();
   while (e != NULL && cls == NULL)
   {
      Class * c = e->toClass ();
      if (c != NULL)
      {
         // info ("Comparing with " + c->Name);
         if (c->Name == name || ( c->Alias == name && c->Alias != "" ))
         {
            cls = c;
         }
      }
      e = e->getNextElem ();
   }
   return cls;
}

Class * searchClassInProject (Project * project, string name)
// search for class by name or alias
{
   assert (project != NULL);
   Class * cls = NULL;

   Element * m = project->getFirstElem ();
   while (m != NULL && cls == NULL)
   {
      Module * module = m->toModule ();
      if (module != NULL)
         cls = searchClassInProgModule (module, name);
      m = m->getNextElem ();
   }

   return cls;
}

Class * searchClassByNameOrAlias (Element * top, string name)
// search for top level class in top level modules
{
   // debug ("Searching for " + name);
   assert (top != NULL);
   Class * cls = NULL;

   while (top->getUpperElem () != NULL)
       top = top->getUpperElem ();

   if (top != NULL)
   {
      Project * project = top->toProject ();

      if (project != NULL)
         cls = searchClassInProject (project, name);
      else
      {
         Module * prog_module = top->toModule ();
         if (prog_module != NULL)
            cls = searchClassInProgModule (prog_module, name);
      }
   }

   return cls;
}

/* ---------------------------------------------------------------------- */

FlexibleElement * allocateDynamicElement (Project * project, Class * cls)
// create new class instance according to declaration of 'cls'
{
   assert (project != NULL);
   assert (cls != NULL);

   FlexibleElement * tmp = new FlexibleElement; // !?
   tmp->setTypeName (cls->Name);

   string type_attr = "";
   string name_attr = "";
   string from_attr = "";

   // TCompClass * comp_cls= dynamic_cast <TCompClass *> (cls);
   // if (comp_cls != NULL)
   // {
      type_attr = cls->TypeAttr;
      name_attr = cls->NameAttr;
      from_attr = cls->FromAttr;
   // }

   // create fields
   Class * def_cls = cls;
   while (def_cls != NULL)
   {
      Element * e = def_cls->getFirstElem (); // !?
      while (e != NULL)
      {
         Attr * attr = e->toAttr ();
         if  (attr != NULL)
         {
            Field * field = tmp->createField (attr->Name); // !?

            if (attr->Name == type_attr)
               tmp->TypeParam = field;

            if (attr->Name == name_attr)
               tmp->NameParam = field;

            if (attr->Name == from_attr)
               tmp->FromParam = field;
         }

         e = e->getNextElem ();
      }

      if (def_cls->Parent == "")
         def_cls = NULL;
      else
         // def_cls = project -> SearchClass (def_cls->Parent); // !?
         def_cls = searchClassInProject (project, def_cls->Parent);
   }

   return tmp;
}

Element * allocateElement (EnvInput & io, Element * top, string name)
{
   // create element
   Element * result = NULL;

   // search factory
   Factory * factory = lookupFactory (top, name);

   if (factory != NULL)
   {
      result = factory->createElement ();
   }
   else
   {
      Class * cls = searchClassByNameOrAlias (top, name);

      if (cls != NULL)
      {
         // found class definition in curent project
         warning ("Dynamic element: " + name); // !?
         Project * project = currentProject (top);
         result = allocateDynamicElement (project, cls);
      }
      else
      {
         // create TFlexibleElement
         warning ("Unknown element type: " + name); // !?
         FlexibleElement * tmp = new FlexibleElement; // !?
         tmp->setTypeName (name);
         result = tmp;
      }
   }

   assert (result != NULL);
   // info ("AllocateElement " + name + " " + result->getTitle () + " " + result->getTypeName ());
   return result;
}

/* ---------------------------------------------------------------------- */

Field * lookupField (EnvInput & io, Element * elem, string name)
// search for field or create new field
{
   Field * field = elem->searchField (name);

   if (field == NULL)
   {
      field = elem->createField (name); // !?
      // %%
      // if (field != NULL)
      //    warning ("Unknown field: " + name); // !?
   }

   if (field == NULL)
      io.error ("Unknown field: " + name);

   return field;
}

/****************************** READ ELEMENT ******************************/

#if 0
string readQualName (EnvInput & io)
// read qualified name: <identifier> ('.' <identifier)*
{
   string txt = io.readIdent ();
   while (io.isSeparator ('.'))
   {
      io.getToken ();
      txt = txt + '.' + io.readIdent ();
   }
   return txt;
}

string readExpression (EnvInput & io)
// read identifier, number, - number, character or string
{
   string txt;

   if (io.tokenKind == nameToken)
      txt = readQualName (io);
   else if (io.tokenKind == numberToken || io.isSeparator ('-'))
      txt = io.readSignedNumber ();
   else if (io.tokenKind == stringToken)
      txt = removeQuotes (io.readString ());
   else if (io.tokenKind == charToken)
      txt = removeQuotes (io.readString (), quote1);
   else
      io.error ("Invalid parameter");

   return txt;
}
#endif

/* ---------------------------------------------------------------------- */

void readList (EnvInput & io, Element * elem, string name)
{
   if (io.isSeparator ('{'))
   {
      io.getToken (); // skip '{'
      while (io.tokenKind == nameToken)
      {
         // !? insert into specific list
         readCompleteElement (io, elem, "");

         // !? comma or semicolon
         // if (io.IsSeparator (',') || io.IsSeparator (';'))
         //    io.GetToken ();
      }
      io.checkSeparator ('}');
   }
   else
   {
      // !? insert into specific list
      readCompleteElement (io, elem, "");
   }

   // if (io.IsSeparator (';'))
   //    io.GetToken ();
}

/* ---------------------------------------------------------------------- */

#if 0
string readBlock (EnvInput & io)
{
   // read text in braces, including all directives with '#'
   // evaluate directives with '@'

   EnvOptions save;
   io.saveOptions (save);

   opt.hashDirectives = false;
   opt.atDirectives = true;
   opt.DollarDirectives = true;

   opt.reportComments = true;
   opt.reportLineCont = true;
   opt.reportSpaces = true;
   opt.reportEol = true;

   string result = "";
   io.checkSeparator ('{');

   int level = 0;

   while (! io.inpEof && (! io.isSeparator ('}') || level != 0))
   {
      if (io.isSeparator ('{')) level ++;
      if (io.isSeparator ('}')) level --;
      result = result + io.tokenTxt;
      io.getToken ();
   }

   // last whitespaces, comments and newlines
   // newlines are imported for directives
   // result += RemoveSuffix (TokenTxt, '}'); // remove last '}'

   io.restoreOptions (save);
   io.checkSeparator ('}');

   // info ("ReadBlock {:" + result + ":}");
   return result;
}
#endif

/* ---------------------------------------------------------------------- */

Element * readCompleteElement (EnvInput & io, Element * top, string name)
{
   // create and read element

   // read type name
   if (name == "")
      name = io.readIdent ();

   // create element
   Element * e = allocateElement (io, top, name);

   // insert into list
   if (top != NULL)
      top->insertLastElem (e);

   // !? identifier already read
   e->setOrigin (io);

   // read contents
   e->skip_name = true;
   e->readElement (io); // virtual function

   return e;
}

/* ---------------------------------------------------------------------- */

void Element::readElement (EnvInput & io)
{
   // read data into existing element

   #if 1
   if (this->TypeParam != NULL && io.isSeparator ('<'))
   {
      // info ("type param " + this->TypeParam->getName ());
      io.getToken ();
      string txt = readType (io);
      // SetField (io, this->TypeParam, txt);
      this->TypeParam->setText (txt);
      this->TypeParam->user_value = true;
      io.checkSeparator ('>');
   }
   #endif

   if (this->NameParam != NULL &&
       (io.tokenKind == nameToken || io.tokenKind == stringToken))
   {
      // info ("name param " + this->NameParam->getName ());
      this->NameParam->readField (io);
   }

   #if 1
   if (this->FromParam != NULL && io.isSeparator (':'))
   {
      // info ("from param " + this->FromParam->getName ());
      io.getToken ();
      if (io.isIdent ("public")) // !?
         io.getToken ();
      this->FromParam->readField (io);
   }
   #endif

   if (io.isSeparator (';'))
   {
      io.getToken ();
      if (this->toProject () != NULL || this->toModule () != NULL)
      {
         while (! io.isEof ())
            this->readSubitem (io);  // virtual function
      }
   }
   else
   {
      io.checkSeparator ('{');

      while (! io.isSeparator ('}'))
         this->readSubitem (io);  // virtual function

      io.checkSeparator ('}');

      if (io.isSeparator (';'))
         io.getToken ();
   }
}

/* ---------------------------------------------------------------------- */

void Element::readSubitem (EnvInput & io)
{
   if (readInnerField (io))
   {
      // nothing
   }
   else if (readInnerElement (io))
   {
      // nothing
   }
   else
   {
      string name = io.readIdent ();

      if (io.isSeparator ('='))
      {
         // field
         Field * field = lookupField (io, this, name);
         io.getToken (); // skip '='
         field->readField (io);
         io.checkSeparator (';');
      }
      else if (io.isSeparator (':'))
      {
         io.getToken (); // skip ':'
         readList (io, this, name);
      }
      else
      {
         // subitem
         readCompleteElement (io, this, name);
      }
   }
}

bool Element::readInnerField (EnvInput & io)
{
   bool result = false;
   if (this->readField (io))
   {
      // nothing
      result = true;
   }
   else
   {
      if (io.tokenKind == nameToken)
      {
         Field * field = this->searchField (io.tokenTxt);
         if (field != NULL)
         {
            io.getToken ();
            io.checkSeparator ('=');
            field->readField (io);
            io.checkSeparator (';');
            result = true;
         }
      }
   }
   return result;
}

bool Element::readInnerElement (EnvInput & io)
{
   bool result = false;
   if (io.tokenKind == nameToken)
   {
      string name = io.tokenTxt;

      Factory * factory = lookupFactory (this, name);
      if (factory != NULL)
      {
         readCompleteElement (io, this, "");
         result = true;
         /*
         TElement * elem = factory->CreateElement ();
         if (elem != NULL)
         {
            elem->ReadSubitem (io);
            result = true;
         }
         */
      }
   }
   return result;
}

/* ---------------------------------------------------------------------- */

void Field::readField (EnvInput & io)
// read expression and store it in 'field'
{
   string txt = io.readValue ();
   this->setText (txt);
   this->user_value = true;
}

/* ---------------------------------------------------------------------- */

void Element::setOrigin (EnvInput & io)
{
   OrigFileName = io.getFileName ();
   OrigLine = io.getTokenLine ();
   OrigColumn = io.getTokenCol ();
}

void Element::setStart (EnvInput & io)
{
   StartLine = io.getTokenLine ();
   StartColumn = io.getTokenCol ();
}

void Element::setStop (EnvInput & io)
{
   StopLine = io.getTokenLine ();
   StopColumn = io.getTokenCol ();
}

/****************************** READ SOURCE *******************************/

string readType (EnvInput & io)
{
   string result = "";

   if (io.isIdent ("const"))
      result = result + io.readIdent () + " ";

   result = result + io.readIdent ();

   if (io.isSeparator ('<'))
   {
      int level = 0;
      do
      {
         if (io.isSeparator ('<')) level ++;
         if (io.isSeparator ('>')) level --;
         result = result + " " + io.tokenTxt;
         io.getToken ();
      }
      while (level > 0);
   }

   while (io.isSeparator ('*') || io.isSeparator ('&'))
   {
      result = result + " " + io.tokenTxt;
      io.getToken ();
   }

   return result;
}

#if 0
bool readBool (EnvInput & io)
{
   bool value = false;

   if (io.isIdent ("true"))
   {
      io.getToken ();
      value = true;
   }
   else if (io.isIdent ("false"))
   {
      io.getToken ();
      value = false;
   }

   else io.error ("Boolean value expected");

   // info (string ("bool value ") + (value ? "true" : "false"));

   return value;
}
#endif

string readInit (EnvInput & io)
{
   string result = ""; // code without braces

   io.checkSeparator ('=');

   int level = 0;
   while (level != 0 || ( ! io.isSeparator (')') && ! io.isSeparator (',') && ! io.isSeparator (';') ))
   {
      if (io.isSeparator ('(')) level ++;
      if (io.isSeparator (')')) level --;
      result = result + " " + io.tokenTxt;
      io.getToken ();
   }

   return result;
}

string readCodeBlock (EnvInput & io)
{
   string result = ""; // code without braces

   EnvOptions save_options;
   io.saveOptions (save_options);
   io.opt.reportEol = true;
   io.opt.reportSpaces = true;
   io.checkSeparator ('{');

   int level = 1;
   while (level > 0)
   {
      if (io.isSeparator ('{')) level ++;
      if (io.isSeparator ('}')) level --;
      if (level > 0)
      {
         result = result + /* " " + */ io.tokenTxt;
         io.getToken ();
      }
   }

   io.restoreOptions (save_options);
   io.checkSeparator ('}');

   return result;
}

string readSemicolonBlock (EnvInput & io)
{
   string result = ""; // code without braces

   while (! io.isSeparator (';'))
   {
      result = result + " " + io.tokenTxt;
      io.getToken ();
   }

   io.checkSeparator (';');
   result = result + ";";

   return result;
}

void readDeclBlock (EnvInput & io, Element * target, DeclKind kind, int group)
{
   EnvOptions save_opt;
   EnvOptions decl_opt;
   io.saveOptions (save_opt);
   io.opt.hashDirectives = false; // report directives
   // NO io.opt.ReportComments = true;
   // NO io.opt.ReportLineCont = true;
   // NO io.opt.ReportSpaces = true;
   // NO io.opt.ReportEol = true;

   io.opt.reportComments = false;

   TextStorage storage;
   Access acs = PublicAccess;
   bool start = true;
   bool directive = false;

   io.checkSeparator ('{');
   io.anyWhiteSpace = false;

   int level = 1;
   while (level > 0)
   {
      if (io.isSeparator ('{')) level ++;
      if (io.isSeparator ('}')) level --;

      bool send = true;

      if (level > 0)
      {
         if (start)
         {
            bool cont = true;
            while (cont)
            {
               if (io.isIdent ("private"))
               {
                  io.getToken ();
                  io.checkSeparator (':');
                  acs = PrivateAccess;
               }
               else if (io.isIdent ("protected"))
               {
                  io.getToken ();
                  io.checkSeparator (':');
                  acs = ProtectedAccess;
               }
               else if (io.isIdent ("public"))
               {
                  io.getToken ();
                  io.checkSeparator (':');
                  acs = PublicAccess;
               }
               else cont = false;
            }
         }

         if (io.isSeparator ('#'))
         {
            directive = true;
            io.saveOptions (decl_opt);
            // io.opt.ReportComments = true;
            io.opt.reportComments = false;
            io.opt.reportLineCont = true;
            io.opt.reportSpaces = true;
            io.opt.reportEol = true;
         }

         send = false;
         start = false;
         if ((io.isSeparator (';') || io.isSeparator ('}')) && level == 1)
         {
            send = true;
            start = true;
         }

         if (directive)
         {
            if (io.isEol ())
            {
               directive = false;
               io.restoreOptions (decl_opt);
               send = true;
            }
            else
            {
               storage.put_str (io.tokenTxt);
            }
         }
         else
         {
            if (io.anyWhiteSpace)
            {
               if (! storage.empty ())
                  storage.put (' ');
               io.anyWhiteSpace = false;
            }
            storage.put_str (io.tokenTxt);
         }

         io.getToken ();
      }

      if (send && level == 0 && trim (storage.to_string ()) == "")
         send = false;

      if (send)
      {
          SimpleDecl * decl = new SimpleDecl;
          decl->Kind = kind;
          decl->access = acs;
          decl->Group = group;
          target->add (decl);

          decl->putLn (storage.to_string ());
          storage.reset ();
      }
   }

   io.restoreOptions (save_opt);
   io.checkSeparator ('}');
}

/* ---------------------------------------------------------------------- */

Element * readDeclaration (EnvInput & io, Element * above)
{
   Element * result = NULL;

   if (io.isIdent ("class") || io.isIdent ("struct"))
   {
      Class * cls = new Class;
      // above->add (cls); // attributes need contact to module
      cls->readSource (io);
      result = cls;
   }
   else if (io.isIdent ("enum"))
   {
      EnumType * enum_type = new EnumType;
      enum_type->readSource (io);
      result = enum_type;
   }
   else if (io.isIdent ("friend"))
   {
      SimpleDecl * decl = new SimpleDecl;
      decl->Kind = intf;
      decl->putLn (readSemicolonBlock (io));
      result = decl;
   }
   else if (io.isIdent ("typedef"))
   {
      SimpleDecl * decl = new SimpleDecl;
      decl->Kind = intf;
      decl->putLn (readSemicolonBlock (io));
      result = decl;
   }
   else
   {
      bool a_extern = false;
      bool a_static = false;
      bool a_virtual = false;
      bool a_inline = false;

      while (io.isIdent ("extern") ||
             io.isIdent ("static") ||
             io.isIdent ("virtual") ||
             io.isIdent ("inline"))
      {
         if (io.isIdent ("extern"))
         {
            io.getToken ();
            a_extern = true;
         }

         if (io.isIdent ("static"))
         {
            io.getToken ();
            a_static = true;
         }

         if (io.isIdent ("virtual"))
         {
            io.getToken ();
            a_virtual = true;
         }

         if (io.isIdent ("inline"))
         {
            io.getToken ();
            a_inline = true;
         }
      }

      string type = "";

      if (io.isIdent ("const"))
      {
         io.getToken ();
         type = "const ";
      }

      bool a_destructor = false;
      if (io.isSeparator ('~'))
      {
         io.getToken ();
         a_destructor = true;
      }
      else
      {
         type = type + readType (io);
      }

      if (io.isSeparator ('{'))
      {
          result = readCompleteElement (io, above, type);
          result->removeElem (); // temporarily unlink
      }
      else
      {
         string name = io.readValue ();

         if (io.isSeparator ('('))
         {
            Func * decl = new Func;
            decl->Type = type;
            decl->Name = name;
            decl->Extern = a_extern;
            decl->Static = a_static;
            decl->Virtual = a_virtual;
            decl->Inline = a_inline;
            decl->Destructor = a_destructor;

            io.checkSeparator ('(');
            while (! io.isSeparator (')'))
            {
               ParamDecl * param = new ParamDecl;
               param->Type = readType (io);
               param->Name = io.readIdent ();

               if (io.isSeparator ('='))
                  param->Init = readInit (io);

               decl->Params->insertLastElem (param);

               if (! io.isSeparator (')'))
                  io.checkSeparator (',');
            }
            io.checkSeparator (')');

            if (io.isIdent ("const"))
            {
               io.getToken ();
               decl->Follow = " const";
            }

            if (io.isSeparator ('{'))
            {
               decl->put (readCodeBlock (io));

               Module * module = currentModuleOrNull (above);
               if (module != NULL && module->keep_methods)
                  decl->Inline = true; // mark as inline, so method is not moved to implementation file
            }
            else
            {
                if (io.isSeparator ('='))
                {
                   io.getToken ();
                   if (! io.isNumber () || io.tokenTxt != "0")
                      io.error ("Zero expected");
                   io.getToken ();
                }
               decl->no_impl = true;
               io.checkSeparator (';');
            }
            result = decl;
         }
         else
         {
            if (a_destructor)
               io.error ("Destructor without parameters");

            if (a_virtual)
               io.error ("Virtual function without parameters");

            if (a_inline)
               io.error ("Inline function without parameters");

            Var * decl = new Var;
            decl->Type = type;
            decl->Name = name;
            decl->Extern = a_extern;
            decl->Static = a_static;

            if (io.isSeparator ('='))
               decl->Init = readInit (io);

            io.checkSeparator (';');
            result = decl;
         }
      }
   }

   return result;
}

/* ---------------------------------------------------------------------- */

#if 0
bool readBoolParam (EnvInput & io)
{
   io.getToken (); // skip identifier
   io.checkSeparator ('=');
   bool result = io.readBool ();
   io.checkSeparator (';');
   return result;
}

string readIdentParam (EnvInput & io)
{
   io.getToken (); // skip identifier
   io.checkSeparator ('=');
   string result = io.readIdent ();
   io.checkSeparator (';');
   return result;
}

string readValueParam (EnvInput & io)
{
   io.getToken (); // skip identifier
   io.checkSeparator ('=');
   string result = readValue (io);
   io.checkSeparator (';');
   return result;
}
#endif

/***************************** READ ENUM TYPE  ******************************/

void EnumType::readSource (EnvInput & io)
{
   io.checkIdent ("enum");
   Name = io.readIdent ();

   readItems (io);

   if (io.isSeparator (';')) // !?
      io.checkSeparator (';');
}

void EnumType::readItems (EnvInput & io)
{
   io.checkSeparator ('{');

   bool cont = false;
   while (! io.isSeparator ('}'))
   {
      if (cont)
         io.checkSeparator (',');
      else
         cont = true;

      EnumItem * item = new EnumItem;
      item->Name = io.readIdent ();
      add (item);
   }

   io.checkSeparator ('}');
}

/******************************* READ CLASS  *******************************/

void Class::readSource (EnvInput & io)
{
   Access acs;

   if (skip_name)
   {
      acs = PublicAccess;
   }
   else if (io.isIdent ("struct")) // !?
   {
      io.checkIdent ("struct");
      acs = PublicAccess;
   }
   else
   {
      io.checkIdent ("class");
      acs = PrivateAccess;
   }

   setStart (io);
   // Name = io.ReadIdent ();
   Name = io.readValue (); // identifier or string

   if (io.isSeparator (':'))
   {
       io.getToken ();
       if (io.isIdent ("public")) // !? skip
          io.getToken ();
       Parent = io.readIdent ();
   }

   if (io.isSeparator (';'))
      io.checkSeparator (';');
   else
   {
      io.checkSeparator ('{');

      while (! io.isSeparator ('}'))
         readMember (io, acs);

      setStop (io);
      io.checkSeparator ('}');

      if (io.isSeparator (';')) // !?
         io.checkSeparator (';');
   }
}

void Class::readSubitem (EnvInput & io)
{
   readMember (io, read_access);
}

/*
bool IsNext (TEnvInput & io, char c)
{
   TInputItem * inx = io.Mark ();
   io.GetToken ();
   bool result = io.IsSeparator (c);
   io.Rewind (inx);
   return result;
}
*/

void Class::readMember (EnvInput & io, Access & acs, int grp)
{
   if (io.isIdent ("private"))
   {
      io.getToken ();
      io.checkSeparator (':');
      acs = PrivateAccess;
   }
   else if (io.isIdent ("protected"))
   {
      io.getToken ();
      io.checkSeparator (':');
      acs = ProtectedAccess;
   }
   else if (io.isIdent ("public"))
   {
      io.getToken ();
      io.checkSeparator (':');
      acs = PublicAccess;
   }
   else if (readInnerField (io))
   {
      // nothing
   }
   #if 0
   else if (readInnerElement (io))
   {
      // nothing
   }
   #endif
   else if (io.isIdent ("decl")) // !? compatibility with previous version
   {
      io.getToken ();
      readDeclBlock (io, this, intf, user_group);
   }
   else if (io.isIdent ("init"))
   {
      io.getToken ();
      string code = readCodeBlock (io);
      getInit () ->put (code);

   }
   else if (io.isIdent ("attr") && getTypeName () == "Class") // !?
   {
      Attr * attr = new Attr;
      attr->access = PublicAccess;
      attr->Group = grp;
      insertLastElem (attr);
      attr->readSource (io);
   }
   else if (io.isIdent ("list") && getTypeName () == "Class") // !?
   {
      List * list = new List;
      list->access = acs;
      list->Group = grp;
      insertLastElem (list);
      list->readSource (io);
   }
   #if 0
   else if (io.isIdent ("Styler"))
   {
      Styler * styler = new Styler;
      insertLastElem (styler);
      styler->readElement (io);
   }
   #endif
   #if 0
   else if (getTypeName () == "Class" ||
            // getTypeName () == "TGtkClass" ||
            io.isIdent ("string") ||
            io.isIdent ("bool") ||
            io.isIdent ("int") ||
            io.isIdent ("void") ||
            io.isIdent ("ExtModel") ||
            io.isIdent ("ExtDriver") ||
            io.isIdent ("SimTreeViewItem") ||
            io.isIdent ("GtkCellRenderer") ||
            io.isIdent ("SimWidget") ||
            io.isIdent ("SimSubMenu") ||
            io.isIdent ("SimTreeView") ||
            io.isIdent ("SimTreeItem") ||
            io.isIdent ("SimPropertyItem") ||
            io.isIdent ("GtkTreeViewColumn") ||
            io.isIdent ("SimImage") ||
            io.isIdent ("SimPalettePage") ||
            io.isIdent ("GtkToolbarChild") ||
            io.isIdent ("GtkWidget") ||
            io.isIdent ("GdkFont") ||
            io.isIdent ("GdkBitmap") ||
            io.isIdent ("GdkPixmap") ||
            io.isIdent ("QBoxLayout") ||
            io.isIdent ("MyListView") ||
            io.isIdent ("MyListViewItem") ||
            io.isIdent ("QtAbstractAdapter") ||
            io.isSeparator ('~') ||
            io.isIdent ("guint16") ||
            io.isIdent ("static") ||
            io.isIdent ("inline") ||
            io.isIdent ("friend") ||
            io.isIdent ("enum") ||
            io.isIdent ("typedef") ||
            io.isIdent ("inline") ||
            io.isIdent ("virtual")) // !?
   #endif
   else if (! io.nextSeparator ('=') &&
            ! io.isIdent ("TGtkSignal") &&
            ! io.isIdent ("TQtAttr") &&
            ! io.isIdent ("TQtSignal") &&
            ! io.isIdent ("attr") &&
            ! io.isIdent ("list") &&
            ! io.isIdent ("signal")) // !?
   {
      Element * item = readDeclaration (io, this);
      if (item != NULL)
      {
         item->access = acs;
         item->Group = grp;
         insertLastElem (item);
      }
   }
   else
   {
       Element::readSubitem (io);
   }
}

/* ---------------------------------------------------------------------- */

void Attr::readSource (EnvInput & io)
{
   if (! skip_name)
      io.checkIdent ("attr");

   if (io.isSeparator ('<')) // !?
   {
      io.checkSeparator ('<');
      if (io.tokenKind == stringToken) // !?
         Type = io.readString ();
      else
         Type = readType (io);
      io.checkSeparator ('>');
   }

   if (io.tokenKind == nameToken || io.tokenKind == stringToken)
      Name = io.readValue ();

   if (io.isSeparator (';'))
      io.getToken ();
   else
   {
      io.checkSeparator ('{');
      while (! io.isSeparator ('}'))
      {
          if (readInnerField (io))
          {
             /* nothing */
          }
          else
          {
              // !?
              string id = io.readIdent ();
              io.checkSeparator ('=');
              io.readValue ();
              io.checkSeparator (';');
              io.warning ("Unknown field: " + id);
          }
      }
      io.checkSeparator ('}');

      if (io.isSeparator (';'))
         io.getToken ();
   }

   Module * module = currentModule (this);
   if (module != NULL)
   {
      if (module->only_variables)
      {
         only_variable = true;
      }
   }
}

/* ---------------------------------------------------------------------- */

void List::readSource (EnvInput & io)
{
   if (! skip_name)
   io.checkIdent ("list");

   io.checkSeparator ('<');
   element = io.readIdent ();
   io.checkSeparator ('>');

   if (io.isIdentToken ())
      Name = io.readIdent ();

   if (io.isSeparator (';'))
      io.getToken ();
   else
   {
      io.checkSeparator ('{');
      while (! io.isSeparator ('}'))
      {
          if (readInnerField (io))
          {
             /* nothing */
          }
          else
          {
              // !?
              string id = io.readIdent ();
              io.checkSeparator ('=');
              io.readValue ();
              io.checkSeparator (';');
              io.warning ("Unknown field: " + id);
          }
      }
      io.checkSeparator ('}');

      if (io.isSeparator (';'))
         io.getToken ();
   }

   setNames (); // default identifiers
}

/* ---------------------------------------------------------------------- */

//******************************* READ MODULE ******************************/

void Module::readSubitem (EnvInput & io)
{
   if (readInnerField (io))
   {
      // nothing
   }
   else if (readInnerElement (io))
   {
      // nothing
   }
   else if (/* io.IsIdent ("include") || */ io.isIdent ("header")) // conflict with previous version
   {
      io.getToken ();
      string fileName = io.readString ();
      includeFile (intf, fileName);
      if (io.isSeparator (';'))
         io.checkSeparator (';');
   }
   else if (io.isIdent ("idecl")) // !? compatibility with previous version
   {
      io.getToken ();
      readDeclBlock (io, this, intf, include_group);
   }
   else if (io.isIdent ("decl"))
   {
      io.getToken ();
      readDeclBlock (io, this, intf, extension_group);
   }
   else if (io.isIdent ("user_decl"))
   {
      io.getToken ();
      readDeclBlock (io, this, intf, user_group);
   }
   else if (io.isIdent ("store"))
   {
      readStore (io, this);
   }
   #if 1
   else if (io.isIdent ("class") || io.isIdent ("struct"))
   {
      Class * cls = new Class;
      add (cls); // attributes need contact to module
      cls->readSource (io);
   }
   #endif
   #if 0
   else if (getTypeName () == "Module" ||
            io.isIdent ("enum") ||
            io.isIdent ("void") ||
            io.isIdent ("inline") ||
            io.isIdent ("GtkTreePath") ||
            io.isIdent ("typedef")) // !?
   #endif
   else if (! io.nextSeparator ('='))
   {
      Element * item = readDeclaration (io, this);
      add (item);
   }
   else
   {
       Element::readSubitem (io);
   }
}

void Module::readSource (EnvInput & io)
{
   if (! skip_name)
   {
      // if (io.IsIdent ("TGtkModule")) // !?
      //    io.GetToken ();
      // else
      io.checkIdent ("module");
   }

   if (io.tokenKind == nameToken || io.tokenKind == stringToken)
      Name = io.readValue ();

   if (io.isSeparator (';'))
   {
      io.checkSeparator (';');

      while (! io.isEof ())
      {
         readSubitem (io);
      }
   }
   else
   {
      io.checkSeparator ('{');

      while (! io.isSeparator ('}'))
      {
         readSubitem (io);
      }

      io.checkSeparator ('}');
   }
}

/****************************** READ PROJECT ******************************/

void customCommand (EnvInput & io, Project * prj)
{
   if (io.isIdent ("alt_select"))
   {
      io.getToken ();
      string txt = io.readNumber ();
      int num;
      if (StrToIntOk (txt, num))
      {
         assert (prj != NULL);
         prj->selected_alternative = num;
         prj->select_alternative = true;
      }
      io.checkSeparator (';');
   }
}

void Project::readSource (EnvInput & io)
{
   customCommand (io, this);

   if (! skip_name)
      io.checkIdent ("project");

   Name = io.readValue ();
   io.checkSeparator (';');

   while (! io.isEof ())
   {
      readSubitem (io);
   }
}

void loadProject (Project * project, string file_name)
{
   assert (project != NULL);

   // open file
   EnvInput & io = project->input;
   io.opt.atDirectives = true;
   io.open (file_name);

   // read first token
   io.getToken ();

   if (io.isEof ())
      io.error ("Empty file");

   customCommand (io, project); // read alt_select

   if (io.isIdent ("project"))
   {
      // p->SetOrigin ();
      project->readSource (io);
   }
   else if (io.isIdent ("module"))
   {
      project->Name = removeSuffix (file_name, ".t"); // !?
      Module * m = new Module;
      project->add (m);
      m->readSource (io);
   }
   else
   {
      if (io.isEof ())
         io.error ("Empty project");

      while (! io.isEof ())
         readCompleteElement (io, project, "");
   }

   if (! io.isEof ())
      io.error ("End of input file expected");
}

// CLOSE_NAMESPACE
