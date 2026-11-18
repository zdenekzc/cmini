
/* code-output.cc */

// OPEN_NAMESPACE

/***************************** WRITE ELEMENT ******************************/

void writeValue (EnvOutput & io, Field * p)
{
   string txt = p->getText ();
   if (isValidIdent (txt) || isValidNumber (txt))
      io.put (txt);
   else
      io.put (quoteStr (txt));
}

void writeTypeValue (EnvOutput & io, Field * p)
{
   string txt = p->getText ();

   int inx = txt.length () - 1;
   while (inx >= 0 && (txt [inx] == ' ' || txt [inx] == '*'))
      inx --;

   string prefix;
   if (inx >= 0)
      prefix = Head (txt, inx+1);
   else
      prefix = "";

   if (isValidIdent (txt) || isValidNumber (txt))
      io.put (txt);
   else
      io.put (quoteStr(txt));
}

#if 0
void writeBlock (EnvOutput & io, string head, string body)
{
   io.putLn (head);
   io.putLn ("{");
   io.incIndent ();

   int len = body.length ();
   int inx = 0; // index of current character
   int start = 0; // start of line

   while (inx < len)
   {
      start = inx;
      while (inx < len && /* body [inx] != cr && */ body [inx] != lf) inx ++;

      io.putLn (body.substr (start, inx-start));

      if (inx+1 < len && body [inx] == cr && body [inx+1] == lf) inx ++;
      inx ++;
   }

   io.decIndent ();
   io.putLn ("}");
}
#endif

/***************************** INPUT / OUTPUT *****************************/

void Element::writeElement (EnvOutput & io)
{
   io.put (this->getTypeName ());

   if (this->TypeParam != NULL && ! this->TypeParam->isDefault ())
   {
      io.put (" <");
      writeTypeValue (io, this->TypeParam);
      io.put (">");
   }

   if (this->NameParam != NULL && ! this->NameParam->isDefault ())
   {
      io.put (" ");
      writeValue (io, this->NameParam);
   }

   if (this->FromParam != NULL && ! this->FromParam->isDefault ())
   {
      io.put (": ");
      writeValue (io, this->FromParam);
   }

   bool any = false;

   // fields
   Field * p = this->getFirstField ();
   while (p != NULL)
   {
      if ((! p->isDefault () || p->user_value) &&
          p != this->TypeParam &&
          p != this->NameParam &&
          p != this->FromParam)
      {
         if (!any)
         {
            io.putEol ();
            io.putLn ("{");
            io.incIndent ();
            any = true;
         }
         p->writeField (io); // virtual function
      }
      p = p->getNextField ();
   }

   bool empty_line = any;

   // subitems
   Element * s = this->getFirstElem ();
   while (s != NULL)
   {
      if (!any)
      {
         io.putEol ();
         io.putLn ("{");
         io.incIndent ();
         any = true;
      }

      if (empty_line)
         io.putEol ();
      empty_line = true;

      s->writeElement (io);
      s = s->getNextElem ();
   }

   // finish
   if (any)
   {
      io.decIndent ();
      io.putLn ("}");
   }
   else
   {
      io.putLn (";");
   }
}

/* ---------------------------------------------------------------------- */

void Field::writeField (EnvOutput & io)
{
   io.put (this->getName ());
   io.put (" = ");

   writeValue (io, this);
   io.put (";");
   io.putEol ();
}

/****************************** WRITE SOURCE ******************************/

void Element::put (string txt)
{
    TextItem * item = new TextItem;
    item->Code = txt;
    add (item);
}

void Element::putLn (string txt)
{
    LineItem * item = new LineItem;
    item->Code = txt;
    add (item);
}

void Element::emptyLine ()
{
   Element * item = new EmptyLineItem;
   add (item);
}

void Element::noEmptyLine ()
{
   Element * item = new NoEmptyLineItem;
   add (item);
}

void Element::indent ()
{
   Element * item = new IndentItem;
   add (item);
}

void Element::unindent ()
{
   Element * item = new UnindentItem;
   add (item);
}

void Element::Location (StatList * & place)
{
   place = new StatList;

   LocationItem * item = new LocationItem;
   item->InnerItem = place;
   add (item);
}

void Element::addStatList (StatList * list)
{
   add (list); // !?
}

/* ----------------------------- write code ----------------------------- */

void Element::writeCode (TextOutput & io)
{
   Element * item = getFirstElem ();
   while (item != NULL)
   {
      item->writeCode (io);
      item = item->getNextElem ();
   }
}

