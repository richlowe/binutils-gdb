/* GNU/Linux on AArch64 target support, prototypes.

   Copyright (C) 2012-2014 Free Software Foundation, Inc.
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

#include "regset.h"

/* X0 - X31, SP, PC, PSR, TP */
#define AARCH64_SOL2_SIZEOF_GREGSET  (36 * X_REGISTER_SIZE)

/* V0 - V31, FPCR, FPSR */
#define AARCH64_SOL2_SIZEOF_FPREGSET (33 * V_REGISTER_SIZE)

extern const struct regset aarch64_sol2_gregset;
extern const struct regset aarch64_sol2_fpregset;

