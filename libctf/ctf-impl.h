/* Implementation header.
   Copyright (C) 2019 Free Software Foundation, Inc.

   This file is part of libctf.

   libctf is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not see
   <http://www.gnu.org/licenses/>.  */

#ifndef	_CTF_IMPL_H
#define	_CTF_IMPL_H

#include "config.h"
#include <errno.h>
#include "ctf-decls.h"
#include <ctf-api.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include <elf.h>
#include <bfd.h>

#ifdef	__cplusplus
extern "C"
  {
#endif

/* Compiler attributes.  */

#if defined (__GNUC__)

/* GCC.  We assume that all compilers claiming to be GCC support sufficiently
   many GCC attributes that the code below works.  If some non-GCC compilers
   masquerading as GCC in fact do not implement these attributes, version checks
   may be required.  */

/* We use the _libctf_*_ pattern to avoid clashes with any future attribute
   macros glibc may introduce, which have names of the pattern
   __attribute_blah__.  */

#define _libctf_printflike_(string_index,first_to_check) \
    __attribute__ ((__format__ (__printf__, (string_index), (first_to_check))))
#define _libctf_unlikely_(x) __builtin_expect ((x), 0)
#define _libctf_unused_ __attribute__ ((__unused__))
#define _libctf_malloc_ __attribute__((__malloc__))

#endif

/* libctf in-memory state.  */

typedef struct ctf_fixed_hash ctf_hash_t; /* Private to ctf-hash.c.  */
typedef struct ctf_dynhash ctf_dynhash_t; /* Private to ctf-hash.c.  */

typedef struct ctf_strs
{
  const char *cts_strs;		/* Base address of string table.  */
  size_t cts_len;		/* Size of string table in bytes.  */
} ctf_strs_t;

typedef struct ctf_dmodel
{
  const char *ctd_name;		/* Data model name.  */
  int ctd_code;			/* Data model code.  */
  size_t ctd_pointer;		/* Size of void * in bytes.  */
  size_t ctd_char;		/* Size of char in bytes.  */
  size_t ctd_short;		/* Size of short in bytes.  */
  size_t ctd_int;		/* Size of int in bytes.  */
  size_t ctd_long;		/* Size of long in bytes.  */
} ctf_dmodel_t;

typedef struct ctf_lookup
{
  const char *ctl_prefix;	/* String prefix for this lookup.  */
  size_t ctl_len;		/* Length of prefix string in bytes.  */
  ctf_hash_t *ctl_hash;		/* Pointer to hash table for lookup.  */
} ctf_lookup_t;

typedef struct ctf_fileops
{
  uint32_t (*ctfo_get_kind) (uint32_t);
  uint32_t (*ctfo_get_root) (uint32_t);
  uint32_t (*ctfo_get_vlen) (uint32_t);
  ssize_t (*ctfo_get_ctt_size) (const ctf_file_t *, const ctf_type_t *,
				ssize_t *, ssize_t *);
  ssize_t (*ctfo_get_vbytes) (unsigned short, ssize_t, size_t);
} ctf_fileops_t;

typedef struct ctf_list
{
  struct ctf_list *l_prev;	/* Previous pointer or tail pointer.  */
  struct ctf_list *l_next;	/* Next pointer or head pointer.  */
} ctf_list_t;

typedef enum
  {
   CTF_PREC_BASE,
   CTF_PREC_POINTER,
   CTF_PREC_ARRAY,
   CTF_PREC_FUNCTION,
   CTF_PREC_MAX
  } ctf_decl_prec_t;

typedef struct ctf_decl_node
{
  ctf_list_t cd_list;		/* Linked list pointers.  */
  ctf_id_t cd_type;		/* Type identifier.  */
  uint32_t cd_kind;		/* Type kind.  */
  uint32_t cd_n;		/* Type dimension if array.  */
} ctf_decl_node_t;

typedef struct ctf_decl
{
  ctf_list_t cd_nodes[CTF_PREC_MAX]; /* Declaration node stacks.  */
  int cd_order[CTF_PREC_MAX];	     /* Storage order of decls.  */
  ctf_decl_prec_t cd_qualp;	     /* Qualifier precision.  */
  ctf_decl_prec_t cd_ordp;	     /* Ordered precision.  */
  char *cd_buf;			     /* Buffer for output.  */
  int cd_err;			     /* Saved error value.  */
  int cd_enomem;		     /* Nonzero if OOM during printing.  */
} ctf_decl_t;

typedef struct ctf_dmdef
{
  ctf_list_t dmd_list;		/* List forward/back pointers.  */
  char *dmd_name;		/* Name of this member.  */
  ctf_id_t dmd_type;		/* Type of this member (for sou).  */
  unsigned long dmd_offset;	/* Offset of this member in bits (for sou).  */
  int dmd_value;		/* Value of this member (for enum).  */
} ctf_dmdef_t;

typedef struct ctf_dtdef
{
  ctf_list_t dtd_list;		/* List forward/back pointers.  */
  char *dtd_name;		/* Name associated with definition (if any).  */
  ctf_id_t dtd_type;		/* Type identifier for this definition.  */
  ctf_type_t dtd_data;		/* Type node (see <ctf.h>).  */
  union
  {
    ctf_list_t dtu_members;	/* struct, union, or enum */
    ctf_arinfo_t dtu_arr;	/* array */
    ctf_encoding_t dtu_enc;	/* integer or float */
    ctf_id_t *dtu_argv;		/* function */
    ctf_slice_t dtu_slice;	/* slice */
  } dtd_u;
} ctf_dtdef_t;

typedef struct ctf_dvdef
{
  ctf_list_t dvd_list;		/* List forward/back pointers.  */
  char *dvd_name;		/* Name associated with variable.  */
  ctf_id_t dvd_type;		/* Type of variable.  */
  unsigned long dvd_snapshots;	/* Snapshot count when inserted.  */
} ctf_dvdef_t;

typedef struct ctf_bundle
{
  ctf_file_t *ctb_file;		/* CTF container handle.  */
  ctf_id_t ctb_type;		/* CTF type identifier.  */
  ctf_dtdef_t *ctb_dtd;		/* CTF dynamic type definition (if any).  */
} ctf_bundle_t;

/* The ctf_file is the structure used to represent a CTF container to library
   clients, who see it only as an opaque pointer.  Modifications can therefore
   be made freely to this structure without regard to client versioning.  The
   ctf_file_t typedef appears in <ctf-api.h> and declares a forward tag.

   NOTE: ctf_update() requires that everything inside of ctf_file either be an
   immediate value, a pointer to dynamically allocated data *outside* of the
   ctf_file itself, or a pointer to statically allocated data.  If you add a
   pointer to ctf_file that points to something within the ctf_file itself,
   you must make corresponding changes to ctf_update().  */

struct ctf_file
{
  const ctf_fileops_t *ctf_fileops; /* Version-specific file operations.  */
  ctf_sect_t ctf_data;		    /* CTF data from object file.  */
  ctf_sect_t ctf_symtab;	    /* Symbol table from object file.  */
  ctf_sect_t ctf_strtab;	    /* String table from object file.  */
  void *ctf_data_mmapped;	    /* CTF data we mmapped, to free later.  */
  size_t ctf_data_mmapped_len;	    /* Length of CTF data we mmapped.  */
  ctf_hash_t *ctf_structs;	    /* Hash table of struct types.  */
  ctf_hash_t *ctf_unions;	    /* Hash table of union types.  */
  ctf_hash_t *ctf_enums;	    /* Hash table of enum types.  */
  ctf_hash_t *ctf_names;	    /* Hash table of remaining type names.  */
  ctf_lookup_t ctf_lookups[5];	    /* Pointers to hashes for name lookup.  */
  ctf_strs_t ctf_str[2];	    /* Array of string table base and bounds.  */
  const unsigned char *ctf_base;  /* Base of CTF header + uncompressed buffer.  */
  const unsigned char *ctf_buf;	  /* Uncompressed CTF data buffer.  */
  size_t ctf_size;		  /* Size of CTF header + uncompressed data.  */
  uint32_t *ctf_sxlate;		  /* Translation table for symtab entries.  */
  unsigned long ctf_nsyms;	  /* Number of entries in symtab xlate table.  */
  uint32_t *ctf_txlate;		  /* Translation table for type IDs.  */
  uint32_t *ctf_ptrtab;		  /* Translation table for pointer-to lookups.  */
  struct ctf_varent *ctf_vars;	  /* Sorted variable->type mapping.  */
  unsigned long ctf_nvars;	  /* Number of variables in ctf_vars.  */
  unsigned long ctf_typemax;	  /* Maximum valid type ID number.  */
  const ctf_dmodel_t *ctf_dmodel; /* Data model pointer (see above).  */
  struct ctf_file *ctf_parent;	  /* Parent CTF container (if any).  */
  const char *ctf_parlabel;	  /* Label in parent container (if any).  */
  const char *ctf_parname;	  /* Basename of parent (if any).  */
  char *ctf_dynparname;		  /* Dynamically allocated name of parent.  */
  uint32_t ctf_parmax;		  /* Highest type ID of a parent type.  */
  uint32_t ctf_refcnt;		  /* Reference count (for parent links).  */
  uint32_t ctf_flags;		  /* Libctf flags (see below).  */
  int ctf_errno;		  /* Error code for most recent error.  */
  int ctf_version;		  /* CTF data version.  */
  ctf_dynhash_t *ctf_dthash;	  /* Hash of dynamic type definitions.  */
  ctf_dynhash_t *ctf_dtbyname;	  /* DTDs, indexed by name.  */
  ctf_list_t ctf_dtdefs;	  /* List of dynamic type definitions.  */
  ctf_dynhash_t *ctf_dvhash;	  /* Hash of dynamic variable mappings.  */
  ctf_list_t ctf_dvdefs;	  /* List of dynamic variable definitions.  */
  size_t ctf_dtvstrlen;		  /* Total length of dynamic type+var strings.  */
  unsigned long ctf_dtnextid;	  /* Next dynamic type id to assign.  */
  unsigned long ctf_dtoldid;	  /* Oldest id that has been committed.  */
  unsigned long ctf_snapshots;	  /* ctf_snapshot() plus ctf_update() count.  */
  unsigned long ctf_snapshot_lu;  /* ctf_snapshot() call count at last update.  */
  ctf_archive_t *ctf_archive;	  /* Archive this ctf_file_t came from.  */
  char *ctf_tmp_typeslice;	  /* Storage for slicing up type names.  */
  size_t ctf_tmp_typeslicelen;	  /* Size of the typeslice.  */
  void *ctf_specific;		  /* Data for ctf_get/setspecific().  */
};

/* An abstraction over both a ctf_file_t and a ctf_archive_t.  */

struct ctf_archive_internal
{
  int ctfi_is_archive;
  ctf_file_t *ctfi_file;
  struct ctf_archive *ctfi_archive;
  ctf_sect_t ctfi_symsect;
  ctf_sect_t ctfi_strsect;
  void *ctfi_data;
  bfd *ctfi_abfd;		    /* Optional source of section data.  */
  void (*ctfi_bfd_close) (struct ctf_archive_internal *);
};

/* Return x rounded up to an alignment boundary.
   eg, P2ROUNDUP(0x1234, 0x100) == 0x1300 (0x13*align)
   eg, P2ROUNDUP(0x5600, 0x100) == 0x5600 (0x56*align)  */
#define P2ROUNDUP(x, align)		(-(-(x) & -(align)))

/* * If an offs is not aligned already then round it up and align it. */
#define LCTF_ALIGN_OFFS(offs, align) ((offs + (align - 1)) & ~(align - 1))

#define LCTF_TYPE_ISPARENT(fp, id) ((id) <= fp->ctf_parmax)
#define LCTF_TYPE_ISCHILD(fp, id) ((id) > fp->ctf_parmax)
#define LCTF_TYPE_TO_INDEX(fp, id) ((id) & (fp->ctf_parmax))
#define LCTF_INDEX_TO_TYPE(fp, id, child) (child ? ((id) | (fp->ctf_parmax+1)) : \
					   (id))

