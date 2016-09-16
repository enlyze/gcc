/* Routines for GCC for ARM/pe.
   Copyright (C) 1995-2016 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "basic-block.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "tm_p.h"
#include "stringpool.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "lto-streamer.h"
#include "output.h"
#include "varasm.h"
#include "lto-section-names.h"


/* Return the type that we should use to determine if DECL is
   imported or exported.  */

static tree
associated_type (tree decl)
{
  return (DECL_CONTEXT (decl) && TYPE_P (DECL_CONTEXT (decl))
          ?  DECL_CONTEXT (decl) : NULL_TREE);
}

/* Return true if DECL should be a dllexport'd object.  */

static bool
arm_pe_determine_dllexport_p (tree decl)
{
  if (TREE_CODE (decl) != VAR_DECL && TREE_CODE (decl) != FUNCTION_DECL)
    return false;

  /* Don't export local clones of dllexports.  */
  if (!TREE_PUBLIC (decl))
    return false;

  if (TREE_CODE (decl) == FUNCTION_DECL
      && DECL_DECLARED_INLINE_P (decl)
      && !flag_keep_inline_dllexport)
    return false; 

  if (lookup_attribute ("dllexport", DECL_ATTRIBUTES (decl)))
    return true;

  return false;
}

/* Return true if DECL should be a dllimport'd object.  */

static bool
arm_pe_determine_dllimport_p (tree decl)
{
  tree assoc;

  if (TREE_CODE (decl) != VAR_DECL && TREE_CODE (decl) != FUNCTION_DECL)
    return false;

  if (DECL_DLLIMPORT_P (decl))
    return true;

  /* The DECL_DLLIMPORT_P flag was set for decls in the class definition
     by  targetm.cxx.adjust_class_at_definition.  Check again to emit
     error message if the class attribute has been overridden by an
     out-of-class definition of static data.  */
  assoc = associated_type (decl);
  if (assoc && lookup_attribute ("dllimport", TYPE_ATTRIBUTES (assoc))
      && TREE_CODE (decl) == VAR_DECL
      && TREE_STATIC (decl) && TREE_PUBLIC (decl)
      && !DECL_EXTERNAL (decl)
      /* vtable's are linkonce constants, so defining a vtable is not
	 an error as long as we don't try to import it too.  */
      && !DECL_VIRTUAL_P (decl))
	error ("definition of static data member %q+D of "
	       "dllimport%'d class", decl);

  return false;
}

/* Emit an assembler directive to set symbol for DECL visibility to
   the visibility type VIS, which must not be VISIBILITY_DEFAULT.
   As for PE there is no hidden support in gas, we just warn for
   user-specified visibility attributes.  */

void
arm_pe_assemble_visibility (tree decl, int)
{
  if (!decl
      || !lookup_attribute ("visibility", DECL_ATTRIBUTES (decl)))
    return;
  if (!DECL_ARTIFICIAL (decl))
    warning (OPT_Wattributes, "visibility attribute not supported "
			      "in this configuration; ignored");
}

/* This is used as a target hook to modify the DECL_ASSEMBLER_NAME
   in the language-independent default hook
   langhooks,c:lhd_set_decl_assembler_name ()
   and in cp/mangle,c:mangle_decl ().  */
tree
arm_pe_mangle_decl_assembler_name (tree decl, tree id)
{
  return id;
}

/* This hook behaves the same as varasm.c/assemble_name(), but
   generates the name into memory rather than outputting it to
   a file stream.  */

tree
arm_pe_mangle_assembler_name (const char *name)
{
  const char *skipped = name + (*name == '*' ? 1 : 0);
  const char *stripped = targetm.strip_name_encoding (skipped);
  if (*name != '*' && *user_label_prefix)
    stripped = ACONCAT ((user_label_prefix, stripped, NULL));
  return get_identifier (stripped);
}

