
/* code.h */

#ifndef __PROG_CODE_H__
#define __PROG_CODE_H__

#include "std.h"
#include "envio.h"
#include "collection.h"

#include "elem.h"
#include "field.h"
#include "groups.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

class Project;
class Module;
class EnumType;
class EnumItem;
class Class;
class Attr;
class List;
class Expr;
class ExprList;
class Stat;
class StatList;
class ParamDecl;
class ParamList;
class SimpleDecl;
class Var;
class Func;

class ClassPtrField;
class AttrPtrField;
class ListPtrField;

class DeclKindField;
class TypeKindField;

/* ---------------------------------------------------------------------- */

typedef Element ProgItem; // !?

enum DeclKind
{
   intf,
   impl
};

enum TypeKind { bool_type,
                 enum_type,
                 int_type,
                 float_type,
                 string_type,
                 class_type,
                 pointer_type,
                 bitset_type, // bit set stored as integer
                 unknown_type };

/* ---------------------------------------------------------------------- */

class Project : public Element
{
   public:
      string inp_name;    // input file name
      string header_name; // output header file name
      string code_name;   // output code file name

      string desc_header_name; // output description header file name
      string desc_code_name;   // output description code file name

      string output_directory;

      bool only_makefile; // generate only Makefile or Makefile.am, configure.in, autoinit, autoclean and .kdevprj

      bool compile_one_module;
      bool compile_project;

      bool count_alternatives;
      bool list_alternatives;

      bool select_alternative;
      int  selected_alternative;

   public:
      EnvInput input;
      // TCondSymbols & symbols;
      EnvVariables variables;

   public:
      bool isDefinedSymbol (string name) { return input.symbols.isDefinedSymbol (name); }
      void defineSymbolWithParams (string name, string params, string value) { input.symbols.defineSymbolWithParams (name, params, value); }
      void defineSymbol (string name, string value) { input.symbols.defineSymbol (name, value); }
      void undefineSymbol (string name) { input.symbols.undefineSymbol (name); }

      void addVariable (string name, string value) { variables.addVariable (name, value); }
      string recallVariable (string name) { return variables.recallVariable (name); }

   public:
      Module * searchModule (string id);
      Module * findModule (string id);
      Module * getFirstModule ();

   #if 1
   // types and init values
   private:
      Dictionary <string, TypeKind> type_table;
      StringDictionary init_table;
      StringCollection warning_table;

      typedef Element Type;
      void getTypeSpec (string type, TypeKind & kind, Type * & spec);
      string initValue (string type, bool construct);

   public:
      TypeKind getTypeKind  (string type); // type classification
      string    getInitValue (string type); // C++ initialization value
      string    getConstructorInitValue (string type); // C++ initialization used in constructor
      string    getProgramValue (string type, string value); // convert value to C++ constant

      void      storeTypeKind (string type, TypeKind kind);
      void      storeInitValue (string type, string value);
   #endif

   protected:
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);

   public:
      void readSource (EnvInput & io);
      virtual void readElement (EnvInput & io) { readSource (io); }

   public:
      Project ();
      virtual ~ Project () { }
      virtual Project * toProject () { return this; }
      virtual string getTypeName () { return "Project"; }
};

/* ---------------------------------------------------------------------- */

class Module : public Element
{
   public:
      bool   automatic_namespace; // use OPEN_NAMESPACE / CLOSE_NAMESPACE
      string module_namespace;

      bool export_symbols; // duplicate symbol definitions in header file
      bool export_symbols_to_impl; // duplicate symbol definitions in implementation file

      bool only_header; // create only header file (used in comp1/compile.cc)
      bool generate_cpp; // create minimal .cpp file // !? not implemented

      string source_file_name; // source file name is used only for comment
      string header_name; // output file name for generated header
      string code_name; // output file name for generated implementation

      string desc_header_name; // output description header file name
      string desc_code_name;   // output description code file name

