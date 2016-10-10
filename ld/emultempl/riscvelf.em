# This shell script emits a C file. -*- C -*-
#   Copyright (C) 2004-2022 Free Software Foundation, Inc.
#
# This file is part of the GNU Binutils.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
# MA 02110-1301, USA.

fragment <<EOF

#include "ldmain.h"
#include "ldctor.h"
#include "elf/riscv.h"
#include "elfxx-riscv.h"

static struct bfd_link_needed_list *force_needed_top;

static bool
_bfd_elf_link_create_dynstrtab (bfd *abfd, struct bfd_link_info *info)
{
  struct elf_link_hash_table *hash_table;

  hash_table = elf_hash_table (info);
  if (hash_table->dynobj == NULL)
    hash_table->dynobj = abfd;

  if (hash_table->dynstr == NULL)
    {
      hash_table->dynstr = _bfd_elf_strtab_init ();
      if (hash_table->dynstr == NULL)
	return false;
    }
  return true;
}

static void
elf_add_dt_needed_tag (bfd *abfd,
		       struct bfd_link_info *info,
		       const char *soname,
		       bool do_it)
{
  struct elf_link_hash_table *hash_table;
  bfd_size_type oldsize;
  bfd_size_type strindex;

  if (!_bfd_elf_link_create_dynstrtab (abfd, info))
    return;

  hash_table = elf_hash_table (info);
  oldsize = _bfd_elf_strtab_size (hash_table->dynstr);
  strindex = _bfd_elf_strtab_add (hash_table->dynstr, soname, false);
  if (strindex == (bfd_size_type) -1)
    return;

  if (oldsize == _bfd_elf_strtab_size (hash_table->dynstr))
    {
      asection *sdyn;
      const struct elf_backend_data *bed;
      bfd_byte *extdyn;

      bed = get_elf_backend_data (hash_table->dynobj);
      sdyn = bfd_get_section_by_name (hash_table->dynobj, ".dynamic");
      if (sdyn != NULL)
	for (extdyn = sdyn->contents;
	     extdyn < sdyn->contents + sdyn->size;
	     extdyn += bed->s->sizeof_dyn)
	  {
	    Elf_Internal_Dyn dyn;

	    bed->s->swap_dyn_in (hash_table->dynobj, extdyn, &dyn);
	    if (dyn.d_tag == DT_NEEDED
		&& dyn.d_un.d_val == strindex)
	      {
		_bfd_elf_strtab_delref (hash_table->dynstr, strindex);
		return;
	      }
	  }
    }

  if (do_it)
    {
      if (!_bfd_elf_link_create_dynamic_sections (hash_table->dynobj, info))
	return;

      if (!_bfd_elf_add_dynamic_entry (info, DT_NEEDED, strindex))
	return;
    }
  else
    /* We were just checking for existence of the tag.  */
    _bfd_elf_strtab_delref (hash_table->dynstr, strindex);

  return;
}

static void
riscv_elf_before_allocation (void)
{
  struct bfd_link_needed_list *entry = force_needed_top;
  while (entry) {
    elf_add_dt_needed_tag (link_info.output_bfd, &link_info, entry->name, true);
    entry = entry->next;
  }
  gld${EMULATION_NAME}_before_allocation ();

  if (link_info.discard == discard_sec_merge)
    link_info.discard = discard_l;

  if (!bfd_link_relocatable (&link_info))
    {
      /* We always need at least some relaxation to handle code alignment.  */
      if (RELAXATION_DISABLED_BY_USER)
	TARGET_ENABLE_RELAXATION;
      else
	ENABLE_RELAXATION;
    }

  link_info.relax_pass = 3;
}

static void
gld${EMULATION_NAME}_after_allocation (void)
{
  int need_layout = 0;

  /* Don't attempt to discard unused .eh_frame sections until the final link,
     as we can't reliably tell if they're used until after relaxation.  */
  if (!bfd_link_relocatable (&link_info))
    {
      need_layout = bfd_elf_discard_info (link_info.output_bfd, &link_info);
      if (need_layout < 0)
	{
	  einfo (_("%X%P: .eh_frame/.stab edit: %E\n"));
	  return;
	}
    }

  /* PR 27566, if the phase of data segment is exp_seg_relro_adjust,
     that means we are still adjusting the relro, and shouldn't do the
     relaxations at this stage.  Otherwise, we will get the symbol
     values beofore handling the relro, and may cause truncated fails
     when the relax range crossing the data segment.  One of the solution
     is to monitor the data segment phase while relaxing, to know whether
     the relro has been handled or not.

     I think we probably need to record more information about data
     segment or alignments in the future, to make sure it is safe
     to doing relaxations.  */
  enum phase_enum *phase = &(expld.dataseg.phase);
  bfd_elf${ELFSIZE}_riscv_set_data_segment_info (&link_info, (int *) phase);

  ldelf_map_segments (need_layout);
}

/* This is a convenient point to tell BFD about target specific flags.
   After the output has been created, but before inputs are read.  */

static void
riscv_create_output_section_statements (void)
{
  /* See PR 22920 for an example of why this is necessary.  */
  if (strstr (bfd_get_target (link_info.output_bfd), "riscv") == NULL)
    {
      /* The RISC-V backend needs special fields in the output hash structure.
	 These will only be created if the output format is a RISC-V format,
	 hence we do not support linking and changing output formats at the
	 same time.  Use a link followed by objcopy to change output formats.  */
      einfo (_("%F%P: error: cannot change output format"
	       " whilst linking %s binaries\n"), "RISC-V");
      return;
    }
}

EOF

# Define some shell vars to insert bits of code into the standard elf
# parse_args and list_options functions.
#
PARSE_AND_LIST_PROLOGUE='
#define OPTION_FORCE_ADD_NEEDED	300
'

PARSE_AND_LIST_LONGOPTS='
  { "force-add-needed", required_argument, NULL, OPTION_FORCE_ADD_NEEDED },
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("\
  --force-add-needed=<dso>    Force add needed\n"));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_FORCE_ADD_NEEDED:
      {
        struct bfd_link_needed_list *new_entry = xmalloc (sizeof (struct bfd_link_needed_list));
	if (new_entry) {
		new_entry->name = strdup(optarg);
		new_entry->by = 0;
		new_entry->next = 0;
		struct bfd_link_needed_list *entry = force_needed_top;;
		if (entry) {
			while (entry->next)
				entry = entry->next;
			entry->next = new_entry;
		} else {
			force_needed_top = new_entry;
		}
	}
      }
      break;
'

LDEMUL_BEFORE_ALLOCATION=riscv_elf_before_allocation
LDEMUL_AFTER_ALLOCATION=gld${EMULATION_NAME}_after_allocation
LDEMUL_CREATE_OUTPUT_SECTION_STATEMENTS=riscv_create_output_section_statements