void
arm_pe_encode_section_info (tree decl, rtx rtl, int first)
{
  rtx symbol;
  int flags;

  /* Do this last, due to our frobbing of DECL_DLLIMPORT_P above.  */
  default_encode_section_info (decl, rtl, first);

  /* Careful not to prod global register variables.  */
  if (!MEM_P (rtl))
    return;

  symbol = XEXP (rtl, 0);
  gcc_assert (GET_CODE (symbol) == SYMBOL_REF);

  switch (TREE_CODE (decl))
    {
    case FUNCTION_DECL:
    case VAR_DECL:
      break;

    default:
      return;
    }

  /* Mark the decl so we can tell from the rtl whether the object is
     dllexport'd or dllimport'd.  tree.c: merge_dllimport_decl_attributes
     handles dllexport/dllimport override semantics.  */
  flags = (SYMBOL_REF_FLAGS (symbol) &
	   ~(SYMBOL_FLAG_DLLIMPORT | SYMBOL_FLAG_DLLEXPORT));
  if (arm_pe_determine_dllexport_p (decl))
    flags |= SYMBOL_FLAG_DLLEXPORT;
  else if (arm_pe_determine_dllimport_p (decl))
    flags |= SYMBOL_FLAG_DLLIMPORT;
 
  SYMBOL_REF_FLAGS (symbol) = flags;
}


bool
arm_pe_binds_local_p (const_tree exp)
{
  if ((TREE_CODE (exp) == VAR_DECL || TREE_CODE (exp) == FUNCTION_DECL)
      && DECL_DLLIMPORT_P (exp))
    return false;

  /* External public symbols, which aren't weakref-s,
     have local-binding for PE targets.  */
  if (DECL_P (exp)
      && !lookup_attribute ("weakref", DECL_ATTRIBUTES (exp))
      && TREE_PUBLIC (exp)
      && DECL_EXTERNAL (exp))
    return true;

#ifndef MAKE_DECL_ONE_ONLY
  /* PR target/66655: If a function has been marked as DECL_ONE_ONLY
     but we do not the means to make it so, then do not allow it to
     bind locally.  */
  if (DECL_P (exp)
      && TREE_CODE (exp) == FUNCTION_DECL
      && TREE_PUBLIC (exp)
      && DECL_ONE_ONLY (exp)
      && ! DECL_EXTERNAL (exp)
      && DECL_DECLARED_INLINE_P (exp))
    return false;
#endif
  
  return default_binds_local_p_1 (exp, 0);
}

/* Also strip the fastcall prefix and stdcall suffix.  */

const char *
arm_pe_strip_name_encoding_full (const char *str)
{
  const char *p;
  const char *name = default_strip_name_encoding (str);

  /* Strip leading '@' on fastcall symbols.  */
  if (*name == '@')
    name++;

  /* Strip trailing "@n".  */
  p = strchr (name, '@');
  if (p)
    return ggc_alloc_string (name, p - name);

  return name;
}

void
arm_pe_unique_section (tree decl, int reloc)
{
  int len;
  const char *name, *prefix;
  char *string;

  /* Ignore RELOC, if we are allowed to put relocated
     const data into read-only section.  */
  if (!flag_writable_rel_rdata)
    reloc = 0;
  name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  name = arm_pe_strip_name_encoding_full (name);

  /* The object is put in, for example, section .text$foo.
     The linker will then ultimately place them in .text
     (everything from the $ on is stripped). Don't put
     read-only data in .rdata section to avoid a PE linker
     bug when .rdata$* grouped sections are used in code
     without a .rdata section.  */
  if (TREE_CODE (decl) == FUNCTION_DECL)
    prefix = ".text$";
  else if (decl_readonly_section (decl, reloc))
    prefix = ".rdata$";
  else
    prefix = ".data$";
  len = strlen (name) + strlen (prefix);
  string = XALLOCAVEC (char, len + 1);
  sprintf (string, "%s%s", prefix, name);

  set_decl_section_name (decl, string);
}

/* Local and global relocs can be placed always into readonly memory for
   memory for PE-COFF targets.  */
int
arm_pe_reloc_rw_mask (void)
{
  return 0;
}

/* Select a set of attributes for section NAME based on the properties
   of DECL and whether or not RELOC indicates that DECL's initializer
   might contain runtime relocations.

   We make the section read-only and executable for a function decl,
   read-only for a const data decl, and writable for a non-const data decl.

   If the section has already been defined, to not allow it to have
   different attributes, as (1) this is ambiguous since we're not seeing
   all the declarations up front and (2) some assemblers (e.g. SVR4)
   do not recognize section redefinitions.  */