      bool no_class_modif; // do not add constructors and forward class declarattions, only for splitted headers (used in gram/process.cc)
      bool only_variables; // set only_variable for all attributes

      bool keep_methods; // keep method implementations in class declarations
      bool experimental; // experimental components

      bool generate_components; // set generate_component for all classes
      bool factory_list; // create function <module name>_factories

   protected:
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);

   private:
      Func * init_func;
      StatList * fact_point;

   public:
      Func * getModuleInit ();
      void addFactoryToInitList (string id, string cond_symbol);

   public:
      EnumType * searchEnum (string id);
      EnumType * findEnum (string id);
      EnumType * getFirstEnum ();

      Class * searchClass (string id);
      Class * findClass (string id);
      Class * seekClass (string id);
      Class * lookupClass (string name);
      Class * getFirstClass ();

      Module * getNextModule ();

   public:
      SimpleDecl * addSimpleDecl (DeclKind kind, string code, int group);
      void defineSymbol (DeclKind kind, string id, int group);

   public:
      void includeFile (DeclKind kind, string file_name);
      void includeSysFile (DeclKind kind, string file_name);
      void Using (DeclKind kind, string object_name);
      void usingNamespace (DeclKind kind, string namespace_name);

   public:
      ProgItem * declSect (DeclKind kind) { return this; }

   private:
      void addInfo (DeclKind kind);
      void addCondition ();
      void addNamespaces (DeclKind kind);
      void addClasses ();
      void addConstructors ();
      void addComments ();

   public:
      virtual void readSubitem (EnvInput & io);
      void readSource (EnvInput & io);
      virtual void readElement (EnvInput & io) { readSource (io); }

   public:
      virtual void Prog ();
      virtual void completeProg ();
      virtual void writeIntf (TextOutput & io);
      virtual void writeImpl (TextOutput & io);

   public:
      void writeInterface ();
      void writeImplementation ();

   public:
      Module ();
      virtual ~ Module () { }
      virtual Module * toModule () { return this; }
      virtual string getTypeName () { return "Module"; }
};

/* ---------------------------------------------------------------------- */

class EnumType : public Element
{
   public:
      bool no_impl;

   public:
      EnumItem * getFirstItem ();

   public:
      virtual bool isBitSetType () { return false; }

   protected:
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);

   public:
      void readItems (EnvInput & io);
      void readSource (EnvInput & io);
      virtual void readElement (EnvInput & io) { readSource (io); }

      virtual void Prog ();
      virtual void writeIntf (TextOutput & io);

   public:
      EnumType ();
      virtual ~ EnumType () { }
      virtual EnumType * toEnumType () { return this; }
      virtual string getTypeName () { return "EnumType"; }
};

class EnumItem : public Element
{
   public:
      string Init;

   public:
      EnumItem * getNextItem ();

   protected:
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);

   public:
      virtual void writeIntf (TextOutput & io);

   public:
      EnumItem ();
      virtual ~ EnumItem () { }
      virtual EnumItem * toEnumItem () { return this; }
      virtual string getTypeName () { return "EnumItem"; }
};

/* ---------------------------------------------------------------------- */

class Class : public Element
{
   public:
      string Parent;
      string Interface; // !?

   public:
      bool no_automatic_constructor; // do not generate default constructor
      bool auxiliary_constructor; // add protected constructor for base class initialization

      bool virtual_base;
      bool virtual_destructor;

   public:
      bool no_component; // prevent component generation for component desctriptions
      bool no_forward; // forward class declaration is not necessary
      bool foreign_decl; // class declared in another module
      bool interface_class; // stencil generated browse interface class

   public:
      string Tooltip;
      string ImageName;
      string SelectedImageName;
      string PageName;
      bool   OnlyImage;

      string Alias;
      string AliasGroup;

      string TypeAttr;
      string NameAttr;
      string FromAttr;

