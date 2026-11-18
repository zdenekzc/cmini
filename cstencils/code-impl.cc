
/* code-impl.cc */

OPEN_NAMESPACE

/************************ ENUMERATION TYPE FIELDS *************************/

/* TDeclKindField */

#if 0
DeclKindField::DeclKindField () :
   pointer (NULL)
{ }

DeclKind DeclKindField::getValue ()
{
   return * pointer;
}

void DeclKindField::setValue (DeclKind val)
{
   * pointer = val;
}

string DeclKindField::getText ()
{
   DeclKind val = getValue ();
   if (val == intf)
      return "intf";
   if (val == impl)
      return "impl";
   assert (false);
}

void DeclKindField::setText (string val)
{
   if (val == "intf")
   {
      setValue (intf);
      return;
   }
   if (val == "impl")
   {
      setValue (impl);
      return;
   }
   assert (false);
}

DeclKind DeclKindField::defaultValue ()
{
   return intf;
}

void DeclKindField::setPointer (DeclKind * p_pointer)
{
   pointer = p_pointer;
}

void DeclKindField::setVariable (DeclKind & p_variable)
{
   pointer = & p_variable;
}
#endif

/* TTypeKindField */

TypeKindField::TypeKindField () :
   pointer (NULL)
{ }

TypeKind TypeKindField::getValue ()
{
   return * pointer;
}

void TypeKindField::setValue (TypeKind val)
{
   * pointer = val;
}

string TypeKindField::getText ()
{
   TypeKind val = getValue ();
   if (val == bool_type)
      return "bool_type";
   if (val == enum_type)
      return "enum_type";
   if (val == int_type)
      return "int_type";
   if (val == float_type)
      return "float_type";
   if (val == string_type)
      return "string_type";
   if (val == class_type)
      return "class_type";
   if (val == pointer_type)
      return "pointer_type";
   if (val == bitset_type)
      return "bitset_type";
   if (val == unknown_type)
      return "unknown_type";
   assert (false);
}

void TypeKindField::setText (string val)
{
   if (val == "bool_type")
   {
      setValue (bool_type);
      return;
   }
   if (val == "enum_type")
   {
      setValue (enum_type);
      return;
   }
   if (val == "int_type")
   {
      setValue (int_type);
      return;
   }
   if (val == "float_type")
   {
      setValue (float_type);
      return;
   }
   if (val == "string_type")
   {
      setValue (string_type);
      return;
   }
   if (val == "class_type")
   {
      setValue (class_type);
      return;
   }
   if (val == "pointer_type")
   {
      setValue (pointer_type);
      return;
   }
   if (val == "bitset_type")
   {
      setValue (bitset_type);
      return;
   }
   if (val == "unknown_type")
   {
      setValue (unknown_type);
      return;
   }
   assert (false);
}

TypeKind TypeKindField::defaultValue ()
{
   return bool_type;
}

void TypeKindField::setPointer (TypeKind * p_pointer)
{
   pointer = p_pointer;
}

void TypeKindField::setVariable (TypeKind & p_variable)
{
   pointer = & p_variable;
}

/********************************* PROJECT *********************************/

Project::Project () :
   inp_name (),
   header_name (),
   code_name (),

   desc_header_name (),
   desc_code_name (),

   output_directory (),

   only_makefile (false),
   compile_one_module (false),
   compile_project (false),

   count_alternatives (false),
   list_alternatives (false),

   select_alternative (false),
   selected_alternative (0),

   input (),
   // symbols (input.symbols),
   variables (),

   type_table (),
   init_table (),
   warning_table ()
{
   NameParam = Name_Field;
}

void Project::createLocalFields ()
{
   Element::createLocalFields ();

   StrField * inp_name_Field = new StrField;
   inp_name_Field->setName ("inp_name");
   inp_name_Field->setVariable (inp_name);
   this->insertLastField (inp_name_Field);

   StrField * header_name_Field = new StrField;
   header_name_Field->setName ("header_name");
   header_name_Field->setVariable (header_name);
   this->insertLastField (header_name_Field);

   StrField * code_name_Field = new StrField;
   code_name_Field->setName ("code_name");
   code_name_Field->setVariable (code_name);
   this->insertLastField (code_name_Field);

   StrField * desc_header_name_Field = new StrField;
   desc_header_name_Field->setName ("desc_header_name");
   desc_header_name_Field->setVariable (desc_header_name);
   this->insertLastField (desc_header_name_Field);

   StrField * desc_code_name_Field = new StrField;
   desc_code_name_Field->setName ("desc_code_name");
   desc_code_name_Field->setVariable (desc_code_name);
   this->insertLastField (desc_code_name_Field);

   StrField * output_directory_Field = new StrField;
   output_directory_Field->setName ("output_directory");
   output_directory_Field->setVariable (output_directory);
   this->insertLastField (output_directory_Field);

   BoolField * only_makefile_Field = new BoolField;
   only_makefile_Field->setName ("only_makefile");
   only_makefile_Field->setVariable (only_makefile);
   this->insertLastField (only_makefile_Field);

   BoolField * compile_one_module_Field = new BoolField;
   compile_one_module_Field->setName ("compile_one_module");
   compile_one_module_Field->setVariable (compile_one_module);
   this->insertLastField (compile_one_module_Field);

   BoolField * compile_project_Field = new BoolField;
   compile_project_Field->setName ("compile_project");
   compile_project_Field->setVariable (compile_project);
   this->insertLastField (compile_project_Field);

   BoolField * count_alternatives_Field = new BoolField;
   count_alternatives_Field->setName ("count_alternatives");
   count_alternatives_Field->setVariable (count_alternatives);
   this->insertLastField (count_alternatives_Field);

   BoolField * list_alternatives_Field = new BoolField;
   list_alternatives_Field->setName ("list_alternatives");
   list_alternatives_Field->setVariable (list_alternatives);
   this->insertLastField (list_alternatives_Field);

   BoolField * select_alternative_Field = new BoolField;
   select_alternative_Field->setName ("select_alternative");
   select_alternative_Field->setVariable (select_alternative);
   this->insertLastField (select_alternative_Field);

   IntField * selected_alternative_Field = new IntField;
   selected_alternative_Field->setName ("selected_alternative");
   selected_alternative_Field->setVariable (selected_alternative);
   this->insertLastField (selected_alternative_Field);
}