/* ??? This differs from the "standard" PE implementation in that we
   handle the SHARED variable attribute.  Should this be done for all
   PE targets?  */

#define SECTION_PE_SHARED	SECTION_MACH_DEP

unsigned int
arm_pe_section_type_flags (tree decl, const char *, int reloc)
{
  unsigned int flags;

  /* Ignore RELOC, if we are allowed to put relocated
     const data into read-only section.  */
  if (!flag_writable_rel_rdata)
    reloc = 0;

  if (decl && TREE_CODE (decl) == FUNCTION_DECL)
    flags = SECTION_CODE;
  else if (decl && decl_readonly_section (decl, reloc))
    flags = 0;
  else
    {
      flags = SECTION_WRITE;

      if (decl && TREE_CODE (decl) == VAR_DECL
	  && lookup_attribute ("shared", DECL_ATTRIBUTES (decl)))
	flags |= SECTION_PE_SHARED;
    }

  if (decl && DECL_P (decl) && DECL_ONE_ONLY (decl))
    flags |= SECTION_LINKONCE;

  return flags;
}

void
arm_pe_asm_named_section (const char *name, unsigned int flags, 
			   tree decl)
{
  char flagchars[8], *f = flagchars;

#if defined (HAVE_GAS_SECTION_EXCLUDE) && HAVE_GAS_SECTION_EXCLUDE == 1
  if ((flags & SECTION_EXCLUDE) != 0)
    *f++ = 'e';
#endif

  if ((flags & (SECTION_CODE | SECTION_WRITE)) == 0)
    /* readonly data */
    {
      *f++ ='d';  /* This is necessary for older versions of gas.  */
      *f++ ='r';
    }
  else	
    {
      if (flags & SECTION_CODE)
        *f++ = 'x';
      if (flags & SECTION_WRITE)
        *f++ = 'w';
      if (flags & SECTION_PE_SHARED)
        *f++ = 's';
#if !defined (HAVE_GAS_SECTION_EXCLUDE) || HAVE_GAS_SECTION_EXCLUDE == 0
      /* If attribute "e" isn't supported we mark this section as
         never-load.  */
      if ((flags & SECTION_EXCLUDE) != 0)
	*f++ = 'n';
#endif
    }

  /* LTO sections need 1-byte alignment to avoid confusing the
     zlib decompression algorithm with trailing zero pad bytes.  */
  if (strncmp (name, LTO_SECTION_NAME_PREFIX,
			strlen (LTO_SECTION_NAME_PREFIX)) == 0)
    *f++ = '0';

  *f = '\0';

  fprintf (asm_out_file, "\t.section\t%s,\"%s\"\n", name, flagchars);

  if (flags & SECTION_LINKONCE)
    {
      /* Functions may have been compiled at various levels of
	 optimization so we can't use `same_size' here.
	 Instead, have the linker pick one, without warning.
	 If 'selectany' attribute has been specified,  MS compiler
	 sets 'discard' characteristic, rather than telling linker
	 to warn of size or content mismatch, so do the same.  */ 
      bool discard = (flags & SECTION_CODE)
		      || (TREE_CODE (decl) != IDENTIFIER_NODE
			  && lookup_attribute ("selectany",
					       DECL_ATTRIBUTES (decl)));
      fprintf (asm_out_file, "\t.linkonce %s\n",
	       (discard  ? "discard" : "same_size"));
    }
}

/* Beware, DECL may be NULL if compile_file() is emitting the LTO marker.  */