   protected:
      StrField * Parent_Field;
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);

   private:
      Func * init_func;
      Func * cleanup_func;

   public:
      Func * component_link; // linkElem method
      Func * component_remove; // removeElem method

   public:
      bool no_description; // prevent description generation for desctription classes

   private:
      Access read_access; // access modifier for ReadSubitem

   private:
      Class * parent_cls; // cached value

   public:
      Class * parentCls ();

   public:
      Attr * searchAttr (string id);
      Attr * findAttr (string id);
      Attr * seekAttr (string id);
      Attr * lookupAttr (string id);
      Attr * getFirstAttr ();

      List * searchList (string id);
      List * findList (string id);
      List * seekList (string id);
      List * lookupList (string id);
      List * getFirstList ();

      Func * searchFunc (string id);
      Func * getFirstSignal ();
      Func * getFirstSlot ();

      Class * getNextClass ();

   public:
      Func * getInit (); // create default constructor
      Func * getCleanup (); // create destructor

   public:
      void addVirtualBaseClass (string interface) { }

   public:
      void addComments ();

      virtual void writePrefix (TextOutput & io);
      virtual void writeInitMembers (TextOutput & io);

      void readMember (EnvInput & io, Access & acs, int grp = 0);
      virtual void readSubitem (EnvInput & io);
      void readSource (EnvInput & io);
      virtual void readElement (EnvInput & io) { readSource (io); }

      virtual void Prog ();
      virtual void completeProg ();
      virtual void writeIntf (TextOutput & io);
      virtual void writeImpl (TextOutput & io);

   public:
      Class ();
      virtual ~ Class () { }
      virtual Class * toClass () { return this; }
      virtual string getTypeName () { return "Class"; }
};

/* ---------------------------------------------------------------------- */

class Attr : public Element
{
   public:
      string Type;
      string Init;

   #if 1
      string FieldType;

      bool only_variable; // do not declare get and set methods
      bool no_comment; // no comment before variable declaration

      bool no_variable; // do not declare data member
      bool no_compare;  // do not compare new and old value

      bool protected_variable; // protected, not private data member
      bool public_variable; // public, not private data member

      bool no_get_method; // do not generate get method
      bool no_set_method; // do not generate set method

      bool no_get_impl; // do not implement get method
      bool no_set_impl; // do not implement set method

      bool empty_get_impl; // empty get method implementation
      bool empty_set_impl; // empty set method implementation

      bool virtual_get; // virtual get method
      bool virtual_set; // virtual set method

      bool abstract_get; // abstract get method
      bool abstract_set; // abstract set method

      bool no_inline_get; // do not inline get method
      bool inline_set; // inline set method

      bool field_variable; // field variable is necessary

      bool print_decl; // !?

   protected:
      StrField * Type_Field;
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);


   // identifiers
   public:
      string field_id;
      string get_id;
      string set_id;

   // functions
   public:
      // TFunc * field_func; // variable (NULL if no_variable == true)
      Func * get_func;
      Func * set_func;

      StatList * get_point; // location in get method
      StatList * set_point; // location in set or update method

   public:
      // flag used before comparing old and new value in 'set' method
      string compare_flag_name;  // flag variable identifier
      string compare_flag_value; // bit number identifier

   private:
      Class * source_cls; // cached value

   public:
      Class * sourceCls ();

   public:
      string attrType () { return Type; }
      TypeKind attrKind ();
      string varietyKey () { return variety_symbol; } ; // variety_symbol (if different from class variety symbol)

   #endif

   public:
      string getFunc () { return "get" + Name; }
      string setFunc () { return "set" + Name; }

   public:
      Attr * getNextAttr ();

   private:
      Element * following_item;
   public:
      void add_following (Element * item);

   public:
      void readSource (EnvInput & io);
      virtual void readElement (EnvInput & io) { readSource (io); }

      virtual void Prog ();
      virtual void writeIntf (TextOutput & io);

   public:
      Attr ();
      virtual ~ Attr () { }
      virtual Attr * toAttr () { return this; }
      virtual string getTypeName () { return "Attr"; }
};