#define LCTF_INDEX_TO_TYPEPTR(fp, i) \
  ((ctf_type_t *)((uintptr_t)(fp)->ctf_buf + (fp)->ctf_txlate[(i)]))

#define LCTF_INFO_KIND(fp, info)	((fp)->ctf_fileops->ctfo_get_kind(info))
#define LCTF_INFO_ISROOT(fp, info)	((fp)->ctf_fileops->ctfo_get_root(info))
#define LCTF_INFO_VLEN(fp, info)	((fp)->ctf_fileops->ctfo_get_vlen(info))
#define LCTF_VBYTES(fp, kind, size, vlen) \
  ((fp)->ctf_fileops->ctfo_get_vbytes(kind, size, vlen))

static inline ssize_t ctf_get_ctt_size (const ctf_file_t *fp,
					const ctf_type_t *tp,
					ssize_t *sizep,
					ssize_t *incrementp)
{
  return (fp->ctf_fileops->ctfo_get_ctt_size (fp, tp, sizep, incrementp));
}

#define LCTF_CHILD	0x0001	/* CTF container is a child */
#define LCTF_RDWR	0x0002	/* CTF container is writable */
#define LCTF_DIRTY	0x0004	/* CTF container has been modified */

extern const ctf_type_t *ctf_lookup_by_id (ctf_file_t **, ctf_id_t);