/* -------------------------- conditional symbols ----------------------- */

class VarietyInfo
{
    public:
       string current_symbol;
       string class_symbol;
       bool back_indent;
       bool no_empty_line;

       VarietyInfo ();
       void change_variety (TextOutput & io, string variety_symbol);
};

VarietyInfo::VarietyInfo () :
   current_symbol (),
   class_symbol (),
   back_indent (false),
   no_empty_line (false)
{ }

void VarietyInfo::change_variety (TextOutput & io, string variety_symbol)
{
   if (variety_symbol == class_symbol)
      variety_symbol = "";

   if (variety_symbol != current_symbol && current_symbol != "")
   {
      io.noEmptyLine ();
      io.putCondEol ();
      if (back_indent) io.unindent ();
      io.putLn ("#endif // " + current_symbol);
      if (back_indent) io.indent ();
      if (! no_empty_line) io.emptyLine ();
   }

   if (variety_symbol != current_symbol && variety_symbol != "")
   {
      io.putCondEol ();
      if (back_indent) io.unindent ();
      io.putLn ("#ifdef " + variety_symbol);
      if (back_indent) io.indent ();
   }

   current_symbol = variety_symbol;
}

/* -------------------------- write interface --------------------------- */

void Element::writeIntf (TextOutput & io)
{
   VarietyInfo variety;

   Element * item = getFirstElem ();
   while (item != NULL)
   {
      variety.change_variety (io, item->variety_symbol);

      item->writeIntf (io);
      item = item->getNextElem ();
   }

   variety.change_variety (io, "");
}

/* ------------------------ write implementation ------------------------- */

void Element::writeImpl (TextOutput & io)
{
   VarietyInfo variety;

   Element * item = getFirstElem ();
   while (item != NULL)
   {
      variety.change_variety (io, item->variety_symbol);

      item->writeImpl (io);
      item = item->getNextElem ();
   }

   variety.change_variety (io, "");
}

/* ---------------------------- Expression ----------------------------- */

void Expr::writeCode (TextOutput & io)
{
   // if (Comment != "")
   //    io.PutLn ("/* " + Comment + " */");

   io.put (Code);

   Element * item = getFirstElem ();
   while (item != NULL)
   {
      item->writeCode (io);
      item = item->getNextElem ();
   }
}

/* ----------------------------- Statements ----------------------------- */

#if 0
void SimpleStat::writeCode (TextOutput & io)
{
   io.putLn (Code);
}

void CompoundStat::writeCode (TextOutput & io)
{
   io.putLn ("{");
   io.incIndent ();

   Stat::writeIntf (io);

   io.decIndent ();
   io.putLn ("}");
}

void IfStat::writeCode (TextOutput & io)
{
   io.put ("if (");
   io.put (Condition);
   io.put (")");
   io.putEol ();

   if (Then.getFirstElem () == Then.getLastElem () &&
       Then.getFirstElem () != NULL)
   {
      // one statement
      io.incIndent ();
      Then.writeCode (io);
      io.decIndent ();
   }
   else
   {
      // empty or more statements
      io.putLn ("{");
      io.incIndent ();
      Then.writeCode (io);
      io.decIndent ();
      io.putLn ("}");
   }

   if (Else.getFirstElem() != NULL)
   {
      if (Else.getFirstElem () == Else.getLastElem () &&
          Else.getFirstElem ()->isIfStat ())
      {
         io.put ("else "); // continue with nested if on same line
         Else.writeCode (io);
      }
      else if (Else.getFirstElem () == Else.getLastElem ())
      {
         // one statement
         io.putLn ("else");
         io.incIndent ();
         Else.writeCode (io);
         io.decIndent ();
      }
      else
      {
         // more statements
         io.putLn ("else");
         io.putLn ("{");
         io.incIndent ();
         Else.writeCode (io);
         io.decIndent ();
         io.putLn ("}");
      }
   }
}

void WhileStat::writeCode (TextOutput & io)
{
   io.put ("while (");
   io.put (Condition);
   io.put (")");
   io.putEol ();

   if (Body.getFirstElem () == Body.getLastElem () &&
       Body.getFirstElem () != NULL)
   {
      // one statement
      io.incIndent ();
      Body.writeCode (io);
      io.decIndent ();
   }
   else
   {
      // empty or more statements
      io.putLn ("{");
      io.incIndent ();
      Body.writeCode (io);
      io.decIndent ();
      io.putLn ("}");
   }
}
#endif

