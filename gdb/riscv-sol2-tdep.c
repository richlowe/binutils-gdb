/* Target-dependent code for Newlib AArch64.

   Copyright (C) 2011-2014 Free Software Foundation, Inc.
   Contributed by ARM Ltd.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "frame.h"
#include "frame-unwind.h"
#include "gdbarch.h"
#include "riscv-tdep.h"
#include "riscv-sol2-tdep.h"
#include "osabi.h"
#include "inferior.h"
#include "regcache.h"
#include "regset.h"
#include "solib-svr4.h"
#include "trad-frame.h"
#include "sol2-tdep.h"
#include "stap-probe.h"
#include "parser-defs.h"
#include "user-regs.h"
#include <ctype.h>

/* Register maps.  */

static const struct regcache_map_entry riscv_sol2_gregmap[] =
  {
  { 31, RISCV_RA_REGNUM, 0 }, /* x1 to x31 */
  { 1,  RISCV_PC_REGNUM, 0 },
  { 0 }
};

static const struct regcache_map_entry riscv_sol2_fpregmap[] =
  {
  { 32, RISCV_FIRST_FP_REGNUM, 0 },
  { 1, RISCV_CSR_FCSR_REGNUM, 0 },
  { 0 }
};

/* Register set definitions.  */

const struct regset riscv_sol2_gregset =
  {
    riscv_sol2_gregmap, regcache_supply_regset, regcache_collect_regset
  };

const struct regset riscv_sol2_fpregset =
  {
    riscv_sol2_fpregmap,
    regcache_supply_regset, regcache_collect_regset
  };

/* Implement the "regset_from_core_section" gdbarch method.  */

static void
riscv_sol2_iterate_over_regset_sections (struct gdbarch *gdbarch,
					    iterate_over_regset_sections_cb *cb,
					    void *cb_data,
					    const struct regcache *regcache)
{
  cb (".reg", (32 * riscv_isa_xlen (gdbarch)), (32 * riscv_isa_xlen (gdbarch)),
      &riscv_sol2_gregset, NULL, cb_data);
  cb (".reg2", (32 * riscv_isa_flen (gdbarch)) + 8,
      (32 * riscv_isa_flen (gdbarch)) + 8,
      &riscv_sol2_fpregset, NULL, cb_data);
}


struct riscv_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  CORE_ADDR pc;

  /* Do we have a frame?  */
  int frameless_p;

  /* The offset from the base register to the CFA.  */
  int frame_offset;

  /* Mask of `local' and `in' registers saved in the register save area.  */
  unsigned short int saved_regs_mask;

  /* Mask of `out' registers copied or renamed to their `in' sibling.  */
  unsigned char copied_regs_mask;

  /* Do we have a Structure, Union or Quad-Precision return value?  */
  int struct_return_p;

  /* Table of saved registers.  */
  struct trad_frame_saved_reg *saved_regs;
};

static CORE_ADDR
riscv_sol2_mcontext_addr (struct frame_info *this_frame)
{
  CORE_ADDR ucontext_addr;

  ucontext_addr = get_frame_register_unsigned (this_frame, RISCV_A0_REGNUM + 2);
  return ucontext_addr + 8 * 7;
}

static struct riscv_frame_cache *
riscv_alloc_frame_cache (void)
{
  struct riscv_frame_cache *cache;

  cache = FRAME_OBSTACK_ZALLOC (struct riscv_frame_cache);

  /* Base address.  */
  cache->base = 0;
  cache->pc = 0;

  /* Frameless until proven otherwise.  */
  cache->frameless_p = 1;
  cache->frame_offset = 0;
  cache->saved_regs_mask = 0;
  cache->copied_regs_mask = 0;
  cache->struct_return_p = 0;

  return cache;
}

static struct riscv_frame_cache *
riscv_sol2_sigtramp_frame_cache (struct frame_info *this_frame,
				   void **this_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  struct riscv_frame_cache *cache;
  CORE_ADDR mcontext_addr;
  int regnum;
  int xlen = riscv_isa_xlen (gdbarch);
  int flen = riscv_isa_flen (gdbarch);

  if (*this_cache)
    return (riscv_frame_cache*)*this_cache;

  cache = riscv_alloc_frame_cache ();
  *this_cache = cache;

  cache->saved_regs = trad_frame_alloc_saved_regs (this_frame);

  mcontext_addr = riscv_sol2_mcontext_addr (this_frame);

  for (regnum = 0; regnum < 31; regnum++)
	  cache->saved_regs[regnum].set_addr(mcontext_addr + regnum * xlen);
  cache->saved_regs[RISCV_PC_REGNUM].set_addr(mcontext_addr + 31 * xlen);	// pc

  for (regnum = 0; regnum < 32; regnum++)
	  cache->saved_regs[regnum + RISCV_FIRST_FP_REGNUM].set_addr(mcontext_addr + 32 * xlen + regnum * flen);

  cache->saved_regs[RISCV_CSR_FCSR_REGNUM].set_addr(mcontext_addr + 32 * xlen + 32 * flen);

  return cache;
}

static void
riscv_sol2_sigtramp_frame_this_id (struct frame_info *this_frame,
				     void **this_cache,
				     struct frame_id *this_id)
{
  struct riscv_frame_cache *cache =
    riscv_sol2_sigtramp_frame_cache (this_frame, this_cache);

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static struct value *
riscv_sol2_sigtramp_frame_prev_register (struct frame_info *this_frame,
					   void **this_cache,
					   int regnum)
{
  struct riscv_frame_cache *cache =
    riscv_sol2_sigtramp_frame_cache (this_frame, this_cache);

  struct value *ret = trad_frame_get_prev_register (this_frame, cache->saved_regs, regnum);
  return ret;
}

static int
riscv_sol2_sigtramp_frame_sniffer (const struct frame_unwind *self,
				     struct frame_info *this_frame,
				     void **this_cache)
{
  CORE_ADDR pc = get_frame_pc (this_frame);
  const char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  return (name && (strcmp ("sigacthandler", name) == 0 || strcmp (name, "ucbsigvechandler") == 0));
}


static const struct frame_unwind riscv_sol2_sigtramp_frame_unwind =
{
  "riscv solaris sigtramp",
  SIGTRAMP_FRAME,
  default_frame_unwind_stop_reason,
  riscv_sol2_sigtramp_frame_this_id,
  riscv_sol2_sigtramp_frame_prev_register,
  NULL,
  riscv_sol2_sigtramp_frame_sniffer
};


/* Implement the 'init_osabi' method of struct gdb_osabi_handler.  */

static void
riscv_sol2_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  frame_unwind_append_unwinder (gdbarch, &riscv_sol2_sigtramp_frame_unwind);
  sol2_init_abi (info, gdbarch);

  /* Solaris has SVR4-style shared libraries...  */
  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);
  set_solib_svr4_fetch_link_map_offsets (gdbarch,
					 (riscv_isa_xlen (gdbarch) == 4
					  ? svr4_ilp32_fetch_link_map_offsets
					  : svr4_lp64_fetch_link_map_offsets));

  set_gdbarch_software_single_step (gdbarch, riscv_software_single_step);
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
extern initialize_file_ftype _initialize_riscv_sol2_tdep;

void
_initialize_riscv_sol2_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_riscv, 0, GDB_OSABI_SOLARIS,
			  riscv_sol2_init_abi);
}
