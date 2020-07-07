/*
  XTulator: A portable, open-source 80186 PC emulator.
  Copyright (C)2020 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

//Be sure to only define ONE of the CPU_* options at any given time, or you will likely get some unexpected/bad results!

//#define CPU_8086
//#define CPU_186
#define CPU_V20
//#define CPU_286

#if defined(CPU_8086)
#define CPU_CLEAR_ZF_ON_MUL
#define CPU_ALLOW_POP_CS
#else
#define CPU_ALLOW_ILLEGAL_OP_EXCEPTION
#define CPU_LIMIT_SHIFT_COUNT
#endif

#if defined(CPU_V20)
#define CPU_NO_SALC
#endif

#if defined(CPU_286) || defined(CPU_386)
#define CPU_286_STYLE_PUSH_SP
#else
#define CPU_SET_HIGH_FLAGS
#endif
