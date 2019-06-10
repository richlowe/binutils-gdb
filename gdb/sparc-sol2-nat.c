/* Native-dependent code for Solaris SPARC.

   Copyright (C) 2003-2017 Free Software Foundation, Inc.

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
#include "regcache.h"

#include <sys/procfs.h>
#include "gregset.h"

#include "sparc-tdep.h"
#include "sparc64-tdep.h"
#include "target.h"
#include "procfs.h"

/* Solaris 7 (Solaris 2.7, SunOS 5.7) and up support two process data
   models, the traditional 32-bit data model (ILP32) and the 64-bit
   data model (LP64).  The format of /proc depends on the data model
   of the observer (the controlling process, GDB in our case).  The
   Solaris header files conveniently define PR_MODEL_NATIVE to the
   data model of the controlling process.  If its value is
   PR_MODEL_LP64, we know that GDB is being compiled as a 64-bit
   program.

   GNU/Linux uses the same formats as Solaris for its core files (but
   not for ptrace(2)).  The GNU/Linux headers don't define
   PR_MODEL_NATIVE though.  Therefore we rely on the __arch64__ define
   provided by GCC to determine the appropriate data model.

   Note that a 32-bit GDB won't be able to debug a 64-bit target
   process using /proc on Solaris.  */

static int from_corefile = -1;

void
supply_gregset (struct regcache *regcache, const prgregset_t *gregs)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  int pointer_size = gdbarch_ptr_bit (gdbarch);

  if (from_corefile == -1)
    from_corefile = regcache_from_corefile (regcache);

  if ((pointer_size == 32) && (from_corefile == 1))
    sparc32_supply_gregset (&sparc32_sol2_gregmap, regcache, -1, gregs);
  else
    sparc64_supply_gregset (&sparc64_sol2_gregmap, regcache, -1, gregs);
}

void
supply_fpregset (struct regcache *regcache, const prfpregset_t *fpregs)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  int pointer_size = gdbarch_ptr_bit (gdbarch);

  if (from_corefile == -1)
    from_corefile = regcache_from_corefile (regcache);

  if ((pointer_size == 32) && (from_corefile == 1))
    sparc32_supply_fpregset (&sparc32_sol2_fpregmap, regcache, -1, fpregs);
  else
    sparc64_supply_fpregset (&sparc64_sol2_fpregmap, regcache, -1, fpregs);
}

void
fill_gregset (const struct regcache *regcache, prgregset_t *gregs, int regnum)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  int pointer_size = gdbarch_ptr_bit (gdbarch);

  if (from_corefile == -1)
    from_corefile = regcache_from_corefile (regcache);

  if ((pointer_size == 32) && (from_corefile == 1))
    sparc32_collect_gregset (&sparc32_sol2_gregmap, regcache, regnum, gregs);
  else
    sparc64_collect_gregset (&sparc64_sol2_gregmap, regcache, regnum, gregs);
}

void
fill_fpregset (const struct regcache *regcache,
	       prfpregset_t *fpregs, int regnum)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  int pointer_size = gdbarch_ptr_bit (gdbarch);

  if (from_corefile == -1)
    from_corefile = regcache_from_corefile (regcache);

  if ((pointer_size == 32) && (from_corefile == 1))
    sparc32_collect_fpregset (&sparc32_sol2_fpregmap, regcache, regnum, fpregs);
  else
    sparc64_collect_fpregset (&sparc64_sol2_fpregmap, regcache, regnum, fpregs);
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
extern initialize_file_ftype _initialize_sparc_sol2_nat;

void
_initialize_sparc_sol2_nat (void)
{
  struct target_ops *t;

  t = procfs_target ();
#ifdef NEW_PROC_API	/* Solaris 6 and above can do HW watchpoints.  */
  procfs_use_watchpoints (t);
#endif
  add_target (t);
}
