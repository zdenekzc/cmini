
/* elem-impl.cc */

OPEN_NAMESPACE

/******************************** ELEMENT *********************************/

Element::Element () :
   Name (),
   Comment (),
   access (PublicAccess),
   Group (0),

   #if 1
   Title (),
   Image (),
   SelectedImage (),
   factory (NULL),
   #endif

   Name_Field (NULL),

   firstElem (NULL),
   lastElem (NULL),
   prevElem (NULL),
   nextElem (NULL),
   upperElem (NULL),

   firstField (NULL),
   lastField (NULL),

   #if 1
   Tree_Observer (NULL),
   PropTable_Observer (NULL),
   #endif

   fields_created (false),

   skip_name (false),

   TypeParam (NULL),
   NameParam (NULL),
   FromParam (NULL),

   OrigFileName (),
   OrigLine (0),
   OrigColumn (0),

   StartLine (0),
   StartColumn (0),
   StopLine (0),
   StopColumn (0),

   imported (false),
   ignore_subelements (false),

   add_comment_before_forward_decl (),
   add_space_after_forward_decl (false),

   variety_symbol ()
{
   StrField * field = new StrField;
   field->setName ("Name");
   field->setVariable (Name);
   this->insertLastField (field);
   Name_Field = field;
}

/* ---------------------------------------------------------------------- */

void Element::createLocalFields ()
{
   StrField * field = new StrField;
   field->setName ("Comment");
   field->setVariable (Comment);
   this->insertLastField (field);
   // Comment_Field = field;

   // Group_Field
   // Access_Field

   #if 0
   field = new StrField;
   field->setName ("Title");
   field->setVariable (Title);
   this->insertLastField (field);
   Title_Field = field;
   #endif

   #if 1
   field = new StrField;
   field->setName ("Image");
   field->setVariable (Image);
   this->insertLastField (field);
   // Image_Field = field;

   field = new StrField;
   field->setName ("SelectedImage");
   field->setVariable (SelectedImage);
   this->insertLastField (field);
   // SelectedImage_Field = field;

   // Factory_Field
   #endif

   // %%
   field = new StrField;
   field->setName ("variety_symbol");
   field->setVariable (variety_symbol);
   this->insertLastField (field);
   // variety_symbol_Field = field;
}

/* ---------------------------------------------------------------------- */

bool Element::readField (EnvInput & io)
{
   if (io.isIdent ("Name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Comment"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Comment = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   /*
   else if (io.IsIdent ("Access"))
   {
      io.GetToken ();
      io.CheckSeparator ('=');
      access = io.ReadValue ();
      io.CheckSeparator (';');
      return true;
   }
   */
   else if (io.isIdent ("Group"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Group = io.readInt ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Title"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Title = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Image"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Image = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("SelectedImage"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      SelectedImage = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   /*
   else if (io.IsIdent ("Factory"))
   {
      io.GetToken ();
      io.CheckSeparator ('=');
      factory = io.ReadValue ();
      io.CheckSeparator (';');
      return true;
   }
   */
   else return false;
}

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE
