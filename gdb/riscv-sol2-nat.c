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

#include "riscv-tdep.h"
#include "riscv-sol2-tdep.h"
#include "target.h"
#include "procfs.h"

void
supply_gregset (struct regcache *regcache, const prgregset_t *gregs)
{
	int xlen = register_size (regcache->arch(), RISCV_RA_REGNUM);
	regcache_supply_regset (&riscv_sol2_gregset, regcache, -1, (const gdb_byte *) gregs, xlen * 32);
}

void
supply_fpregset (struct regcache *regcache, const prfpregset_t *fpregs)
{
	int flen = register_size (regcache->arch(), RISCV_FIRST_FP_REGNUM);
	regcache_supply_regset (&riscv_sol2_fpregset, regcache, -1, (const gdb_byte *) fpregs, flen * 32 + 8);
}

void
fill_gregset (const struct regcache *regcache, prgregset_t *gregs, int regnum)
{
	int xlen = register_size (regcache->arch(), RISCV_RA_REGNUM);
	regcache_collect_regset (&riscv_sol2_gregset, regcache, regnum, (gdb_byte *) gregs, xlen * 32);
}

void
fill_fpregset (const struct regcache *regcache, prfpregset_t *fpregs, int regnum)
{
	int flen = register_size (regcache->arch(), RISCV_FIRST_FP_REGNUM);
	regcache_collect_regset (&riscv_sol2_fpregset, regcache, regnum, (gdb_byte *) fpregs, flen * 32 + 8);
}