void
arm_pe_asm_output_aligned_decl_common (FILE *stream, tree decl,
					const char *name, HOST_WIDE_INT size,
					HOST_WIDE_INT align)
{
  HOST_WIDE_INT rounded;

  /* Compute as in assemble_noswitch_variable, since we don't have
     support for aligned common on older binutils.  We must also
     avoid emitting a common symbol of size zero, as this is the
     overloaded representation that indicates an undefined external
     symbol in the PE object file format.  */
  rounded = size ? size : 1;
  rounded += (BIGGEST_ALIGNMENT / BITS_PER_UNIT) - 1;
  rounded = (rounded / (BIGGEST_ALIGNMENT / BITS_PER_UNIT)
	     * (BIGGEST_ALIGNMENT / BITS_PER_UNIT));
  
  arm_pe_maybe_record_exported_symbol (decl, name, 1);

  fprintf (stream, "\t.comm\t");
  assemble_name (stream, name);
  fprintf (stream, ", " HOST_WIDE_INT_PRINT_DEC "\t" ASM_COMMENT_START
	   " " HOST_WIDE_INT_PRINT_DEC "\n", rounded, size);
}

/* The Microsoft linker requires that every function be marked as
   DT_FCN.  When using gas on cygwin, we must emit appropriate .type
   directives.  */

#include "gsyms.h"

/* Mark a function appropriately.  This should only be called for
   functions for which we are not emitting COFF debugging information.
   FILE is the assembler output file, NAME is the name of the
   function, and PUB is nonzero if the function is globally
   visible.  */

void
arm_pe_declare_function_type (FILE *file, const char *name, int pub)
{
  fprintf (file, "\t.def\t");
  assemble_name (file, name);
  fprintf (file, ";\t.scl\t%d;\t.type\t%d;\t.endef\n",
	   pub ? (int) C_EXT : (int) C_STAT,
	   (int) DT_FCN << N_BTSHFT);
}

/* Keep a list of external functions.  */

struct GTY(()) extern_list
{
  struct extern_list *next;
  tree decl;
  const char *name;
};

static GTY(()) struct extern_list *extern_head;

/* Assemble an external function reference.  We need to keep a list of
   these, so that we can output the function types at the end of the
   assembly.  We can't output the types now, because we might see a
   definition of the function later on and emit debugging information
   for it then.  */

void
arm_pe_record_external_function (tree decl, const char *name)
{
  struct extern_list *p;

  p = ggc_alloc<extern_list> ();
  p->next = extern_head;
  p->decl = decl;
  p->name = name;
  extern_head = p;
}

/* Keep a list of exported symbols.  */

struct GTY(()) export_list
{
  struct export_list *next;
  const char *name;
  int is_data;		/* used to type tag exported symbols.  */
};

/* Keep a list of stub symbols.  */

struct GTY(()) stub_list
{
  struct stub_list *next;
  const char *name;
};

static GTY(()) struct export_list *export_head;

static GTY(()) struct stub_list *stub_head;

/* Assemble an export symbol entry.  We need to keep a list of
   these, so that we can output the export list at the end of the
   assembly.  We used to output these export symbols in each function,
   but that causes problems with GNU ld when the sections are
   linkonce.  Beware, DECL may be NULL if compile_file() is emitting
   the LTO marker.  */

void
arm_pe_maybe_record_exported_symbol (tree decl, const char *name, int is_data)
{
  rtx symbol;
  struct export_list *p;

  if (!decl)
    return;

  symbol = XEXP (DECL_RTL (decl), 0);
  gcc_assert (GET_CODE (symbol) == SYMBOL_REF);
  if (!SYMBOL_REF_DLLEXPORT_P (symbol))
    return;

  gcc_assert (TREE_PUBLIC (decl));

  p = ggc_alloc<export_list> ();
  p->next = export_head;
  p->name = name;
  p->is_data = is_data;
  export_head = p;
}

void
arm_pe_record_stub (const char *name)
{
  struct stub_list *p;

  if (!name || *name == 0)
    return;

  p = stub_head;
  while (p != NULL)
    {
      if (p->name[0] == *name
          && !strcmp (p->name, name))
	return;
      p = p->next;
    }

  p = ggc_alloc<stub_list> ();
  p->next = stub_head;
  p->name = name;
  stub_head = p;
}


#ifdef CXX_WRAP_SPEC_LIST

/* Search for a function named TARGET in the list of library wrappers
   we are using, returning a pointer to it if found or NULL if not.
   This function might be called on quite a few symbols, and we only
   have the list of names of wrapped functions available to us as a
   spec string, so first time round we lazily initialise a hash table
   to make things quicker.  */

