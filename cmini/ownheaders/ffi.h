/* ffi.h - cparser / C-- (libffi; v C <ffi.h>); hodnoty a velikost ffi_cif jako libffi na x86-64 -
   plati i v nasm, pasm, asm64 (samopreklad cmm) */
#ifndef CMM_FFI_H
#define CMM_FFI_H
/* rozlozeni jako libffi na x86-64 (C-- ma int 4 bajty, short 2 - jako v C) */
#define FFI_TYPE_STRUCT 13
/* jako v libffi typedef struct _x {...} x - preklad do C (translate.c) pouzije jmeno ffi_type */
typedef struct _ffi_type { unsigned long size; unsigned short alignment; unsigned short type; struct _ffi_type * * elements; } ffi_type;
typedef struct { int abi; unsigned int nargs; void * arg_types; void * rtype; unsigned int bytes; unsigned int flags; } ffi_cif;   /* 32 bajtu */
typedef unsigned long ffi_arg;
enum ffi_status { FFI_OK, FFI_BAD_TYPEDEF, FFI_BAD_ABI };
enum ffi_abi { FFI_DEFAULT_ABI = 2 };   /* FFI_UNIX64 (x86-64) */
extern ffi_type ffi_type_void;
extern ffi_type ffi_type_uint8;
extern ffi_type ffi_type_sint8;
extern ffi_type ffi_type_uint16;
extern ffi_type ffi_type_sint16;
extern ffi_type ffi_type_float;
extern ffi_type ffi_type_uint32;
extern ffi_type ffi_type_sint32;
extern ffi_type ffi_type_uint64;
extern ffi_type ffi_type_sint64;
extern ffi_type ffi_type_pointer;
extern ffi_type ffi_type_double;
typedef void (* ffi_fn_t) ();
ffi_fn_t FFI_FN (void * f);   /* v C makro; nasm, pasm, asm64: funkce v cminirt-host.cc */
ffi_status ffi_prep_cif (ffi_cif * cif, ffi_abi abi, unsigned int nargs, ffi_type * rtype, ffi_type * * atypes);
ffi_status ffi_prep_cif_var (ffi_cif * cif, ffi_abi abi, unsigned int nfixed, unsigned int ntotal, ffi_type * rtype, ffi_type * * atypes);
void ffi_call (ffi_cif * cif, ffi_fn_t fn, void * rvalue, void * * avalue);
/* uzavery: nativni funkce, ktera vola interpret (basic.cc - zpetne volani z qsort) */
typedef struct { char tramp [32]; void * cif; void * fun; void * user_data; } ffi_closure;   /* 56 bajtu jako libffi (x86-64) */
typedef void (* ffi_closure_fun) (ffi_cif * cif, void * ret, void * * args, void * user_data);
void * ffi_closure_alloc (unsigned long size, void * * code);
ffi_status ffi_prep_closure_loc (ffi_closure * closure, ffi_cif * cif, ffi_closure_fun fun, void * user_data, void * codeloc);
#endif