void ClusterStat::writeCode (TextOutput & io)
{
   assert (InnerItem != NULL);
   Element * e = InnerItem->getFirstElem ();

   if (e != NULL && e == InnerItem->getLastElem ())
   {
      Stat::writeIntf (io);
   }
   else
   {
      io.putLn ("{");
      io.incIndent ();

      Stat::writeIntf (io);

      io.decIndent ();
      io.putLn ("}");
   }
}

/* ----------------------------- Text Items ----------------------------- */

void TextItem::writeCode (TextOutput & io)
{
   // io.Put ("/* TTextItem */");
   io.put (Code);
}

void LineItem::writeCode (TextOutput & io)
{
   // io.Put ("/* TLineItem */");
   io.putLn (Code);
}

void IndentItem::writeCode (TextOutput & io)
{
   // io.Put ("/* TIndentItem */");
   io.incIndent ();
}

void UnindentItem::writeCode (TextOutput & io)
{
   // io.Put ("/* TUnindentItem */");
   io.decIndent ();
}

void EmptyLineItem::writeCode (TextOutput & io)
{
   // io.Put ("/* TEmptyLineItem */");
   io.emptyLine ();
   // io.Put ("/* end of TEmptyLineItem */");
}

void NoEmptyLineItem::writeCode (TextOutput & io)
{
   // io.Put ("/* TNoEmptyLineItem */");
   io.noEmptyLine ();
}

void LocationItem::writeCode (TextOutput & io)
{
   if (InnerItem != NULL)
      InnerItem->writeCode (io);
}

/* ------------------------- Simple Declaration ------------------------- */

void SimpleDecl::writeIntf (TextOutput & io)
{
   if (Kind == intf)
   {
      if (Comment != "")
         io.putLn ("// " + Comment);

      writeCode (io);
   }
}

void SimpleDecl::writeImpl (TextOutput & io)
{
   if (Kind == impl)
   {
      if (Comment != "")
         io.putLn ("// " + Comment);

      writeCode (io);
   }
}

/************************** VARIABLE DECLARATION **************************/

void Var::writeIntf (TextOutput & io)
{
   if (Comment != "")
      io.putLn ("// " + Comment);

   if (Extern)
      io.put ("extern ");

   if (Static)
      io.put ("static ");

   io.put (Type);
   io.put (" ");

   if (Precede != "")
   {
      io.put (Precede);
      io.put (" ");
   }

   io.put (Name);

   if (Follow != "")
   {
      io.put (" ");
      io.put (Follow);
   }

   if (Init != "" && SimpleInit)
   {
      io.put (" = ");
      io.put (Init);
   }

   io.put (";");
   io.putEol ();
}

bool Var::hasInit ()
{
   return (! Static);
}

void Var::writeInit (TextOutput & io)
{
   if (! Static)
   {
      if (Init == "")
         Init = currentProject (this) -> getConstructorInitValue (Type);

      io.put (Name + " (" + Init + ")");
   }
}

void Var::writeImpl (TextOutput & io)
{
   if (! SimpleInit)
      if (Extern || Static /* || Init != "" */) // !?
      {
         io.put (Type);
         io.put (" ");

         if (getUpperElem () != NULL)
            getUpperElem () -> writePrefix (io);

         io.put (Name);

         if (Follow != "")
         {
            io.put (" ");
            io.put (Follow);
         }

         if (Init != "" && ! SimpleInit)
         {
            io.put (" = ");
            io.put (Init);
         }

         io.put (";");
         io.putEol ();

         io.emptyLine (); // empty line
      }
}

/************************** PARAMETER DECLARATION *************************/

void ParamDecl::writeIntf (TextOutput & io)
{
   io.put (Type);
   io.put (" ");
   io.put (Name);

   if (Init != "")
   {
      io.put (" = ");
      io.put (Init);
   }
}

void ParamDecl::writeImpl (TextOutput & io)
{
   io.put (Type);
   io.put (" ");
   io.put (Name);
}

/************************** FUNCTION DECLARATION **************************/