/* ---------------------------------------------------------------------- */

class List : public Element
{
   public:
      string element;

   public:
      string Prefix;
      string Suffix1;
      string Suffix2;

   public:
      string Get;

   public:
      string First;
      string Last;
      string Prev;
      string Next;
      string Up;

   public:
      string InsertFirst;
      string InsertLast;
      string InsertBefore;
      string InsertAfter;

      string Link;

      string Remove;
      string RemoveAll;

   #if 1
   public:
      bool virtual_link;     // true => virtual link method
      bool virtual_remove;   // true => virtual remove method

      bool public_variables; // public, not private data member
      bool no_release;       // true => RemoveAll does not delete subitems (only unlink subitems)

      string attach_notification; // notify container
      string detach_notification;

      string insert_notification; // notify item
      string remove_notification;

      bool restricted; // true => no first, last field
      bool combined;   // true => no InsertBefore, InsertAfter, Remove method
      bool component_list; // combined with list of TElement(s)

      bool print_decl;
   #endif

   protected:
      StrField * Element_Field;
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);


   // identifiers
   public:
      string first_id;
      string last_id;
      string prev_id;
      string next_id;
      string up_id;

      string get_first_id;
      string get_last_id;
      string get_prev_id;
      string get_next_id;
      string get_up_id;

      string ins_first_id;
      string ins_last_id;
      string ins_before_id;
      string ins_after_id;
      string link_id;
      string remove_id;
      string remove_all_id;

   // variables and functions
   public:
      Var * first_var;
      Var * last_var;
      Var * prev_var;
      Var * next_var;
      Var * up_var;

      Func * get_first_func;
      Func * get_last_func;
      Func * get_prev_func;
      Func * get_next_func;
      Func * get_up_func;

      Func * ins_first_func;
      Func * ins_last_func;
      Func * ins_before_func;
      Func * ins_after_func;
      Func * link_func;
      Func * remove_func;
      Func * remove_all_func;

   public:
      StatList * check_point;

      StatList * ins_point;
      StatList * rem_point;
      StatList * final_rem_point;

   // private:
   public:
      Class * source_cls; // cached value
      Class * element_cls; // cached value

   public:
      Class * sourceCls ();
      Class * elementCls ();

   private:
      Element * following_item;

   public:
      void add_following (Element * item);
      void add_element (Element * item);

   public:
      List * getNextList ();

   public:
     string getTypeAsString () { return element; } // !?

   public:
      virtual void setNames ();

   private:
      void addField (Class * cls, string result_type, string field_name, Var * & prod);
      void getFunc (Class * cls, string result_type, string func_name, string field_name, Func * & prod);
      void linkMethod ();
      void insertMethods ();
      void removeMethods ();

      void componentLinkMethod ();
      void componentRemoveMethod ();
      void componentInsertMethods ();

   public:
      void readSource (EnvInput & io);
      virtual void readElement (EnvInput & io) { readSource (io); }

      virtual void Prog ();
      virtual void writeIntf (TextOutput & io);

   public:
      List ();
      virtual ~ List () { }
      virtual List * toList () { return this; }
      virtual string getTypeName () { return "List"; }
};

/* ---------------------------------------------------------------------- */

class Expr : public Element
{
   public:
      string Code;

   public:
      virtual void writeCode (TextOutput & io);

   public:
      Expr () { }
      virtual ~ Expr () { }
      virtual string getTypeName () { return "Expr"; }
};

class ExprList : public Element
{
   public:
      ExprList () { }
      virtual ~ ExprList () { }
      virtual string getTypeName () { return "ExprList"; }
};

/* ---------------------------------------------------------------------- */

class Stat : public Element
{
   public:
      Stat () { }
      virtual ~ Stat () { }
      virtual string getTypeName () { return "Stat"; }
};

