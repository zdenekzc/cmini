
/* groups.h */

#ifndef __PROG_GROUPS_H__
#define __PROG_GROUPS_H__

#include "std.h"

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

/* groups */

const int title_group = 1;
const int open_condition_group = 2;
const int include_group = 3;
const int using_group = 4;
const int symbol_group = 5;
const int open_namespace_group = 6;

const int class_forward_group = 7;
const int enum_group = 8;
const int type_alias_group = 9;
const int extension_group = 10;

const int attr_group = 12;
const int field_group = 14;
const int list_group = 15;
const int component_group = 16;
const int component_insert_group = 17;
const int super_group = 18;
const int browse_group = 19; // browse stencil methods (properties, subitems)

// !?
const int view_group = 20;
const int view_type_group = 21;
const int view_changed_flags_group = 22;
const int view_updated_flags_group = 23;
const int view_connected_flags_group = 24;
const int view_modified_group = 25;
const int view_ready_group = 26;
const int signal_group = 27;
const int slot_group = 28;
const int alias_group = 29;

const int gram_public_group = 30;
const int gram_private_group = 31;
   // gram_public_group <= gram_private_group
   // important when public_header == ""

const int gram_set_group = 32;
const int gram_location_group = 33;
const int gram_conv_group = 34;
const int gram_code_group = 35;
const int gram_tree_group = 36;
const int gram_tree2_group = 37;
const int gram_send_group = 38;
const int gram_parse_group = 39;

const int gram_syntactic_predicate_group = 40;
const int gram_semantic_predicate_group = 41;
const int gram_execute_group = 42;
const int gram_keyword_group = 43;
const int gram_primitive_group = 44;
const int gram_lexer_group = 45;
const int gram_binary_group = 46;

const int gram_extension_group = 47;

const int follow_group = 48;

const int callback_group = 49;

const int norm_group = 50; // other generated declarations
const int user_group = 51; // user declarations

const int rpc_decl_group = 52;
const int export_conv_group = 53;
const int export_decl_group = 54;
const int export_group = 55; // export_group should be greater than user_group

const int dtor_group = 56;
const int ctor_group = 57;
const int friend_group = 58;
const int factory_group = 59;

const int check_group = 60;

const int init_module_group = 61;

const int close_namespace_group = 62;
const int close_condition_group = 63;

const int min_group = 0;
const int max_group = 64;

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_CODE_H__ */