void Func::writeBody (TextOutput & io)
{
   if (Constructor)
   {
       Element * cls = getUpperElem ();
       assert (cls != NULL);
       cls->writeInitMembers (io);
   }
   else
   {
      if (! Inline)
         io.putEol (); // finish line
   }

   bool empty = true;
   {
      Element * item = getFirstElem ();
      while (item != NULL && empty)
      {
         empty = item->isEmpty ();
         item = item->getNextElem ();
      }
   }

   if (Inline)
   {
      io.put (" { ");
   }
   else
   {
      if (empty)
      {
         io.put ("{");
      }
      else
      {
         io.putLn ("{");
         io.noEmptyLine();
         io.incIndent ();
      }
   }

   if (Inline)
      io.suspendEol ();

   if (! empty)
   {
       Element * item = getFirstElem ();
       while (item != NULL)
       {
          // io.Put ("/* WriteCode */");
          item->writeCode (io);
          item = item->getNextElem ();
       }
   }

   if (Inline)
      io.resumeEol ();

   if (Inline)
   {
      io.putLn (" }"); // space, brace, end of line
   }
   else
   {
      if (empty)
         io.putLn (" }");
      else
      {
         io.putCondEol ();
         io.decIndent ();
         io.noEmptyLine();
         io.putLn ("}");
      }
   }
}

void Func::writeIntf (TextOutput & io)
{
   if (Comment != "")
      io.putLn ("// " + Comment);

   if (! no_decl)
   {
      if (skip_decl)
         io.putLn ("#if 0");

      if (Inline && sourceCls () == NULL)
         io.put ("inline ");

      if (Extern)
         io.put ("extern ");

      if (Static)
         io.put ("static ");

      if (Virtual)
         io.put ("virtual ");

      if (Explicit)
         io.put ("explicit ");

      if (! Constructor && ! Destructor)
      {
         if (Type == "")
            Type = "void";

         io.put (Type);
         io.put (" ");
      }

      if (Precede != "")
      {
         io.put (Precede);
         io.put (" ");
      }

      if (Destructor)
         io.put ("~");

      io.put (Name);

      io.put (" (");
      ParamDecl * item = getFirstParam ();
      while (item != NULL)
      {
         item->writeIntf (io);
         item = item->getNextParam ();
         if (item != NULL) io.put (", ");
      }
      io.put (")");

      if (Follow != "")
      {
         io.put (" ");
         io.put (Follow);
      }

      if (Abstract)
         io.put (" = 0");

      if (Inline)
      {
         writeBody (io);
         // NO io.PutLn (""); // empty line
      }
      else
      {
         io.put (";");
         io.putEol ();
      }

      if (skip_decl)
         io.putLn ("#endif");
   }
}

void Func::writeImpl (TextOutput & io)
{
   if (! Inline && ! no_impl)
   {
      if (skip_impl || skip_decl)
         io.putLn ("#if 0");

      if (Static && sourceCls () == NULL)
         io.put ("static ");

      if (! Constructor && ! Destructor)
      {
         if (Type == "")
            Type = "void";

         io.put (Type);
         io.put (" ");
      }

      if (Precede != "")
      {
         io.put (Precede);
         io.put (" ");
      }

      if (getUpperElem () != NULL)
         getUpperElem () -> writePrefix (io);

      if (Destructor)
         io.put ("~");

      io.put (Name);

      io.put (" (");
      Element * item = Params->getFirstElem ();
      while (item != NULL)
      {
         item->writeImpl (io);
         item = item->getNextElem ();
         if (item != NULL) io.put (", ");
      }
      io.put (")");

      if (Follow != "")
      {
         io.put (" ");
         io.put (Follow);
      }

      writeBody (io);

      if (skip_impl || skip_decl)
         io.putLn ("#endif");

      io.emptyLine (); // empty line
   }
}

/******************************* ENUM TYPE ********************************/

void EnumType::writeIntf (TextOutput & io)
{
   if (Comment != "")
      io.putLn ("// " + Comment);

   if (! no_impl)
   {
      io.put ("enum ");
      io.put (Name);
      io.putEol ();

      io.putLn ("{");
      io.incIndent ();

      Element * item = getFirstElem ();
      while (item != NULL)
      {
         item->writeIntf (io);
         item = item->getNextElem ();
         if (item != NULL) io.put (",");
         io.putEol ();
      }

      io.decIndent ();
      io.putLn ("};");
      io.emptyLine (); // empty line
   }
}

/* ---------------------------------------------------------------------- */

void EnumItem::writeIntf (TextOutput & io)
{
   if (Comment != "")
      io.putLn ("// " + Comment);

   io.put (Name);
   if (Init != "")
   {
      io.put (" = ");
      io.put (Init);
   }
}

/********************************** CLASS *********************************/