typedef unsigned int (*ctf_hash_fun) (const void *ptr);
extern unsigned int ctf_hash_integer (const void *ptr);
extern unsigned int ctf_hash_string (const void *ptr);

typedef int (*ctf_hash_eq_fun) (const void *, const void *);
extern int ctf_hash_eq_integer (const void *, const void *);
extern int ctf_hash_eq_string (const void *, const void *);

typedef void (*ctf_hash_free_fun) (void *);

extern ctf_hash_t *ctf_hash_create (unsigned long, ctf_hash_fun, ctf_hash_eq_fun);
extern int ctf_hash_insert_type (ctf_hash_t *, ctf_file_t *, uint32_t, uint32_t);
extern int ctf_hash_define_type (ctf_hash_t *, ctf_file_t *, uint32_t, uint32_t);
extern ctf_id_t ctf_hash_lookup_type (ctf_hash_t *, ctf_file_t *, const char *);
extern uint32_t ctf_hash_size (const ctf_hash_t *);
extern void ctf_hash_destroy (ctf_hash_t *);

extern ctf_dynhash_t *ctf_dynhash_create (ctf_hash_fun, ctf_hash_eq_fun,
					  ctf_hash_free_fun, ctf_hash_free_fun);
extern int ctf_dynhash_insert (ctf_dynhash_t *, void *, void *);
extern void ctf_dynhash_remove (ctf_dynhash_t *, const void *);
extern void *ctf_dynhash_lookup (ctf_dynhash_t *, const void *);
extern void ctf_dynhash_destroy (ctf_dynhash_t *);