static const char *
arm_find_on_wrapper_list (const char *target)
{
  static char first_time = 1;
  static hash_table<nofree_string_hash> *wrappers;

  if (first_time)
    {
      /* Beware that this is not a complicated parser, it assumes
         that any sequence of non-whitespace beginning with an
	 underscore is one of the wrapped symbols.  For now that's
	 adequate to distinguish symbols from spec substitutions
	 and command-line options.  */
      static char wrapper_list_buffer[] = CXX_WRAP_SPEC_LIST;
      char *bufptr;
      /* Breaks up the char array into separated strings
         strings and enter them into the hash table.  */
      wrappers = new hash_table<nofree_string_hash> (8);
      for (bufptr = wrapper_list_buffer; *bufptr; ++bufptr)
	{
	  char *found = NULL;
	  if (ISSPACE (*bufptr))
	    continue;
	  if (*bufptr == '_')
	    found = bufptr;
	  while (*bufptr && !ISSPACE (*bufptr))
	    ++bufptr;
	  if (*bufptr)
	    *bufptr = 0;
	  if (found)
	    *wrappers->find_slot (found, INSERT) = found;
	}
      first_time = 0;
    }

  return wrappers->find (target);
}

#endif /* CXX_WRAP_SPEC_LIST */

/* This is called at the end of assembly.  For each external function
   which has not been defined, we output a declaration now.  We also
   output the .drectve section.  */

void
arm_pe_file_end (void)
{
  struct extern_list *p;

  for (p = extern_head; p != NULL; p = p->next)
    {
      tree decl;

      decl = p->decl;

      /* Positively ensure only one declaration for any given symbol.  */
      if (! TREE_ASM_WRITTEN (decl)
	  && TREE_SYMBOL_REFERENCED (DECL_ASSEMBLER_NAME (decl)))
	{
#ifdef CXX_WRAP_SPEC_LIST
	  /* To ensure the DLL that provides the corresponding real
	     functions is still loaded at runtime, we must reference
	     the real function so that an (unused) import is created.  */
	  const char *realsym = arm_find_on_wrapper_list (p->name);
	  if (realsym)
	    arm_pe_declare_function_type (asm_out_file,
		concat ("__real_", realsym, NULL), TREE_PUBLIC (decl));
#endif /* CXX_WRAP_SPEC_LIST */
	  TREE_ASM_WRITTEN (decl) = 1;
	  arm_pe_declare_function_type (asm_out_file, p->name,
					 TREE_PUBLIC (decl));
	}
    }

  if (export_head)
    {
      struct export_list *q;
      drectve_section ();
      for (q = export_head; q != NULL; q = q->next)
	{
	  fprintf (asm_out_file, "\t.ascii \" -export:\\\"%s\\\"%s\"\n",
		   default_strip_name_encoding (q->name),
		   (q->is_data ? ",data" : ""));
	}
    }

  if (stub_head)
    {
      struct stub_list *q;

      for (q = stub_head; q != NULL; q = q->next)
	{
	  const char *name = q->name;
	  const char *oname;

	  if (name[0] == '*')
	    ++name;
	  oname = name;
	  if (name[0] == '.')
	    ++name;
	  if (strncmp (name, "refptr.", 7) != 0)
	    continue;
	  name += 7;
	  fprintf (asm_out_file, "\t.section\t.rdata$%s, \"dr\"\n"
	  		   "\t.globl\t%s\n"
			   "\t.linkonce\tdiscard\n", oname, oname);
	  fprintf (asm_out_file, "%s:\n\t.quad\t%s\n", oname, name);
	}
    }
}

void
arm_pe_start_function (FILE *f, const char *name, tree decl)
{
  arm_pe_maybe_record_exported_symbol (decl, name, 0);
  if (write_symbols != SDB_DEBUG)
    arm_pe_declare_function_type (f, name, TREE_PUBLIC (decl));
  /* In case section was altered by debugging output.  */
  if (decl != NULL_TREE)
    switch_to_section (function_section (decl));
  if (TARGET_THUMB)
    fprintf (f, "\t.code 16\n");
  ASM_OUTPUT_FUNCTION_LABEL (f, name, decl);
}

void
arm_pe_end_function (FILE *f, const char *, tree)
{
}


#include "gt-pe.h"