class StatList : public Element
{
   public:
      StatList () { }
      virtual ~ StatList () { }
      virtual string getTypeName () { return "StatList"; }
};

/* ---------------------------------------------------------------------- */

#if 0
class SimpleStat : public Stat
{
   public:
      string Code;

   public:
      virtual void writeCode (TextOutput & io);

   public:
      SimpleStat () { }
      virtual ~ SimpleStat () { }
      virtual string getTypeName () { return "SimpleStat"; }
};

class IfStat : public Stat
{
   public:
      string Condition;
      StatList Then;
      StatList Else;

   public:
      virtual void writeCode (TextOutput & io);

   public:
      virtual bool isIfStat () { return true; }

   public:
      IfStat () { }
      virtual ~ IfStat () { }
      virtual string getTypeName () { return "IfStat"; }
};

class WhileStat : public Stat
{
   public:
      string Condition;
      StatList Body;

   public:
      virtual void writeCode (TextOutput & io);

   public:
      WhileStat () { }
      virtual ~ WhileStat () { }
      virtual string getTypeName () { return "WhileStat"; }
};

class CompoundStat : public Stat
{
   public:
      StatList Body;

   public:
      virtual void writeCode (TextOutput & io);

   public:
      CompoundStat () { }
      virtual ~ CompoundStat () { }
      virtual string getTypeName () { return "CompoundStat"; }
};
#endif

class ClusterStat : public Stat
{
   public:
      Element * InnerItem;

   public:
      virtual void writeCode (TextOutput & io);

   public:
      ClusterStat (StatList * list) { assert (list != NULL); InnerItem = list; }
      virtual ~ ClusterStat () { }
      virtual string getTypeName () { return "ClusterStat"; }
};

/* ---------------------------------------------------------------------- */

class TextItem : public Element
{
   public:
      string Code;

   public:
      virtual void writeCode (TextOutput & io);
      virtual void writeElement (EnvOutput & io) { io.put (Code); }

   public:
      TextItem () { }
      virtual ~ TextItem () { }
      virtual string getTypeName () { return "TextItem"; }
};

class LineItem : public Element
{
   public:
      string Code;

   public:
      virtual void writeCode (TextOutput & io);
      virtual void writeElement (EnvOutput & io) { io.putLn (Code); }

   public:
      LineItem () { }
      virtual ~ LineItem () { }
      virtual string getTypeName () { return "LineItem"; }
};

class IndentItem : public Element
{
   public:
      virtual void writeCode (TextOutput & io);
      virtual bool isEmpty () { return true; }

   public:
      IndentItem () { }
      virtual ~ IndentItem () { }
      virtual string getTypeName () { return "IndentItem"; }
};

class UnindentItem : public Element
{
   public:
      virtual void writeCode (TextOutput & io);
      virtual bool isEmpty () { return true; }

   public:
      UnindentItem () { }
      virtual ~ UnindentItem () { }
      virtual string getTypeName () { return "UnindentItem"; }
};

class EmptyLineItem : public Element
{
   public:
      virtual void writeCode (TextOutput & io);
      virtual bool isEmpty () { return true; }

   public:
      EmptyLineItem () { }
      virtual ~ EmptyLineItem () { }
      virtual string getTypeName () { return "EmptyLineItem"; }
};

class NoEmptyLineItem : public Element
{
   public:
      virtual void writeCode (TextOutput & io);
      virtual bool isEmpty () { return true; }

   public:
      NoEmptyLineItem () { }
      virtual ~ NoEmptyLineItem () { }
      virtual string getTypeName () { return "NoEmptyLineItem"; }
};

class LocationItem : public Element
{
   public:
      Element * InnerItem;

   public:
      virtual bool isIfStat () { return InnerItem != NULL && InnerItem->isIfStat (); }
      virtual void writeCode (TextOutput & io);

   public:
      LocationItem () : InnerItem (NULL) { }
      virtual ~ LocationItem () { }
      virtual string getTypeName () { return "LocationItem"; }
};