bool Project::readField (EnvInput & io)
{
   if (io.isIdent ("inp_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      inp_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("header_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      header_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("code_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      code_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("desc_header_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      desc_header_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("desc_code_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      desc_code_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("output_directory"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      output_directory = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("only_makefile"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      only_makefile = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("compile_one_module"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      compile_one_module = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("compile_project"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      compile_project = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("count_alternatives"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      count_alternatives = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("list_alternatives"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      list_alternatives = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("select_alternative"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      select_alternative = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("selected_alternative"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      selected_alternative = io.readInt ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}



/********************************** MODULE *********************************/

Module::Module () :
   automatic_namespace (false),
   module_namespace (),

   export_symbols (false),
   export_symbols_to_impl (false),

   only_header (false),
   generate_cpp (false),

   source_file_name (),
   header_name (),
   code_name (),

   desc_header_name (),
   desc_code_name (),

   no_class_modif (false),
   only_variables (false),

   keep_methods (false),
   experimental (false),

   generate_components (false),
   factory_list (false),

   init_func (NULL),
   fact_point (NULL)
{
   NameParam = Name_Field;
   createFields (); // !? gtk mix
}

void Module::createLocalFields ()
{
   Element::createLocalFields ();

   BoolField * automatic_namespace_Field = new BoolField;
   automatic_namespace_Field->setName ("automatic_namespace");
   automatic_namespace_Field->setVariable (automatic_namespace);
   this->insertLastField (automatic_namespace_Field);

   StrField * module_namespace_Field = new StrField;
   module_namespace_Field->setName ("module_namespace");
   module_namespace_Field->setVariable (module_namespace);
   this->insertLastField (module_namespace_Field);

   BoolField * export_symbols_Field = new BoolField;
   export_symbols_Field->setName ("export_symbols");
   export_symbols_Field->setVariable (export_symbols);
   this->insertLastField (export_symbols_Field);

   BoolField * export_symbols_to_impl_Field = new BoolField;
   export_symbols_to_impl_Field->setName ("export_symbols_to_impl");
   export_symbols_to_impl_Field->setVariable (export_symbols_to_impl);
   this->insertLastField (export_symbols_to_impl_Field);

   BoolField * only_header_Field = new BoolField;
   only_header_Field->setName ("only_header");
   only_header_Field->setVariable (only_header);
   this->insertLastField (only_header_Field);

   BoolField * generate_cpp_Field = new BoolField;
   generate_cpp_Field->setName ("generate_cpp");
   generate_cpp_Field->setVariable (generate_cpp);
   this->insertLastField (generate_cpp_Field);

   StrField * source_file_name_Field = new StrField;
   source_file_name_Field->setName ("source_file_name");
   source_file_name_Field->setVariable (source_file_name);
   this->insertLastField (source_file_name_Field);

   StrField * header_name_Field = new StrField;
   header_name_Field->setName ("header_name");
   header_name_Field->setVariable (header_name);
   this->insertLastField (header_name_Field);

   StrField * code_name_Field = new StrField;
   code_name_Field->setName ("code_name");
   code_name_Field->setVariable (code_name);
   this->insertLastField (code_name_Field);

   BoolField * no_class_modif_Field = new BoolField;
   no_class_modif_Field->setName ("no_class_modif");
   no_class_modif_Field->setVariable (no_class_modif);
   this->insertLastField (no_class_modif_Field);

   BoolField * only_variables_Field = new BoolField;
   only_variables_Field->setName ("only_variables");
   only_variables_Field->setVariable (only_variables);
   this->insertLastField (only_variables_Field);

   BoolField * keep_methods_Field = new BoolField;
   keep_methods_Field->setName ("keep_methods");
   keep_methods_Field->setVariable (keep_methods);
   this->insertLastField (keep_methods_Field);

   BoolField * experimental_Field = new BoolField;
   experimental_Field->setName ("experimental");
   experimental_Field->setVariable (experimental);
   this->insertLastField (experimental_Field);

   BoolField * generate_components_Field = new BoolField;
   generate_components_Field->setName ("generate_components");
   generate_components_Field->setVariable (generate_components);
   this->insertLastField (generate_components_Field);

   BoolField * factory_list_Field = new BoolField;
   factory_list_Field->setName ("factory_list");
   factory_list_Field->setVariable (factory_list);
   this->insertLastField (factory_list_Field);
}

bool Module::readField (EnvInput & io)
{
   if (io.isIdent ("automatic_namespace"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      automatic_namespace = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("module_namespace"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      module_namespace = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("export_symbols"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      export_symbols = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("export_symbols_to_impl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      export_symbols_to_impl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("only_header"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      only_header = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("generate_cpp"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      generate_cpp = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("source_file_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      source_file_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("header_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      header_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("code_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      code_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_class_modif"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_class_modif = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("only_variables"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      only_variables = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("keep_methods"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      keep_methods = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("experimental"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      experimental = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("generate_components"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      generate_components = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("factory_list"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      factory_list = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}

/******************************* ENUM TYPE ********************************/

EnumType::EnumType () :
   no_impl (false)
{ }

void EnumType::createLocalFields ()
{
   Element::createLocalFields ();

   BoolField * no_impl_Field = new BoolField;
   no_impl_Field->setName ("no_impl");
   no_impl_Field->setVariable (no_impl);
   this->insertLastField (no_impl_Field);
}

bool EnumType::readField (EnvInput & io)
{
   if (io.isIdent ("no_impl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_impl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}


/******************************* ENUM ITEM ********************************/
EnumItem::EnumItem () :
   Init ()
{ }

void EnumItem::createLocalFields ()
{
   Element::createLocalFields ();

   StrField * Init_Field = new StrField;
   Init_Field->setName ("Init");
   Init_Field->setVariable (Init);
   this->insertLastField (Init_Field);
}

bool EnumItem::readField (EnvInput & io)
{
   if (io.isIdent ("Init"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Init = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}

/********************************** CLASS *********************************/

Class::Class () :
   Parent (),
   Interface (),
   no_automatic_constructor (false),
   auxiliary_constructor (false),
   virtual_base (false),
   virtual_destructor (false),
   no_component (false),
   no_forward (false),
   foreign_decl (false),
   interface_class (false),

   Tooltip (),
   ImageName (),
   SelectedImageName (),
   PageName (),
   OnlyImage (false),
   Alias (),
   AliasGroup (),
   TypeAttr (),
   NameAttr (),
   FromAttr (),

   Parent_Field (NULL),

   init_func (NULL),
   cleanup_func (NULL),
   component_link (NULL),
   component_remove (NULL),
   no_description (false),
   read_access (PrivateAccess), // %%
   parent_cls (NULL)
{
   Parent_Field = new StrField;
   Parent_Field->setName ("Parent");
   Parent_Field->setVariable (Parent);
   this->insertLastField (Parent_Field);

   NameParam = Name_Field;
   FromParam = Parent_Field;
}

void Class::createLocalFields ()
{
   Element::createLocalFields ();

   StrField * Interface_Field = new StrField;
   Interface_Field->setName ("Interface");
   Interface_Field->setVariable (Interface);
   this->insertLastField (Interface_Field);

   BoolField * no_automatic_constructor_Field = new BoolField;
   no_automatic_constructor_Field->setName ("no_automatic_constructor");
   no_automatic_constructor_Field->setVariable (no_automatic_constructor);
   this->insertLastField (no_automatic_constructor_Field);

   BoolField * auxiliary_constructor_Field = new BoolField;
   auxiliary_constructor_Field->setName ("auxiliary_constructor");
   auxiliary_constructor_Field->setVariable (auxiliary_constructor);
   this->insertLastField (auxiliary_constructor_Field);

   BoolField * virtual_base_Field = new BoolField;
   virtual_base_Field->setName ("virtual_base");
   virtual_base_Field->setVariable (virtual_base);
   this->insertLastField (virtual_base_Field);

   BoolField * virtual_destructor_Field = new BoolField;
   virtual_destructor_Field->setName ("virtual_destructor");
   virtual_destructor_Field->setVariable (virtual_destructor);
   this->insertLastField (virtual_destructor_Field);

   BoolField * no_component_Field = new BoolField;
   no_component_Field->setName ("no_component");
   no_component_Field->setVariable (no_component);
   this->insertLastField (no_component_Field);

   BoolField * no_forward_Field = new BoolField;
   no_forward_Field->setName ("no_forward");
   no_forward_Field->setVariable (no_forward);
   this->insertLastField (no_forward_Field);

   BoolField * foreign_decl_Field = new BoolField;
   foreign_decl_Field->setName ("foreign_decl");
   foreign_decl_Field->setVariable (foreign_decl);
   this->insertLastField (foreign_decl_Field);

   StrField * Tooltip_Field = new StrField;
   Tooltip_Field->setName ("Tooltip");
   Tooltip_Field->setVariable (Tooltip);
   this->insertLastField (Tooltip_Field);

   StrField * ImageName_Field = new StrField;
   ImageName_Field->setName ("ImageName");
   ImageName_Field->setVariable (ImageName);
   this->insertLastField (ImageName_Field);

   StrField * SelectedImageName_Field = new StrField;
   SelectedImageName_Field->setName ("SelectedImageName");
   SelectedImageName_Field->setVariable (SelectedImageName);
   this->insertLastField (SelectedImageName_Field);

   StrField * PageName_Field = new StrField;
   PageName_Field->setName ("PageName");
   PageName_Field->setVariable (PageName);
   this->insertLastField (PageName_Field);

   BoolField * OnlyImage_Field = new BoolField;
   OnlyImage_Field->setName ("OnlyImage");
   OnlyImage_Field->setVariable (OnlyImage);
   this->insertLastField (OnlyImage_Field);

   StrField * Alias_Field = new StrField;
   Alias_Field->setName ("Alias");
   Alias_Field->setVariable (Alias);
   this->insertLastField (Alias_Field);

   StrField * AliasGroup_Field = new StrField;
   AliasGroup_Field->setName ("AliasGroup");
   AliasGroup_Field->setVariable (AliasGroup);
   this->insertLastField (AliasGroup_Field);

   StrField * TypeAttr_Field = new StrField;
   TypeAttr_Field->setName ("TypeAttr");
   TypeAttr_Field->setVariable (TypeAttr);
   this->insertLastField (TypeAttr_Field);

   StrField * NameAttr_Field = new StrField;
   NameAttr_Field->setName ("NameAttr");
   NameAttr_Field->setVariable (NameAttr);
   this->insertLastField (NameAttr_Field);

   StrField * FromAttr_Field = new StrField;
   FromAttr_Field->setName ("FromAttr");
   FromAttr_Field->setVariable (FromAttr);
   this->insertLastField (FromAttr_Field);
}

bool Class::readField (EnvInput & io)
{
   if (io.isIdent ("Parent"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Parent = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Interface"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Interface = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_automatic_constructor"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_automatic_constructor = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("auxiliary_constructor"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      auxiliary_constructor = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("virtual_base"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      virtual_base = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("virtual_destructor"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      virtual_destructor = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_component"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_component = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_forward"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_forward = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("foreign_decl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      foreign_decl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Tooltip"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Tooltip = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("ImageName"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      ImageName = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("SelectedImageName"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      SelectedImageName = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("PageName"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      PageName = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("OnlyImage"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      OnlyImage = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Alias"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Alias = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("AliasGroup"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      AliasGroup = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("TypeAttr"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      TypeAttr = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("NameAttr"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      NameAttr = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("FromAttr"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      FromAttr = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}

/********************************** ATTR **********************************/

Attr::Attr () :
   Type (),
   Init (),
   FieldType (),
   only_variable (false),
   no_comment (false),
   no_variable (false),
   no_compare (false),
   protected_variable (false),
   public_variable (false),
   no_get_method (false),
   no_set_method (false),
   no_get_impl (false),
   no_set_impl (false),
   empty_get_impl (false),
   empty_set_impl (false),
   virtual_get (false),
   virtual_set (false),
   abstract_get (false),
   abstract_set (false),
   no_inline_get (false),
   inline_set (false),
   field_variable (false),

   print_decl (false),

   Type_Field (NULL),

   field_id (),
   get_id (),
   set_id (),

   get_func (NULL),
   set_func (NULL),

   get_point (NULL),
   set_point (NULL),

   compare_flag_name (),
   compare_flag_value (),

   source_cls (NULL),
   following_item (NULL)

{
   Type_Field = new StrField;
   Type_Field->setName ("Type");
   Type_Field->setVariable (Type);
   this->insertLastField (Type_Field);

   TypeParam = Type_Field;
   NameParam = Name_Field;

   createFields (); // !? gtk mix
}

void Attr::createLocalFields ()
{
   Element::createLocalFields ();

   StrField * Init_Field = new StrField;
   Init_Field->setName ("Init");
   Init_Field->setVariable (Init);
   this->insertLastField (Init_Field);

   StrField * FieldType_Field = new StrField;
   FieldType_Field->setName ("FieldType");
   FieldType_Field->setVariable (FieldType);
   this->insertLastField (FieldType_Field);

   BoolField * only_variable_Field = new BoolField;
   only_variable_Field->setName ("only_variable");
   only_variable_Field->setVariable (only_variable);
   this->insertLastField (only_variable_Field);

   BoolField * no_comment_Field = new BoolField;
   no_comment_Field->setName ("no_comment");
   no_comment_Field->setVariable (no_comment);
   this->insertLastField (no_comment_Field);

   BoolField * no_variable_Field = new BoolField;
   no_variable_Field->setName ("no_variable");
   no_variable_Field->setVariable (no_variable);
   this->insertLastField (no_variable_Field);

   BoolField * no_compare_Field = new BoolField;
   no_compare_Field->setName ("no_compare");
   no_compare_Field->setVariable (no_compare);
   this->insertLastField (no_compare_Field);

   BoolField * protected_variable_Field = new BoolField;
   protected_variable_Field->setName ("protected_variable");
   protected_variable_Field->setVariable (protected_variable);
   this->insertLastField (protected_variable_Field);

   BoolField * public_variable_Field = new BoolField;
   public_variable_Field->setName ("public_variable");
   public_variable_Field->setVariable (public_variable);
   this->insertLastField (public_variable_Field);

   BoolField * no_get_method_Field = new BoolField;
   no_get_method_Field->setName ("no_get_method");
   no_get_method_Field->setVariable (no_get_method);
   this->insertLastField (no_get_method_Field);

   BoolField * no_set_method_Field = new BoolField;
   no_set_method_Field->setName ("no_set_method");
   no_set_method_Field->setVariable (no_set_method);
   this->insertLastField (no_set_method_Field);

   BoolField * no_get_impl_Field = new BoolField;
   no_get_impl_Field->setName ("no_get_impl");
   no_get_impl_Field->setVariable (no_get_impl);
   this->insertLastField (no_get_impl_Field);

   BoolField * no_set_impl_Field = new BoolField;
   no_set_impl_Field->setName ("no_set_impl");
   no_set_impl_Field->setVariable (no_set_impl);
   this->insertLastField (no_set_impl_Field);

   BoolField * empty_get_impl_Field = new BoolField;
   empty_get_impl_Field->setName ("empty_get_impl");
   empty_get_impl_Field->setVariable (empty_get_impl);
   this->insertLastField (empty_get_impl_Field);

   BoolField * empty_set_impl_Field = new BoolField;
   empty_set_impl_Field->setName ("empty_set_impl");
   empty_set_impl_Field->setVariable (empty_set_impl);
   this->insertLastField (empty_set_impl_Field);

   BoolField * virtual_get_Field = new BoolField;
   virtual_get_Field->setName ("virtual_get");
   virtual_get_Field->setVariable (virtual_get);
   this->insertLastField (virtual_get_Field);

   BoolField * virtual_set_Field = new BoolField;
   virtual_set_Field->setName ("virtual_set");
   virtual_set_Field->setVariable (virtual_set);
   this->insertLastField (virtual_set_Field);

   BoolField * abstract_get_Field = new BoolField;
   abstract_get_Field->setName ("abstract_get");
   abstract_get_Field->setVariable (abstract_get);
   this->insertLastField (abstract_get_Field);

   BoolField * abstract_set_Field = new BoolField;
   abstract_set_Field->setName ("abstract_set");
   abstract_set_Field->setVariable (abstract_set);
   this->insertLastField (abstract_set_Field);

   BoolField * no_inline_get_Field = new BoolField;
   no_inline_get_Field->setName ("no_inline_get");
   no_inline_get_Field->setVariable (no_inline_get);
   this->insertLastField (no_inline_get_Field);

   BoolField * inline_set_Field = new BoolField;
   inline_set_Field->setName ("inline_set");
   inline_set_Field->setVariable (inline_set);
   this->insertLastField (inline_set_Field);

   BoolField * field_variable_Field = new BoolField;
   field_variable_Field->setName ("field_variable");
   field_variable_Field->setVariable (field_variable);
   this->insertLastField (field_variable_Field);

   BoolField * print_decl_Field = new BoolField;
   print_decl_Field->setName ("print_decl");
   print_decl_Field->setVariable (print_decl);
   this->insertLastField (print_decl_Field);
}

bool Attr::readField (EnvInput & io)
{
   if (io.isIdent ("Type"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Type = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Init"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Init = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("FieldType"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      FieldType = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("only_variable"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      only_variable = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_comment"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_comment = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_variable"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_variable = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_compare"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_compare = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("protected_variable"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      protected_variable = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("public_variable"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      public_variable = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_get_method"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_get_method = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_set_method"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_set_method = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_get_impl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_get_impl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_set_impl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_set_impl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("empty_get_impl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      empty_get_impl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("empty_set_impl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      empty_set_impl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("virtual_get"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      virtual_get = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("virtual_set"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      virtual_set = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("abstract_get"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      abstract_get = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("abstract_set"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      abstract_set = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_inline_get"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_inline_get = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("inline_set"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      inline_set = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("field_variable"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      field_variable = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("print_decl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      print_decl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}

/********************************** LIST **********************************/

List::List () :

   Element (),
   Prefix (),
   Suffix1 (),
   Suffix2 (),
   Get (),
   First (),
   Last (),
   Prev (),
   Next (),
   Up (),
   InsertFirst (),
   InsertLast (),
   InsertBefore (),
   InsertAfter (),
   Link (),
   Remove (),
   RemoveAll (),

   virtual_link (false),
   virtual_remove (false),
   public_variables (false),
   no_release (false),

   attach_notification (),
   detach_notification (),

   insert_notification (),
   remove_notification (),

   restricted (false),
   combined (false),
   component_list (false),
   print_decl (false),

   Element_Field (NULL),

   first_id (),
   last_id (),
   prev_id (),
   next_id (),
   up_id (),
   get_first_id (),
   get_last_id (),
   get_prev_id (),
   get_next_id (),
   get_up_id (),
   ins_first_id (),
   ins_last_id (),
   ins_before_id (),
   ins_after_id (),
   link_id (),
   remove_id (),
   remove_all_id (),

   first_var (NULL),
   last_var (NULL),
   prev_var (NULL),
   next_var (NULL),
   up_var (NULL),
   get_first_func (NULL),
   get_last_func (NULL),
   get_prev_func (NULL),
   get_next_func (NULL),
   get_up_func (NULL),
   ins_first_func (NULL),
   ins_last_func (NULL),
   ins_before_func (NULL),
   ins_after_func (NULL),
   link_func (NULL),
   remove_func (NULL),
   remove_all_func (NULL),

   check_point (NULL),
   ins_point (NULL),
   rem_point (NULL),
   final_rem_point (NULL),

   source_cls (NULL),
   element_cls (NULL),

   following_item (NULL)
{
   Element_Field = new StrField;
   Element_Field->setName ("Element");
   Element_Field->setVariable (element);
   this->insertLastField (Element_Field);

   TypeParam = Element_Field;
   NameParam = Name_Field;

   createFields (); // !? gtk mix
}

void List::createLocalFields ()
{
   Element::createLocalFields ();

   StrField * Prefix_Field = new StrField;
   Prefix_Field->setName ("Prefix");
   Prefix_Field->setVariable (Prefix);
   this->insertLastField (Prefix_Field);

   StrField * Suffix1_Field = new StrField;
   Suffix1_Field->setName ("Suffix1");
   Suffix1_Field->setVariable (Suffix1);
   this->insertLastField (Suffix1_Field);

   StrField * Suffix2_Field = new StrField;
   Suffix2_Field->setName ("Suffix2");
   Suffix2_Field->setVariable (Suffix2);
   this->insertLastField (Suffix2_Field);

   StrField * Get_Field = new StrField;
   Get_Field->setName ("Get");
   Get_Field->setVariable (Get);
   this->insertLastField (Get_Field);

   StrField * First_Field = new StrField;
   First_Field->setName ("First");
   First_Field->setVariable (First);
   this->insertLastField (First_Field);

   StrField * Last_Field = new StrField;
   Last_Field->setName ("Last");
   Last_Field->setVariable (Last);
   this->insertLastField (Last_Field);

   StrField * Prev_Field = new StrField;
   Prev_Field->setName ("Prev");
   Prev_Field->setVariable (Prev);
   this->insertLastField (Prev_Field);

   StrField * Next_Field = new StrField;
   Next_Field->setName ("Next");
   Next_Field->setVariable (Next);
   this->insertLastField (Next_Field);

   StrField * Up_Field = new StrField;
   Up_Field->setName ("Up");
   Up_Field->setVariable (Up);
   this->insertLastField (Up_Field);

   StrField * InsertFirst_Field = new StrField;
   InsertFirst_Field->setName ("InsertFirst");
   InsertFirst_Field->setVariable (InsertFirst);
   this->insertLastField (InsertFirst_Field);

   StrField * InsertLast_Field = new StrField;
   InsertLast_Field->setName ("InsertLast");
   InsertLast_Field->setVariable (InsertLast);
   this->insertLastField (InsertLast_Field);

   StrField * InsertBefore_Field = new StrField;
   InsertBefore_Field->setName ("InsertBefore");
   InsertBefore_Field->setVariable (InsertBefore);
   this->insertLastField (InsertBefore_Field);

   StrField * InsertAfter_Field = new StrField;
   InsertAfter_Field->setName ("InsertAfter");
   InsertAfter_Field->setVariable (InsertAfter);
   this->insertLastField (InsertAfter_Field);

   StrField * Link_Field = new StrField;
   Link_Field->setName ("Link");
   Link_Field->setVariable (Link);
   this->insertLastField (Link_Field);

   StrField * Remove_Field = new StrField;
   Remove_Field->setName ("Remove");
   Remove_Field->setVariable (Remove);
   this->insertLastField (Remove_Field);

   StrField * RemoveAll_Field = new StrField;
   RemoveAll_Field->setName ("RemoveAll");
   RemoveAll_Field->setVariable (RemoveAll);
   this->insertLastField (RemoveAll_Field);

   BoolField * virtual_link_Field = new BoolField;
   virtual_link_Field->setName ("virtual_link");
   virtual_link_Field->setVariable (virtual_link);
   this->insertLastField (virtual_link_Field);

   BoolField * virtual_remove_Field = new BoolField;
   virtual_remove_Field->setName ("virtual_remove");
   virtual_remove_Field->setVariable (virtual_remove);
   this->insertLastField (virtual_remove_Field);

   BoolField * public_variables_Field = new BoolField;
   public_variables_Field->setName ("public_variables");
   public_variables_Field->setVariable (public_variables);
   this->insertLastField (public_variables_Field);

   BoolField * no_release_Field = new BoolField;
   no_release_Field->setName ("no_release");
   no_release_Field->setVariable (no_release);
   this->insertLastField (no_release_Field);

   StrField * attach_notification_Field = new StrField;
   attach_notification_Field->setName ("attach_notification");
   attach_notification_Field->setVariable (attach_notification);
   this->insertLastField (attach_notification_Field);

   StrField * detach_notification_Field = new StrField;
   detach_notification_Field->setName ("detach_notification");
   detach_notification_Field->setVariable (detach_notification);
   this->insertLastField (detach_notification_Field);

   StrField * insert_notification_Field = new StrField;
   insert_notification_Field->setName ("insert_notification");
   insert_notification_Field->setVariable (insert_notification);
   this->insertLastField (insert_notification_Field);

   StrField * remove_notification_Field = new StrField;
   remove_notification_Field->setName ("remove_notification");
   remove_notification_Field->setVariable (remove_notification);
   this->insertLastField (remove_notification_Field);

   BoolField * restricted_Field = new BoolField;
   restricted_Field->setName ("restricted");
   restricted_Field->setVariable (restricted);
   this->insertLastField (restricted_Field);

   BoolField * combined_Field = new BoolField;
   combined_Field->setName ("combined");
   combined_Field->setVariable (combined);
   this->insertLastField (combined_Field);

   BoolField * component_list_Field = new BoolField;
   component_list_Field->setName ("component_list");
   component_list_Field->setVariable (component_list);
   this->insertLastField (component_list_Field);

   BoolField * print_decl_Field = new BoolField;
   print_decl_Field->setName ("print_decl");
   print_decl_Field->setVariable (print_decl);
   this->insertLastField (print_decl_Field);
}

bool List::readField (EnvInput & io)
{
   if (io.isIdent ("Element"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      element = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Prefix"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Prefix = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Suffix1"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Suffix1 = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Suffix2"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Suffix2 = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Get"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Get = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("First"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      First = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Last"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Last = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Prev"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Prev = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Next"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Next = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Up"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Up = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("InsertFirst"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      InsertFirst = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("InsertLast"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      InsertLast = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("InsertBefore"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      InsertBefore = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("InsertAfter"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      InsertAfter = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Link"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Link = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Remove"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Remove = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("RemoveAll"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      RemoveAll = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("virtual_link"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      virtual_link = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("virtual_remove"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      virtual_remove = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("public_variables"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      public_variables = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_release"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_release = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("attach_notification"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      attach_notification = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("detach_notification"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      detach_notification = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("insert_notification"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      insert_notification = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("remove_notification"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      remove_notification = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("restricted"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      restricted = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("combined"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      combined = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("component_list"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      component_list = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("print_decl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      print_decl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}

/************************** VARIABLE DECLARATION **************************/

Var::Var () :
   Type (),
   Precede (),
   Follow (),
   Init (),

   Static (false),
   Extern (false),

   SimpleInit (false)
{
   NameParam = Name_Field;
   createFields (); // !? gtk mix
}


void Var::createLocalFields ()
{
   Element::createLocalFields ();

   StrField * Type_Field = new StrField;
   Type_Field->setName ("Type");
   Type_Field->setVariable (Type);
   this->insertLastField (Type_Field);

   StrField * Precede_Field = new StrField;
   Precede_Field->setName ("Precede");
   Precede_Field->setVariable (Precede);
   this->insertLastField (Precede_Field);

   StrField * Follow_Field = new StrField;
   Follow_Field->setName ("Follow");
   Follow_Field->setVariable (Follow);
   this->insertLastField (Follow_Field);

   StrField * Init_Field = new StrField;
   Init_Field->setName ("Init");
   Init_Field->setVariable (Init);
   this->insertLastField (Init_Field);

   BoolField * Static_Field = new BoolField;
   Static_Field->setName ("Static");
   Static_Field->setVariable (Static);
   this->insertLastField (Static_Field);

   BoolField * Extern_Field = new BoolField;
   Extern_Field->setName ("Extern");
   Extern_Field->setVariable (Extern);
   this->insertLastField (Extern_Field);

   BoolField * SimpleInit_Field = new BoolField;
   SimpleInit_Field->setName ("SimpleInit");
   SimpleInit_Field->setVariable (SimpleInit);
   this->insertLastField (SimpleInit_Field);
}

bool Var::readField (EnvInput & io)
{
   if (io.isIdent ("Type"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Type = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Precede"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Precede = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Follow"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Follow = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Init"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Init = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Static"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Static = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Extern"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Extern = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("SimpleInit"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      SimpleInit = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}

/************************** FUNCTION DECLARATION **************************/

Func::Func () :
   Type (),
   Precede (),
   Follow (),
   Constructor (false),
   Destructor (false),
   Signal (false),
   Slot (false),
   Inline (false),
   Static (false),
   Extern (false),
   Virtual (false),
   Explicit (false),
   // Overide (false),
   // Overload (false),
   Abstract (false),
   no_decl (false),
   no_impl (false),
   skip_decl (false),
   skip_impl (false),
   basic_method (false),
   Params (new ParamList),
   modif_point (this), // %%
   source_cls (NULL)
{
   NameParam = Name_Field;
   createFields (); // !? gtk mix
}

void Func::createLocalFields ()
{
   Element::createLocalFields ();

   StrField * Type_Field = new StrField;
   Type_Field->setName ("Type");
   Type_Field->setVariable (Type);
   this->insertLastField (Type_Field);

   StrField * Precede_Field = new StrField;
   Precede_Field->setName ("Precede");
   Precede_Field->setVariable (Precede);
   this->insertLastField (Precede_Field);

   StrField * Follow_Field = new StrField;
   Follow_Field->setName ("Follow");
   Follow_Field->setVariable (Follow);
   this->insertLastField (Follow_Field);

   BoolField * Signal_Field = new BoolField;
   Signal_Field->setName ("Signal");
   Signal_Field->setVariable (Signal);
   this->insertLastField (Signal_Field);

   BoolField * Slot_Field = new BoolField;
   Slot_Field->setName ("Slot");
   Slot_Field->setVariable (Slot);
   this->insertLastField (Slot_Field);

   BoolField * Constructor_Field = new BoolField;
   Constructor_Field->setName ("Constructor");
   Constructor_Field->setVariable (Constructor);
   this->insertLastField (Constructor_Field);

   BoolField * Destructor_Field = new BoolField;
   Destructor_Field->setName ("Destructor");
   Destructor_Field->setVariable (Destructor);
   this->insertLastField (Destructor_Field);

   BoolField * Inline_Field = new BoolField;
   Inline_Field->setName ("Inline");
   Inline_Field->setVariable (Inline);
   this->insertLastField (Inline_Field);

   BoolField * Static_Field = new BoolField;
   Static_Field->setName ("Static");
   Static_Field->setVariable (Static);
   this->insertLastField (Static_Field);

   BoolField * Extern_Field = new BoolField;
   Extern_Field->setName ("Extern");
   Extern_Field->setVariable (Extern);
   this->insertLastField (Extern_Field);

   BoolField * Virtual_Field = new BoolField;
   Virtual_Field->setName ("Virtual");
   Virtual_Field->setVariable (Virtual);
   this->insertLastField (Virtual_Field);

   BoolField * Explicit_Field = new BoolField;
   Explicit_Field->setName ("Explicit");
   Explicit_Field->setVariable (Explicit);
   this->insertLastField (Explicit_Field);

   BoolField * Abstract_Field = new BoolField;
   Abstract_Field->setName ("Abstract");
   Abstract_Field->setVariable (Abstract);
   this->insertLastField (Abstract_Field);

   BoolField * no_decl_Field = new BoolField;
   no_decl_Field->setName ("no_decl");
   no_decl_Field->setVariable (no_decl);
   this->insertLastField (no_decl_Field);

   BoolField * no_impl_Field = new BoolField;
   no_impl_Field->setName ("no_impl");
   no_impl_Field->setVariable (no_impl);
   this->insertLastField (no_impl_Field);

   BoolField * skip_decl_Field = new BoolField;
   skip_decl_Field->setName ("skip_decl");
   skip_decl_Field->setVariable (skip_decl);
   this->insertLastField (skip_decl_Field);

   BoolField * skip_impl_Field = new BoolField;
   skip_impl_Field->setName ("skip_impl");
   skip_impl_Field->setVariable (skip_impl);
   this->insertLastField (skip_impl_Field);

   BoolField * basic_method_Field = new BoolField;
   basic_method_Field->setName ("basic_method");
   basic_method_Field->setVariable (basic_method);
   this->insertLastField (basic_method_Field);
}

bool Func::readField (EnvInput & io)
{
   if (io.isIdent ("Type"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Type = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Precede"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Precede = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Follow"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Follow = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Signal"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Signal = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Slot"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Slot = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Constructor"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Constructor = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Destructor"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Destructor = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Inline"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Inline = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Static"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Static = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Extern"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Extern = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Virtual"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Virtual = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Explicit"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Explicit = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Abstract"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Abstract = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_decl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_decl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("no_impl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      no_impl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("skip_decl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      skip_decl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("skip_impl"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      skip_impl = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("basic_method"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      basic_method = io.readBool ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}

/************************** PARAMETER DECLARATION *************************/

ParamDecl::ParamDecl () :
   Type (),
   Init (),
   user_name ()
{
   createFields (); // !? gtk mix
}

void ParamDecl::createLocalFields ()
{
   Element::createLocalFields ();

   StrField * Type_Field = new StrField;
   Type_Field->setName ("Type");
   Type_Field->setVariable (Type);
   this->insertLastField (Type_Field);

   StrField * Init_Field = new StrField;
   Init_Field->setName ("Init");
   Init_Field->setVariable (Init);
   this->insertLastField (Init_Field);

   StrField * user_name_Field = new StrField;
   user_name_Field->setName ("user_name");
   user_name_Field->setVariable (user_name);
   this->insertLastField (user_name_Field);
}

bool ParamDecl::readField (EnvInput & io)
{
   if (io.isIdent ("Type"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Type = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("Init"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      Init = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else if (io.isIdent ("user_name"))
   {
      io.getToken ();
      io.checkSeparator ('=');
      user_name = io.readValue ();
      io.checkSeparator (';');
      return true;
   }
   else return Element::readField (io);
}


/***************************** EXPORT SYMBOL ******************************/

ExportSymbol::ExportSymbol ()
{
   NameParam = Name_Field;
}

/****************************** TYPE INFO *********************************/

TypeInfo::TypeInfo () :
   kind (bool_type),
   kind_Field (NULL)
{
   #if 1
   kind_Field = new TypeKindField;
   kind_Field->setName ("kind");
   kind_Field->setVariable (kind);
   this->insertLastField (kind_Field);
   #endif

   NameParam = Name_Field;
}

/********************************** STYLE *********************************/

Styler::Styler () :
   Element (),
   Element_Field (NULL)
{
   Element_Field = new StrField;
   Element_Field->setName ("Element");
   Element_Field->setVariable (element);
   this->insertLastField (Element_Field);

   TypeParam = Element_Field;
   NameParam = Name_Field;
}

CLOSE_NAMESPACE