#define	ctf_list_prev(elem)	((void *)(((ctf_list_t *)(elem))->l_prev))
#define	ctf_list_next(elem)	((void *)(((ctf_list_t *)(elem))->l_next))

extern void ctf_list_append (ctf_list_t *, void *);
extern void ctf_list_prepend (ctf_list_t *, void *);
extern void ctf_list_delete (ctf_list_t *, void *);

extern void ctf_dtd_insert (ctf_file_t *, ctf_dtdef_t *);
extern void ctf_dtd_delete (ctf_file_t *, ctf_dtdef_t *);
extern ctf_dtdef_t *ctf_dtd_lookup (const ctf_file_t *, ctf_id_t);
extern ctf_dtdef_t *ctf_dynamic_type (const ctf_file_t *, ctf_id_t);

extern void ctf_dvd_insert (ctf_file_t *, ctf_dvdef_t *);
extern void ctf_dvd_delete (ctf_file_t *, ctf_dvdef_t *);
extern ctf_dvdef_t *ctf_dvd_lookup (const ctf_file_t *, const char *);

extern void ctf_decl_init (ctf_decl_t *);
extern void ctf_decl_fini (ctf_decl_t *);
extern void ctf_decl_push (ctf_decl_t *, ctf_file_t *, ctf_id_t);

_libctf_printflike_ (2, 3)
extern void ctf_decl_sprintf (ctf_decl_t *, const char *, ...);
extern char *ctf_decl_buf (ctf_decl_t *cd);

extern const char *ctf_strraw (ctf_file_t *, uint32_t);
extern const char *ctf_strptr (ctf_file_t *, uint32_t);

extern struct ctf_archive *ctf_arc_open_internal (const char *, int *);
extern struct ctf_archive *ctf_arc_bufopen (const void *, size_t, int *);
extern void ctf_arc_close_internal (struct ctf_archive *);
extern void *ctf_set_open_errno (int *, int);
extern unsigned long ctf_set_errno (ctf_file_t *, int);

_libctf_malloc_
extern void *ctf_data_alloc (size_t);
extern void ctf_data_free (void *, size_t);
extern void ctf_data_protect (void *, size_t);

_libctf_malloc_
extern void *ctf_mmap (size_t length, size_t offset, int fd);
extern void ctf_munmap (void *, size_t);
extern ssize_t ctf_pread (int fd, void *buf, ssize_t count, off_t offset);

_libctf_malloc_
extern void *ctf_alloc (size_t);
extern void ctf_free (void *);

_libctf_malloc_
extern char *ctf_strdup (const char *);
extern char *ctf_str_append (char *, const char *);
extern const char *ctf_strerror (int);

extern ctf_id_t ctf_type_resolve_unsliced (ctf_file_t *, ctf_id_t);
extern int ctf_type_kind_unsliced (ctf_file_t *, ctf_id_t);

_libctf_printflike_ (1, 2)
extern void ctf_dprintf (const char *, ...);
extern void libctf_init_debug (void);

extern Elf64_Sym *ctf_sym_to_elf64 (const Elf32_Sym *src, Elf64_Sym *dst);
extern const char *ctf_lookup_symbol_name (ctf_file_t *fp, unsigned long symidx);

/* Variables, all underscore-prepended. */

extern const char _CTF_SECTION[];	/* name of CTF ELF section */
extern const char _CTF_NULLSTR[];	/* empty string */

extern int _libctf_version;	/* library client version */
extern int _libctf_debug;	/* debugging messages enabled */

#ifdef	__cplusplus
}
#endif

#endif /* _CTF_IMPL_H */