void Class::writeIntf (TextOutput & io)
{
   if (foreign_decl) return; // !?

   io.emptyLine ();

   if (Comment != "")
      io.putLn ("// " + Comment);

   io.put ("class ");
   io.put (Name);
   if (Parent != "")
   {
      io.put (" : public ");
      io.put (Parent);

      if (Interface != "")
      {
         io.put (" , public ");
         io.put (Interface);
      }
   }
   io.putEol ();

   io.putLn ("{");
   io.noEmptyLine ();
   io.incIndent ();
   io.incIndent ();

   Access acs = NoAccess;
   int grp = min_group;

   VarietyInfo variety;
   variety.class_symbol = this->variety_symbol;
   variety.back_indent = true;

   Element * item = getFirstElem ();
   while (item != NULL)
   {
      if (item->Group == ctor_group && grp == dtor_group)
         grp = ctor_group; // join dectructor and constructor groups

      if (grp != item->Group)
      {
         io.emptyLine ();
      }

      variety.change_variety (io, item->variety_symbol);

      if (item->access == NoAccess || item->access == OnlyComment)
      {
         io.decIndent ();
         item->writeIntf (io);
         io.incIndent ();
         acs = NoAccess; // print next access mark
      }
      else
      {
         if (acs != item->access || grp != item->Group)
         {
            acs = item->access;

            io.decIndent ();
            if (acs == PrivateAccess)
               io.putLn ("private:");
            else if (acs == ProtectedAccess)
               io.putLn ("protected:");
            if (acs == PublicAccess)
               io.putLn ("public:");
            io.incIndent ();
         }

         item->writeIntf (io);
      }

      grp = item->Group;
      item = item->getNextElem ();
   }

   variety.change_variety (io, "");

   io.decIndent ();
   io.decIndent ();

   io.noEmptyLine ();
   io.putLn ("};");
   io.emptyLine (); // empty line
}

/* ---------------------------------------------------------------------- */

void Class::writePrefix (TextOutput & io)
{
   io.put (Name);
   io.put ("::");
}

void Class::writeInitMembers (TextOutput & io)
{
   bool any = false;
   io.incIndent ();

   {
       bool any_variety = false;
       Element * item = getFirstElem ();
       while (item != NULL)
       {
          if (item->variety_symbol != "" && item->variety_symbol != this->variety_symbol)
             if (item->hasInit ())
                any_variety = true;
          item = item->getNextElem ();
       }
       if (any_variety && Parent != "")
       {
          io.putLn (" :");
          any = true;
          io.put (Parent + " ()"); // !? without parameters
       }
   }

   VarietyInfo variety;
   variety.class_symbol = this->variety_symbol;
   variety.no_empty_line = true;

   Element * item = getFirstElem ();
   while (item != NULL)
   {
      if (item->hasInit ())
      {
         variety.change_variety (io, item->variety_symbol);

         if (! any)
            io.putLn (" :");
         else
            io.putLn (",");

         any = true;

         item->writeInit (io);
      }
      item = item->getNextElem ();
   }

   variety.change_variety (io, "");

   io.decIndent ();
   io.putEol (); // finish previous line
}

void Class::writeImpl (TextOutput & io)
{
   if (foreign_decl) return; // !?

   io.emptyLine ();
   io.putLn ("/* " + Name + " */");
   io.emptyLine ();

   Element::writeImpl (io);
}

/********************************** ATTR **********************************/

void Attr::writeIntf (TextOutput & io)
{
   if (print_decl)
   {
      io.put ("attr <");
      io.put (Type);
      io.put ("> ");
      io.put (Name);
      io.putLn (";");
   }
}

/********************************** LIST **********************************/

void List::writeIntf (TextOutput & io)
{
   if (print_decl)
   {
      io.put ("list <");
      io.put (element);
      io.put ("> ");
      io.put (Name);
      io.putLn (";");
   }
}

/********************************** MODULE *********************************/

void Module::writeInterface ()
{
   if (header_name != "")
   {
      TextOutput io;
      io.open (header_name);
      writeIntf (io);
      io.store ();
   }
}

void Module::writeImplementation ()
{
   if (code_name != "")
   {
      TextOutput io;
      io.open (code_name);
      writeImpl (io);
      io.store ();
   }
}

/* ---------------------------------------------------------------------- */

void Module::writeIntf (TextOutput & io)
{
   if (! imported)
      Element::writeIntf (io);
}

void Module::writeImpl (TextOutput & io)
{
   if (! imported)
      Element::writeImpl (io);
}

void saveElements (string file_name, Element * top)
{
   EnvOutput io;
   io.open (file_name);
   top->writeElement (io);
   io.store ();
}

// CLOSE_NAMESPACE
