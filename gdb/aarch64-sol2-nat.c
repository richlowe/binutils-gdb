/* Native-dependent code for Solaris SPARC.

   Copyright (C) 2003-2014 Free Software Foundation, Inc.

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

#include "aarch64-tdep.h"
#include "aarch64-sol2-tdep.h"
#include "target.h"
#include "procfs.h"

void
supply_gregset (struct regcache *regcache, const prgregset_t *gregs)
{
	regcache_supply_regset (&aarch64_sol2_gregset, regcache, -1, (const gdb_byte *) gregs, AARCH64_SOL2_SIZEOF_GREGSET);
}

void
supply_fpregset (struct regcache *regcache, const prfpregset_t *fpregs)
{
	regcache_supply_regset (&aarch64_sol2_fpregset, regcache, -1, (const gdb_byte *) fpregs, AARCH64_SOL2_SIZEOF_FPREGSET);
}

void
fill_gregset (const struct regcache *regcache, prgregset_t *gregs, int regnum)
{
	regcache_collect_regset (&aarch64_sol2_gregset, regcache, regnum, (gdb_byte *) gregs, AARCH64_SOL2_SIZEOF_GREGSET);
}

void
fill_fpregset (const struct regcache *regcache, prfpregset_t *fpregs, int regnum)
{
	regcache_collect_regset (&aarch64_sol2_fpregset, regcache, regnum, (gdb_byte *) fpregs, AARCH64_SOL2_SIZEOF_FPREGSET);
}