/* ---------------------------------------------------------------------- */

#if 1
class InitItem : public Element
{
   public:
      string Value;

   public:
      virtual void writeCode (TextOutput & io) { }

   public:
      InitItem () { }
      virtual ~ InitItem () { }
      virtual string getTypeName () { return "InitItem"; }
};

class InitList : public Element
{
   public:
      InitList () { }
      virtual ~ InitList () { }
      virtual string getTypeName () { return "InitList"; }
};
#endif

/* ---------------------------------------------------------------------- */

class ParamDecl : public Element
{
   public:
      string Type;
      string Init;
      string user_name; // (rename to) user identifier (used by viewprog)

   protected:
      StrField * Type_Field;
      StrField * Init_Field;
      StrField * user_name_Field;

   public:
      ParamDecl * getNextParam ();

   public:
      string paramType () { return Type; }

   protected:
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);

   public:
      virtual void writeIntf (TextOutput & io);
      virtual void writeImpl (TextOutput & io);

   public:
      ParamDecl ();
      virtual ~ ParamDecl () { }
      virtual ParamDecl * toParamDecl () { return this; }
      virtual string getTypeName () { return "ParamDecl"; }
};

class ParamList : public Element
{
   public:
      ParamList () { }
      virtual ~ ParamList () { }
      virtual string getTypeName () { return "ParamDecl"; }
};

/* ---------------------------------------------------------------------- */

class SimpleDecl : public Element
{
   public:
      DeclKind Kind;

   public:
      virtual void writeIntf (TextOutput & io);
      virtual void writeImpl (TextOutput & io);

   public:
      SimpleDecl () : Kind (intf) { }
      virtual ~ SimpleDecl () { }
      virtual SimpleDecl * toSimpleDecl () { return this; }
      virtual string getTypeName () { return "SimpleDecl"; }
};

/* ---------------------------------------------------------------------- */

class Var : public Element
{
   public:
      string Type;
      string Precede;
      string Follow;
      string Init;

      bool Static;
      bool Extern;

   public:
      bool SimpleInit; // add initialiation into class declarations

   protected:
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);

   public:
      virtual void writeIntf (TextOutput & io);
      virtual void writeImpl (TextOutput & io);

      virtual bool hasInit ();
      virtual void writeInit (TextOutput & io);

   public:
      Var ();
      virtual ~ Var () { }
      virtual Var * toVar () { return this; }
      virtual string getTypeName () { return "Var"; }
};

/* ---------------------------------------------------------------------- */

class Func : public Element
{
   public:
      string Type;
      string Precede;
      ParamList * Params;
      string Follow;

      bool Constructor;
      bool Destructor;

      bool Signal;
      bool Slot;

      bool Inline;
      bool Static;
      bool Extern;
      bool Virtual;
      bool Explicit;
      // bool Overide;
      // bool Overload;
      bool Abstract;

      bool no_decl;
      bool no_impl;

      bool skip_decl;
      bool skip_impl;

      bool basic_method; // generate when only_basic_methods is true

   protected:
      StrField * Type_Field; // necessary for gtk module

   public:
      ProgItem * modif_point; // location in get method, ...

   private:
      Class * source_cls; // cached value

   public:
      Class * sourceCls ();

   public:
      void addParam (string type, string name, string init = "");
      void addParams (ParamList * list);
      ParamDecl * getFirstParam ();

   public:
      Func * getNextSignal ();
      Func * getNextSlot ();

   public:
      bool isProcedure () { return Type == "" || Type == "void"; }
      string resultType () { return Type == "" ? "void" : Type; }
      ExprList * getParamExpr ();

      string varietyKey () { return variety_symbol; } // variety_symbol (if different from class variety symbol)

   protected:
      virtual void createLocalFields ();
      virtual bool readField (EnvInput & io);

   private:
      void writeBody (TextOutput & io);

   public:
      virtual void writeIntf (TextOutput & io);
      virtual void writeImpl (TextOutput & io);

   public:
      Func ();
      virtual ~ Func ();
      virtual Func * toFunc () { return this; }
      virtual string getTypeName () { return "Func"; }
};

/* ---------------------------------------------------------------------- */

class ExportSymbol : public Element
{
   public:
      void updateProg ();
   public:
      ExportSymbol ();
      virtual ~ ExportSymbol () { }
      virtual string getTypeName () { return "ExportSymbol"; }
};

class TypeInfo : public Element
{
   public:
      TypeKind kind;

   protected:
      TypeKindField * kind_Field;

   public:
      void basicProg ();

   public:
      TypeInfo ();
      virtual ~ TypeInfo () { }
      virtual string getTypeName () { return "TypeInfo"; }
};

/* ---------------------------------------------------------------------- */

class Styler : public Element
{
   public:
      string element;

   protected:
      StrField * Element_Field;

   private:
      Class * source_cls; // cached value
      Class * element_cls; // cached value

   public:
      Class * sourceCls ();
      Class * elementCls ();

   private:
      void attrMethod ();

   public:
      virtual void Prog ();

   public:
      Styler ();
      virtual ~ Styler () { }
      virtual string getTypeName () { return "Styler"; }
};

/* ---------------------------------------------------------------------- */

/* fields */

class ClassPtrField : public IndirectField_Class
{
   public:
      virtual void Prepare ();
};

class AttrPtrField : public IndirectField_Attr
{
   public:
      virtual void Prepare ();
};

class ListPtrField : public IndirectField_List
{
   public:
      virtual void Prepare ();
};

/* ---------------------------------------------------------------------- */

class DeclKindField : public Field
{
   private:
      DeclKind * pointer;
   public:
      virtual DeclKind getValue ();
      virtual void setValue (DeclKind val);
      virtual string getText ();
      virtual void setText (string val);
      virtual DeclKind defaultValue ();
      virtual bool isDefault () { return getValue () == defaultValue (); }
      void setPointer (DeclKind * p_pointer);
      void setVariable (DeclKind & p_variable);
      DeclKindField ();
};

class TypeKindField : public Field
{
   private:
      TypeKind * pointer;
   public:
      virtual TypeKind getValue ();
      virtual void setValue (TypeKind val);
      virtual string getText ();
      virtual void setText (string val);
      virtual TypeKind defaultValue ();
      virtual bool isDefault () { return getValue () == defaultValue (); }
      void setPointer (TypeKind * p_pointer);
      void setVariable (TypeKind & p_variable);
      TypeKindField ();
};

/* ---------------------------------------------------------------------- */

const string ptr_mark = " *"; // !?
const string ref_mark = " &"; // !?

/* ---------------------------------------------------------------------- */

void split_name (string name, string & a, string & b);

Class * createFactory (Class * cls,
                        Class * element_cls,
                        string factory_name,
                        string factory_base_class,
                        Module * target_module);

/* ---------------------------------------------------------------------- */

Project * currentProjectOrNull (Element * elem);
Project * currentProject (Element * elem);

Module * currentModuleOrNull (Element * elem);
Module * currentModule (Element * elem);

Class * currentClassOrNull (Element * elem);
Class * currentClass (Element * elem);

/* ---------------------------------------------------------------------- */

Expr * joinExpr (Expr * a, string txt, Expr * b);
string exprToStr (Expr * expr);
string exprListToStr (ExprList * expr);
string paramListToStr (ParamList * list);

void insertMethod (Class * cls, Access acs, Func * func, int group = 0);

/* ---------------------------------------------------------------------- */

/* input / output */

Element * allocateElement (EnvInput & io, Element * top, string name);

void saveElements (string file_name, Element * top);

void loadProject (Project * project, string file_name);

/* ---------------------------------------------------------------------- */

void code_init ();

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_CODE_H__ */
