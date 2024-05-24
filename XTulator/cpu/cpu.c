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

#include <stdint.h>
#include <stddef.h>
#include "cpu.h"
#include "../config.h"
#include "../debuglog.h"

const uint8_t byteregtable[8] = { regal, regcl, regdl, regbl, regah, regch, regdh, regbh };

const uint8_t parity[0x100] = {
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};

FUNC_INLINE void cpu_writew(CPU_t* cpu, uint32_t addr32, uint16_t value) {
	cpu_write(cpu, addr32, (uint8_t)value);
	cpu_write(cpu, addr32 + 1, (uint8_t)(value >> 8));
}

FUNC_INLINE uint16_t cpu_readw(CPU_t* cpu, uint32_t addr32) {
	return ((uint16_t)cpu_read(cpu, addr32) | (uint16_t)(cpu_read(cpu, addr32 + 1) << 8));
}

FUNC_INLINE void flag_szp8(CPU_t* cpu, uint8_t value) {
	if (!value) {
		cpu->zf = 1;
	}
	else {
		cpu->zf = 0;	/* set or clear zero flag */
	}

	if (value & 0x80) {
		cpu->sf = 1;
	}
	else {
		cpu->sf = 0;	/* set or clear sign flag */
	}

	cpu->pf = parity[value]; /* retrieve parity state from lookup table */
}

FUNC_INLINE void flag_szp16(CPU_t* cpu, uint16_t value) {
	if (!value) {
		cpu->zf = 1;
	}
	else {
		cpu->zf = 0;	/* set or clear zero flag */
	}

	if (value & 0x8000) {
		cpu->sf = 1;
	}
	else {
		cpu->sf = 0;	/* set or clear sign flag */
	}

	cpu->pf = parity[value & 255];	/* retrieve parity state from lookup table */
}

FUNC_INLINE void flag_log8(CPU_t* cpu, uint8_t value) {
	flag_szp8(cpu, value);
	cpu->cf = 0;
	cpu->of = 0; /* bitwise logic ops always clear carry and overflow */
}

FUNC_INLINE void flag_log16(CPU_t* cpu, uint16_t value) {
	flag_szp16(cpu, value);
	cpu->cf = 0;
	cpu->of = 0; /* bitwise logic ops always clear carry and overflow */
}

FUNC_INLINE void flag_adc8(CPU_t* cpu, uint8_t v1, uint8_t v2, uint8_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint16_t	dst;

	dst = (uint16_t)v1 + (uint16_t)v2 + (uint16_t)v3;
	flag_szp8(cpu, (uint8_t)dst);
	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0; /* set or clear overflow flag */
	}

	if (dst & 0xFF00) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0; /* set or clear carry flag */
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0; /* set or clear auxilliary flag */
	}
}

FUNC_INLINE void flag_adc16(CPU_t* cpu, uint16_t v1, uint16_t v2, uint16_t v3) {

	uint32_t	dst;

	dst = (uint32_t)v1 + (uint32_t)v2 + (uint32_t)v3;
	flag_szp16(cpu, (uint16_t)dst);
	if ((((dst ^ v1) & (dst ^ v2)) & 0x8000) == 0x8000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if (dst & 0xFFFF0000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

FUNC_INLINE void flag_add8(CPU_t* cpu, uint8_t v1, uint8_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint16_t	dst;

	dst = (uint16_t)v1 + (uint16_t)v2;
	flag_szp8(cpu, (uint8_t)dst);
	if (dst & 0xFF00) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

FUNC_INLINE void flag_add16(CPU_t* cpu, uint16_t v1, uint16_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint32_t	dst;

	dst = (uint32_t)v1 + (uint32_t)v2;
	flag_szp16(cpu, (uint16_t)dst);
	if (dst & 0xFFFF0000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if (((dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

FUNC_INLINE void flag_sbb8(CPU_t* cpu, uint8_t v1, uint8_t v2, uint8_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint16_t	dst;

	v2 += v3;
	dst = (uint16_t)v1 - (uint16_t)v2;
	flag_szp8(cpu, (uint8_t)dst);
	if (dst & 0xFF00) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

FUNC_INLINE void flag_sbb16(CPU_t* cpu, uint16_t v1, uint16_t v2, uint16_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint32_t	dst;

	v2 += v3;
	dst = (uint32_t)v1 - (uint32_t)v2;
	flag_szp16(cpu, (uint16_t)dst);
	if (dst & 0xFFFF0000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

FUNC_INLINE void flag_sub8(CPU_t* cpu, uint8_t v1, uint8_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint16_t	dst;

	dst = (uint16_t)v1 - (uint16_t)v2;
	flag_szp8(cpu, (uint8_t)dst);
	if (dst & 0xFF00) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

FUNC_INLINE void flag_sub16(CPU_t* cpu, uint16_t v1, uint16_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint32_t	dst;

	dst = (uint32_t)v1 - (uint32_t)v2;
	flag_szp16(cpu, (uint16_t)dst);
	if (dst & 0xFFFF0000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

FUNC_INLINE void op_adc8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b + cpu->oper2b + cpu->cf;
	flag_adc8(cpu, cpu->oper1b, cpu->oper2b, cpu->cf);
}

FUNC_INLINE void op_adc16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 + cpu->oper2 + cpu->cf;
	flag_adc16(cpu, cpu->oper1, cpu->oper2, cpu->cf);
}

FUNC_INLINE void op_add8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b + cpu->oper2b;
	flag_add8(cpu, cpu->oper1b, cpu->oper2b);
}

FUNC_INLINE void op_add16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 + cpu->oper2;
	flag_add16(cpu, cpu->oper1, cpu->oper2);
}

FUNC_INLINE void op_and8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b & cpu->oper2b;
	flag_log8(cpu, cpu->res8);
}

FUNC_INLINE void op_and16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 & cpu->oper2;
	flag_log16(cpu, cpu->res16);
}

FUNC_INLINE void op_or8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b | cpu->oper2b;
	flag_log8(cpu, cpu->res8);
}

FUNC_INLINE void op_or16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 | cpu->oper2;
	flag_log16(cpu, cpu->res16);
}

FUNC_INLINE void op_xor8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b ^ cpu->oper2b;
	flag_log8(cpu, cpu->res8);
}

FUNC_INLINE void op_xor16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 ^ cpu->oper2;
	flag_log16(cpu, cpu->res16);
}

FUNC_INLINE void op_sub8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b - cpu->oper2b;
	flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
}

FUNC_INLINE void op_sub16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 - cpu->oper2;
	flag_sub16(cpu, cpu->oper1, cpu->oper2);
}

FUNC_INLINE void op_sbb8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b - (cpu->oper2b + cpu->cf);
	flag_sbb8(cpu, cpu->oper1b, cpu->oper2b, cpu->cf);
}

FUNC_INLINE void op_sbb16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 - (cpu->oper2 + cpu->cf);
	flag_sbb16(cpu, cpu->oper1, cpu->oper2, cpu->cf);
}

FUNC_INLINE void getea(CPU_t* cpu, uint8_t rmval) {
	uint32_t	tempea;

	tempea = 0;
	switch (cpu->mode) {
	case 0:
		switch (rmval) {
		case 0:
			tempea = cpu->regs.wordregs[regbx] + cpu->regs.wordregs[regsi];
			break;
		case 1:
			tempea = cpu->regs.wordregs[regbx] + cpu->regs.wordregs[regdi];
			break;
		case 2:
			tempea = cpu->regs.wordregs[regbp] + cpu->regs.wordregs[regsi];
			break;
		case 3:
			tempea = cpu->regs.wordregs[regbp] + cpu->regs.wordregs[regdi];
			break;
		case 4:
			tempea = cpu->regs.wordregs[regsi];
			break;
		case 5:
			tempea = cpu->regs.wordregs[regdi];
			break;
		case 6:
			tempea = cpu->disp16;
			break;
		case 7:
			tempea = cpu->regs.wordregs[regbx];
			break;
		}
		break;

	case 1:
	case 2:
		switch (rmval) {
		case 0:
			tempea = cpu->regs.wordregs[regbx] + cpu->regs.wordregs[regsi] + cpu->disp16;
			break;
		case 1:
			tempea = cpu->regs.wordregs[regbx] + cpu->regs.wordregs[regdi] + cpu->disp16;
			break;
		case 2:
			tempea = cpu->regs.wordregs[regbp] + cpu->regs.wordregs[regsi] + cpu->disp16;
			break;
		case 3:
			tempea = cpu->regs.wordregs[regbp] + cpu->regs.wordregs[regdi] + cpu->disp16;
			break;
		case 4:
			tempea = cpu->regs.wordregs[regsi] + cpu->disp16;
			break;
		case 5:
			tempea = cpu->regs.wordregs[regdi] + cpu->disp16;
			break;
		case 6:
			tempea = cpu->regs.wordregs[regbp] + cpu->disp16;
			break;
		case 7:
			tempea = cpu->regs.wordregs[regbx] + cpu->disp16;
			break;
		}
		break;
	}

	cpu->ea = (tempea & 0xFFFF) + (cpu->useseg << 4);
}

FUNC_INLINE void push(CPU_t* cpu, uint16_t pushval) {
	cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regsp] - 2;
	putmem16(cpu, cpu->segregs[regss], cpu->regs.wordregs[regsp], pushval);
}

FUNC_INLINE uint16_t pop(CPU_t* cpu) {

	uint16_t	tempval;

	tempval = getmem16(cpu, cpu->segregs[regss], cpu->regs.wordregs[regsp]);
	cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regsp] + 2;
	return tempval;
}

void cpu_reset(CPU_t* cpu) {
	uint16_t i;
	for (i = 0; i < 256; i++) {
		cpu->int_callback[i] = NULL;
	}
	cpu->segregs[regcs] = 0xFFFF;
	cpu->ip = 0x0000;
	cpu->hltstate = 0;
	cpu->trap_toggle = 0;
}

FUNC_INLINE uint16_t readrm16(CPU_t* cpu, uint8_t rmval) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		return cpu_read(cpu, cpu->ea) | ((uint16_t)cpu_read(cpu, cpu->ea + 1) << 8);
	}
	else {
		return getreg16(cpu, rmval);
	}
}

FUNC_INLINE uint8_t readrm8(CPU_t* cpu, uint8_t rmval) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		return cpu_read(cpu, cpu->ea);
	}
	else {
		return getreg8(cpu, rmval);
	}
}

FUNC_INLINE void writerm16(CPU_t* cpu, uint8_t rmval, uint16_t value) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		cpu_write(cpu, cpu->ea, value & 0xFF);
		cpu_write(cpu, cpu->ea + 1, value >> 8);
	}
	else {
		putreg16(cpu, rmval, value);
	}
}

FUNC_INLINE void writerm8(CPU_t* cpu, uint8_t rmval, uint8_t value) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		cpu_write(cpu, cpu->ea, value);
	}
	else {
		putreg8(cpu, rmval, value);
	}
}

FUNC_INLINE uint8_t op_grp2_8(CPU_t* cpu, uint8_t cnt) {

	uint16_t	s;
	uint16_t	shift;
	uint16_t	oldcf;
	uint16_t	msb;

	s = cpu->oper1b;
	oldcf = cpu->cf;
#ifdef CPU_LIMIT_SHIFT_COUNT
	cnt &= 0x1F;
#endif
	switch (cpu->reg) {
	case 0: /* ROL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | cpu->cf;
		}

		if (cnt == 1) {
			//cpu->of = cpu->cf ^ ( (s >> 7) & 1);
			if ((s & 0x80) && cpu->cf) cpu->of = 1; else cpu->of = 0;
		}
		else cpu->of = 0;
		break;

	case 1: /* ROR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = (s >> 1) | (cpu->cf << 7);
		}

		if (cnt == 1) {
			cpu->of = (s >> 7) ^ ((s >> 6) & 1);
		}
		break;

	case 2: /* RCL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			if (s & 0x80) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | oldcf;
		}

		if (cnt == 1) {
			cpu->of = cpu->cf ^ ((s >> 7) & 1);
		}
		break;

	case 3: /* RCR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			cpu->cf = s & 1;
			s = (s >> 1) | (oldcf << 7);
		}

		if (cnt == 1) {
			cpu->of = (s >> 7) ^ ((s >> 6) & 1);
		}
		break;

	case 4:
	case 6: /* SHL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = (s << 1) & 0xFF;
		}

		if ((cnt == 1) && (cpu->cf == (s >> 7))) {
			cpu->of = 0;
		}
		else {
			cpu->of = 1;
		}

		flag_szp8(cpu, (uint8_t)s);
		break;

	case 5: /* SHR r/m8 */
		if ((cnt == 1) && (s & 0x80)) {
			cpu->of = 1;
		}
		else {
			cpu->of = 0;
		}

		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = s >> 1;
		}

		flag_szp8(cpu, (uint8_t)s);
		break;

	case 7: /* SAR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			msb = s & 0x80;
			cpu->cf = s & 1;
			s = (s >> 1) | msb;
		}

		cpu->of = 0;
		flag_szp8(cpu, (uint8_t)s);
		break;
	}

	return s & 0xFF;
}

FUNC_INLINE uint16_t op_grp2_16(CPU_t* cpu, uint8_t cnt) {

	uint32_t	s;
	uint32_t	shift;
	uint32_t	oldcf;
	uint32_t	msb;

	s = cpu->oper1;
	oldcf = cpu->cf;
#ifdef CPU_LIMIT_SHIFT_COUNT
	cnt &= 0x1F;
#endif
	switch (cpu->reg) {
	case 0: /* ROL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | cpu->cf;
		}

		if (cnt == 1) {
			cpu->of = cpu->cf ^ ((s >> 15) & 1);
		}
		break;

	case 1: /* ROR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = (s >> 1) | (cpu->cf << 15);
		}

		if (cnt == 1) {
			cpu->of = (s >> 15) ^ ((s >> 14) & 1);
		}
		break;

	case 2: /* RCL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			if (s & 0x8000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | oldcf;
		}

		if (cnt == 1) {
			cpu->of = cpu->cf ^ ((s >> 15) & 1);
		}
		break;

	case 3: /* RCR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			cpu->cf = s & 1;
			s = (s >> 1) | (oldcf << 15);
		}

		if (cnt == 1) {
			cpu->of = (s >> 15) ^ ((s >> 14) & 1);
		}
		break;

	case 4:
	case 6: /* SHL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = (s << 1) & 0xFFFF;
		}

		if ((cnt == 1) && (cpu->cf == (s >> 15))) {
			cpu->of = 0;
		}
		else {
			cpu->of = 1;
		}

		flag_szp16(cpu, (uint16_t)s);
		break;

	case 5: /* SHR r/m8 */
		if ((cnt == 1) && (s & 0x8000)) {
			cpu->of = 1;
		}
		else {
			cpu->of = 0;
		}

		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = s >> 1;
		}

		flag_szp16(cpu, (uint16_t)s);
		break;

	case 7: /* SAR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			msb = s & 0x8000;
			cpu->cf = s & 1;
			s = (s >> 1) | msb;
		}

		cpu->of = 0;
		flag_szp16(cpu, (uint16_t)s);
		break;
	}

	return (uint16_t)s & 0xFFFF;
}

FUNC_INLINE void op_div8(CPU_t* cpu, uint16_t valdiv, uint8_t divisor) {
	if (divisor == 0) {
		cpu_intcall(cpu, 0);
		return;
	}

	if ((valdiv / (uint16_t)divisor) > 0xFF) {
		cpu_intcall(cpu, 0);
		return;
	}

	cpu->regs.byteregs[regah] = valdiv % (uint16_t)divisor;
	cpu->regs.byteregs[regal] = valdiv / (uint16_t)divisor;
}

FUNC_INLINE void op_idiv8(CPU_t* cpu, uint16_t valdiv, uint8_t divisor) {
	//TODO: Rewrite IDIV code, I wrote this in 2011. It can be made far more efficient.
	uint16_t	s1;
	uint16_t	s2;
	uint16_t	d1;
	uint16_t	d2;
	int	sign;

	if (divisor == 0) {
		cpu_intcall(cpu, 0);
		return;
	}

	s1 = valdiv;
	s2 = divisor;
	sign = (((s1 ^ s2) & 0x8000) != 0);
	s1 = (s1 < 0x8000) ? s1 : ((~s1 + 1) & 0xffff);
	s2 = (s2 < 0x8000) ? s2 : ((~s2 + 1) & 0xffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFF00) {
		cpu_intcall(cpu, 0);
		return;
	}

	if (sign) {
		d1 = (~d1 + 1) & 0xff;
		d2 = (~d2 + 1) & 0xff;
	}

	cpu->regs.byteregs[regah] = (uint8_t)d2;
	cpu->regs.byteregs[regal] = (uint8_t)d1;
}

FUNC_INLINE void op_grp3_8(CPU_t* cpu) {
	cpu->oper1 = signext(cpu->oper1b);
	cpu->oper2 = signext(cpu->oper2b);
	switch (cpu->reg) {
	case 0:
	case 1: /* TEST */
		flag_log8(cpu, cpu->oper1b & getmem8(cpu, cpu->segregs[regcs], cpu->ip));
		StepIP(cpu, 1);
		break;

	case 2: /* NOT */
		cpu->res8 = ~cpu->oper1b;
		break;

	case 3: /* NEG */
		cpu->res8 = (~cpu->oper1b) + 1;
		flag_sub8(cpu, 0, cpu->oper1b);
		if (cpu->res8 == 0) {
			cpu->cf = 0;
		}
		else {
			cpu->cf = 1;
		}
		break;

	case 4: /* MUL */
		cpu->temp1 = (uint32_t)cpu->oper1b * (uint32_t)cpu->regs.byteregs[regal];
		cpu->regs.wordregs[regax] = cpu->temp1 & 0xFFFF;
		flag_szp8(cpu, (uint8_t)cpu->temp1);
		if (cpu->regs.byteregs[regah]) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
#ifdef CPU_CLEAR_ZF_ON_MUL
		cpu->zf = 0;
#endif
		break;

	case 5: /* IMUL */
		cpu->oper1 = signext(cpu->oper1b);
		cpu->temp1 = signext(cpu->regs.byteregs[regal]);
		cpu->temp2 = cpu->oper1;
		if ((cpu->temp1 & 0x80) == 0x80) {
			cpu->temp1 = cpu->temp1 | 0xFFFFFF00;
		}

		if ((cpu->temp2 & 0x80) == 0x80) {
			cpu->temp2 = cpu->temp2 | 0xFFFFFF00;
		}

		cpu->temp3 = (cpu->temp1 * cpu->temp2) & 0xFFFF;
		cpu->regs.wordregs[regax] = cpu->temp3 & 0xFFFF;
		if (cpu->regs.byteregs[regah]) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
#ifdef CPU_CLEAR_ZF_ON_MUL
		cpu->zf = 0;
#endif
		break;

	case 6: /* DIV */
		op_div8(cpu, cpu->regs.wordregs[regax], cpu->oper1b);
		break;

	case 7: /* IDIV */
		op_idiv8(cpu, cpu->regs.wordregs[regax], cpu->oper1b);
		break;
	}
}

FUNC_INLINE void op_div16(CPU_t* cpu, uint32_t valdiv, uint16_t divisor) {
	if (divisor == 0) {
		cpu_intcall(cpu, 0);
		return;
	}

	if ((valdiv / (uint32_t)divisor) > 0xFFFF) {
		cpu_intcall(cpu, 0);
		return;
	}

	cpu->regs.wordregs[regdx] = valdiv % (uint32_t)divisor;
	cpu->regs.wordregs[regax] = valdiv / (uint32_t)divisor;
}

FUNC_INLINE void op_idiv16(CPU_t* cpu, uint32_t valdiv, uint16_t divisor) {
	//TODO: Rewrite IDIV code, I wrote this in 2011. It can be made far more efficient.
	uint32_t	d1;
	uint32_t	d2;
	uint32_t	s1;
	uint32_t	s2;
	int	sign;

	if (divisor == 0) {
		cpu_intcall(cpu, 0);
		return;
	}

	s1 = valdiv;
	s2 = divisor;
	s2 = (s2 & 0x8000) ? (s2 | 0xffff0000) : s2;
	sign = (((s1 ^ s2) & 0x80000000) != 0);
	s1 = (s1 < 0x80000000) ? s1 : ((~s1 + 1) & 0xffffffff);
	s2 = (s2 < 0x80000000) ? s2 : ((~s2 + 1) & 0xffffffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFFFF0000) {
		cpu_intcall(cpu, 0);
		return;
	}

	if (sign) {
		d1 = (~d1 + 1) & 0xffff;
		d2 = (~d2 + 1) & 0xffff;
	}

	cpu->regs.wordregs[regax] = d1;
	cpu->regs.wordregs[regdx] = d2;
}

FUNC_INLINE void op_grp3_16(CPU_t* cpu) {
	switch (cpu->reg) {
	case 0:
	case 1: /* TEST */
		flag_log16(cpu, cpu->oper1 & getmem16(cpu, cpu->segregs[regcs], cpu->ip));
		StepIP(cpu, 2);
		break;

	case 2: /* NOT */
		cpu->res16 = ~cpu->oper1;
		break;

	case 3: /* NEG */
		cpu->res16 = (~cpu->oper1) + 1;
		flag_sub16(cpu, 0, cpu->oper1);
		if (cpu->res16) {
			cpu->cf = 1;
		}
		else {
			cpu->cf = 0;
		}
		break;

	case 4: /* MUL */
		cpu->temp1 = (uint32_t)cpu->oper1 * (uint32_t)cpu->regs.wordregs[regax];
		cpu->regs.wordregs[regax] = cpu->temp1 & 0xFFFF;
		cpu->regs.wordregs[regdx] = cpu->temp1 >> 16;
		flag_szp16(cpu, (uint16_t)cpu->temp1);
		if (cpu->regs.wordregs[regdx]) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
#ifdef CPU_CLEAR_ZF_ON_MUL
		cpu->zf = 0;
#endif
		break;

	case 5: /* IMUL */
		cpu->temp1 = cpu->regs.wordregs[regax];
		cpu->temp2 = cpu->oper1;
		if (cpu->temp1 & 0x8000) {
			cpu->temp1 |= 0xFFFF0000;
		}

		if (cpu->temp2 & 0x8000) {
			cpu->temp2 |= 0xFFFF0000;
		}

		cpu->temp3 = cpu->temp1 * cpu->temp2;
		cpu->regs.wordregs[regax] = cpu->temp3 & 0xFFFF;	/* into register ax */
		cpu->regs.wordregs[regdx] = cpu->temp3 >> 16;	/* into register dx */
		if (cpu->regs.wordregs[regdx]) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
#ifdef CPU_CLEAR_ZF_ON_MUL
		cpu->zf = 0;
#endif
		break;

	case 6: /* DIV */
		op_div16(cpu, ((uint32_t)cpu->regs.wordregs[regdx] << 16) + cpu->regs.wordregs[regax], cpu->oper1);
		break;

	case 7: /* DIV */
		op_idiv16(cpu, ((uint32_t)cpu->regs.wordregs[regdx] << 16) + cpu->regs.wordregs[regax], cpu->oper1);
		break;
	}
}

FUNC_INLINE void op_grp5(CPU_t* cpu) {
	switch (cpu->reg) {
	case 0: /* INC Ev */
		cpu->oper2 = 1;
		cpu->tempcf = cpu->cf;
		op_add16(cpu);
		cpu->cf = cpu->tempcf;
		writerm16(cpu, cpu->rm, cpu->res16);
		break;

	case 1: /* DEC Ev */
		cpu->oper2 = 1;
		cpu->tempcf = cpu->cf;
		op_sub16(cpu);
		cpu->cf = cpu->tempcf;
		writerm16(cpu, cpu->rm, cpu->res16);
		break;

	case 2: /* CALL Ev */
		push(cpu, cpu->ip);
		cpu->ip = cpu->oper1;
		break;

	case 3: /* CALL Mp */
		push(cpu, cpu->segregs[regcs]);
		push(cpu, cpu->ip);
		getea(cpu, cpu->rm);
		cpu->ip = (uint16_t)cpu_read(cpu, cpu->ea) + (uint16_t)cpu_read(cpu, cpu->ea + 1) * 256;
		cpu->segregs[regcs] = (uint16_t)cpu_read(cpu, cpu->ea + 2) + (uint16_t)cpu_read(cpu, cpu->ea + 3) * 256;
		break;

	case 4: /* JMP Ev */
		cpu->ip = cpu->oper1;
		break;

	case 5: /* JMP Mp */
		getea(cpu, cpu->rm);
		cpu->ip = (uint16_t)cpu_read(cpu, cpu->ea) + (uint16_t)cpu_read(cpu, cpu->ea + 1) * 256;
		cpu->segregs[regcs] = (uint16_t)cpu_read(cpu, cpu->ea + 2) + (uint16_t)cpu_read(cpu, cpu->ea + 3) * 256;
		break;

	case 6: /* PUSH Ev */
		push(cpu, cpu->oper1);
		break;
	}
}

FUNC_INLINE void cpu_intcall(CPU_t* cpu, uint8_t intnum) {
	if (cpu->int_callback[intnum] != NULL) {
		(*cpu->int_callback[intnum])(cpu, intnum);
		return;
	}

	push(cpu, makeflagsword(cpu));
	push(cpu, cpu->segregs[regcs]);
	push(cpu, cpu->ip);
	cpu->segregs[regcs] = getmem16(cpu, 0, (uint16_t)intnum * 4 + 2);
	cpu->ip = getmem16(cpu, 0, (uint16_t)intnum * 4);
	cpu->ifl = 0;
	cpu->tf = 0;
}

void cpu_interruptCheck(CPU_t* cpu, I8259_t* i8259) {
	/* get next interrupt from the i8259, if any */
	if (!cpu->trap_toggle && (cpu->ifl && (i8259->irr & (~i8259->imr)))) {
		cpu->hltstate = 0;
		cpu_intcall(cpu, i8259_nextintr(i8259));
	}
}

void cpu_exec(CPU_t* cpu, uint32_t execloops) {

	uint32_t loopcount;
	uint8_t docontinue;
	static uint16_t firstip;

	for (loopcount = 0; loopcount < execloops; loopcount++) {

		if (cpu->trap_toggle) {
			cpu_intcall(cpu, 1);
		}

		if (cpu->tf) {
			cpu->trap_toggle = 1;
		}
		else {
			cpu->trap_toggle = 0;
		}

		if (cpu->hltstate) goto skipexecution;

		cpu->reptype = 0;
		cpu->segoverride = 0;
		cpu->useseg = cpu->segregs[regds];
		docontinue = 0;
		firstip = cpu->ip;

		while (!docontinue) {
			cpu->segregs[regcs] = cpu->segregs[regcs] & 0xFFFF;
			cpu->ip = cpu->ip & 0xFFFF;
			cpu->savecs = cpu->segregs[regcs];
			cpu->saveip = cpu->ip;
			cpu->opcode = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);

			switch (cpu->opcode) {
				/* segment prefix check */
			case 0x2E:	/* segment cpu->segregs[regcs] */
				cpu->useseg = cpu->segregs[regcs];
				cpu->segoverride = 1;
				break;

			case 0x3E:	/* segment cpu->segregs[regds] */
				cpu->useseg = cpu->segregs[regds];
				cpu->segoverride = 1;
				break;

			case 0x26:	/* segment cpu->segregs[reges] */
				cpu->useseg = cpu->segregs[reges];
				cpu->segoverride = 1;
				break;

			case 0x36:	/* segment cpu->segregs[regss] */
				cpu->useseg = cpu->segregs[regss];
				cpu->segoverride = 1;
				break;

				/* repetition prefix check */
			case 0xF3:	/* REP/REPE/REPZ */
				cpu->reptype = 1;
				break;

			case 0xF2:	/* REPNE/REPNZ */
				cpu->reptype = 2;
				break;

			default:
				docontinue = 1;
				break;
			}
		}

		cpu->totalexec++;

		switch (cpu->opcode) {
		case 0x0:	/* 00 ADD Eb Gb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getreg8(cpu, cpu->reg);
			op_add8(cpu);
			writerm8(cpu, cpu->rm, cpu->res8);
			break;

		case 0x1:	/* 01 ADD Ev Gv */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getreg16(cpu, cpu->reg);
			op_add16(cpu);
			writerm16(cpu, cpu->rm, cpu->res16);
			break;

		case 0x2:	/* 02 ADD Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			op_add8(cpu);
			putreg8(cpu, cpu->reg, cpu->res8);
			break;

		case 0x3:	/* 03 ADD Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			op_add16(cpu);
			putreg16(cpu, cpu->reg, cpu->res16);
			break;

		case 0x4:	/* 04 ADD cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			op_add8(cpu);
			cpu->regs.byteregs[regal] = cpu->res8;
			break;

		case 0x5:	/* 05 ADD eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			op_add16(cpu);
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0x6:	/* 06 PUSH cpu->segregs[reges] */
			push(cpu, cpu->segregs[reges]);
			break;

		case 0x7:	/* 07 POP cpu->segregs[reges] */
			cpu->segregs[reges] = pop(cpu);
			break;

		case 0x8:	/* 08 OR Eb Gb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getreg8(cpu, cpu->reg);
			op_or8(cpu);
			writerm8(cpu, cpu->rm, cpu->res8);
			break;

		case 0x9:	/* 09 OR Ev Gv */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getreg16(cpu, cpu->reg);
			op_or16(cpu);
			writerm16(cpu, cpu->rm, cpu->res16);
			break;

		case 0xA:	/* 0A OR Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			op_or8(cpu);
			putreg8(cpu, cpu->reg, cpu->res8);
			break;

		case 0xB:	/* 0B OR Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			op_or16(cpu);
			if ((cpu->oper1 == 0xF802) && (cpu->oper2 == 0xF802)) {
				cpu->sf = 0;	/* cheap hack to make Wolf 3D think we're a 286 so it plays */
			}

			putreg16(cpu, cpu->reg, cpu->res16);
			break;

		case 0xC:	/* 0C OR cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			op_or8(cpu);
			cpu->regs.byteregs[regal] = cpu->res8;
			break;

		case 0xD:	/* 0D OR eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			op_or16(cpu);
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0xE:	/* 0E PUSH cpu->segregs[regcs] */
			push(cpu, cpu->segregs[regcs]);
			break;

#ifdef CPU_ALLOW_POP_CS //only the 8086/8088 does this.
		case 0xF: //0F POP CS
			cpu->segregs[regcs] = pop(cpu);
			break;
#endif

		case 0x10:	/* 10 ADC Eb Gb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getreg8(cpu, cpu->reg);
			op_adc8(cpu);
			writerm8(cpu, cpu->rm, cpu->res8);
			break;

		case 0x11:	/* 11 ADC Ev Gv */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getreg16(cpu, cpu->reg);
			op_adc16(cpu);
			writerm16(cpu, cpu->rm, cpu->res16);
			break;

		case 0x12:	/* 12 ADC Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			op_adc8(cpu);
			putreg8(cpu, cpu->reg, cpu->res8);
			break;

		case 0x13:	/* 13 ADC Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			op_adc16(cpu);
			putreg16(cpu, cpu->reg, cpu->res16);
			break;

		case 0x14:	/* 14 ADC cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			op_adc8(cpu);
			cpu->regs.byteregs[regal] = cpu->res8;
			break;

		case 0x15:	/* 15 ADC eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			op_adc16(cpu);
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0x16:	/* 16 PUSH cpu->segregs[regss] */
			push(cpu, cpu->segregs[regss]);
			break;

		case 0x17:	/* 17 POP cpu->segregs[regss] */
			cpu->segregs[regss] = pop(cpu);
			break;

		case 0x18:	/* 18 SBB Eb Gb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getreg8(cpu, cpu->reg);
			op_sbb8(cpu);
			writerm8(cpu, cpu->rm, cpu->res8);
			break;

		case 0x19:	/* 19 SBB Ev Gv */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getreg16(cpu, cpu->reg);
			op_sbb16(cpu);
			writerm16(cpu, cpu->rm, cpu->res16);
			break;

		case 0x1A:	/* 1A SBB Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			op_sbb8(cpu);
			putreg8(cpu, cpu->reg, cpu->res8);
			break;

		case 0x1B:	/* 1B SBB Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			op_sbb16(cpu);
			putreg16(cpu, cpu->reg, cpu->res16);
			break;

		case 0x1C:	/* 1C SBB cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			op_sbb8(cpu);
			cpu->regs.byteregs[regal] = cpu->res8;
			break;

		case 0x1D:	/* 1D SBB eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			op_sbb16(cpu);
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0x1E:	/* 1E PUSH cpu->segregs[regds] */
			push(cpu, cpu->segregs[regds]);
			break;

		case 0x1F:	/* 1F POP cpu->segregs[regds] */
			cpu->segregs[regds] = pop(cpu);
			break;

		case 0x20:	/* 20 AND Eb Gb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getreg8(cpu, cpu->reg);
			op_and8(cpu);
			writerm8(cpu, cpu->rm, cpu->res8);
			break;

		case 0x21:	/* 21 AND Ev Gv */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getreg16(cpu, cpu->reg);
			op_and16(cpu);
			writerm16(cpu, cpu->rm, cpu->res16);
			break;

		case 0x22:	/* 22 AND Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			op_and8(cpu);
			putreg8(cpu, cpu->reg, cpu->res8);
			break;

		case 0x23:	/* 23 AND Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			op_and16(cpu);
			putreg16(cpu, cpu->reg, cpu->res16);
			break;

		case 0x24:	/* 24 AND cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			op_and8(cpu);
			cpu->regs.byteregs[regal] = cpu->res8;
			break;

		case 0x25:	/* 25 AND eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			op_and16(cpu);
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0x27:	/* 27 DAA */
		{
			uint8_t old_al;
			old_al = cpu->regs.byteregs[regal];
			if (((cpu->regs.byteregs[regal] & 0x0F) > 9) || cpu->af) {
				cpu->oper1 = (uint16_t)cpu->regs.byteregs[regal] + 0x06;
				cpu->regs.byteregs[regal] = cpu->oper1 & 0xFF;
				if (cpu->oper1 & 0xFF00) cpu->cf = 1;
				if ((cpu->oper1 & 0x000F) < (old_al & 0x0F)) cpu->af = 1;
			}
			if (((cpu->regs.byteregs[regal] & 0xF0) > 0x90) || cpu->cf) {
				cpu->oper1 = (uint16_t)cpu->regs.byteregs[regal] + 0x60;
				cpu->regs.byteregs[regal] = cpu->oper1 & 0xFF;
				if (cpu->oper1 & 0xFF00) cpu->cf = 1; else cpu->cf = 0;
			}
			flag_szp8(cpu, cpu->regs.byteregs[regal]);
			break;
		}

		case 0x28:	/* 28 SUB Eb Gb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getreg8(cpu, cpu->reg);
			op_sub8(cpu);
			writerm8(cpu, cpu->rm, cpu->res8);
			break;

		case 0x29:	/* 29 SUB Ev Gv */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getreg16(cpu, cpu->reg);
			op_sub16(cpu);
			writerm16(cpu, cpu->rm, cpu->res16);
			break;

		case 0x2A:	/* 2A SUB Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			op_sub8(cpu);
			putreg8(cpu, cpu->reg, cpu->res8);
			break;

		case 0x2B:	/* 2B SUB Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			op_sub16(cpu);
			putreg16(cpu, cpu->reg, cpu->res16);
			break;

		case 0x2C:	/* 2C SUB cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			op_sub8(cpu);
			cpu->regs.byteregs[regal] = cpu->res8;
			break;

		case 0x2D:	/* 2D SUB eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			op_sub16(cpu);
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0x2F:	/* 2F DAS */
		{
			uint8_t old_al;
			old_al = cpu->regs.byteregs[regal];
			if (((cpu->regs.byteregs[regal] & 0x0F) > 9) || cpu->af) {
				cpu->oper1 = (uint16_t)cpu->regs.byteregs[regal] - 0x06;
				cpu->regs.byteregs[regal] = cpu->oper1 & 0xFF;
				if (cpu->oper1 & 0xFF00) cpu->cf = 1;
				if ((cpu->oper1 & 0x000F) >= (old_al & 0x0F)) cpu->af = 1;
			}
			if (((cpu->regs.byteregs[regal] & 0xF0) > 0x90) || cpu->cf) {
				cpu->oper1 = (uint16_t)cpu->regs.byteregs[regal] - 0x60;
				cpu->regs.byteregs[regal] = cpu->oper1 & 0xFF;
				if (cpu->oper1 & 0xFF00) cpu->cf = 1; else cpu->cf = 0;
			}
			flag_szp8(cpu, cpu->regs.byteregs[regal]);
			break;
		}

		case 0x30:	/* 30 XOR Eb Gb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getreg8(cpu, cpu->reg);
			op_xor8(cpu);
			writerm8(cpu, cpu->rm, cpu->res8);
			break;

		case 0x31:	/* 31 XOR Ev Gv */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getreg16(cpu, cpu->reg);
			op_xor16(cpu);
			writerm16(cpu, cpu->rm, cpu->res16);
			break;

		case 0x32:	/* 32 XOR Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			op_xor8(cpu);
			putreg8(cpu, cpu->reg, cpu->res8);
			break;

		case 0x33:	/* 33 XOR Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			op_xor16(cpu);
			putreg16(cpu, cpu->reg, cpu->res16);
			break;

		case 0x34:	/* 34 XOR cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			op_xor8(cpu);
			cpu->regs.byteregs[regal] = cpu->res8;
			break;

		case 0x35:	/* 35 XOR eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			op_xor16(cpu);
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0x37:	/* 37 AAA ASCII */
			if (((cpu->regs.byteregs[regal] & 0xF) > 9) || (cpu->af == 1)) {
				cpu->regs.wordregs[regax] = cpu->regs.wordregs[regax] + 0x106;
				cpu->af = 1;
				cpu->cf = 1;
			}
			else {
				cpu->af = 0;
				cpu->cf = 0;
			}

			cpu->regs.byteregs[regal] = cpu->regs.byteregs[regal] & 0xF;
			break;

		case 0x38:	/* 38 CMP Eb Gb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getreg8(cpu, cpu->reg);
			flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
			break;

		case 0x39:	/* 39 CMP Ev Gv */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getreg16(cpu, cpu->reg);
			flag_sub16(cpu, cpu->oper1, cpu->oper2);
			break;

		case 0x3A:	/* 3A CMP Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
			break;

		case 0x3B:	/* 3B CMP Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			flag_sub16(cpu, cpu->oper1, cpu->oper2);
			break;

		case 0x3C:	/* 3C CMP cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
			break;

		case 0x3D:	/* 3D CMP eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			flag_sub16(cpu, cpu->oper1, cpu->oper2);
			break;

		case 0x3F:	/* 3F AAS ASCII */
			if (((cpu->regs.byteregs[regal] & 0xF) > 9) || (cpu->af == 1)) {
				cpu->regs.wordregs[regax] = cpu->regs.wordregs[regax] - 6;
				cpu->regs.byteregs[regah] = cpu->regs.byteregs[regah] - 1;
				cpu->af = 1;
				cpu->cf = 1;
			}
			else {
				cpu->af = 0;
				cpu->cf = 0;
			}

			cpu->regs.byteregs[regal] = cpu->regs.byteregs[regal] & 0xF;
			break;

		case 0x40:	/* 40 INC eAX */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = 1;
			op_add16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0x41:	/* 41 INC eCX */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regcx];
			cpu->oper2 = 1;
			op_add16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regcx] = cpu->res16;
			break;

		case 0x42:	/* 42 INC eDX */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regdx];
			cpu->oper2 = 1;
			op_add16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regdx] = cpu->res16;
			break;

		case 0x43:	/* 43 INC eBX */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regbx];
			cpu->oper2 = 1;
			op_add16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regbx] = cpu->res16;
			break;

		case 0x44:	/* 44 INC eSP */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regsp];
			cpu->oper2 = 1;
			op_add16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regsp] = cpu->res16;
			break;

		case 0x45:	/* 45 INC eBP */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regbp];
			cpu->oper2 = 1;
			op_add16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regbp] = cpu->res16;
			break;

		case 0x46:	/* 46 INC eSI */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regsi];
			cpu->oper2 = 1;
			op_add16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regsi] = cpu->res16;
			break;

		case 0x47:	/* 47 INC eDI */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regdi];
			cpu->oper2 = 1;
			op_add16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regdi] = cpu->res16;
			break;

		case 0x48:	/* 48 DEC eAX */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = 1;
			op_sub16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regax] = cpu->res16;
			break;

		case 0x49:	/* 49 DEC eCX */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regcx];
			cpu->oper2 = 1;
			op_sub16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regcx] = cpu->res16;
			break;

		case 0x4A:	/* 4A DEC eDX */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regdx];
			cpu->oper2 = 1;
			op_sub16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regdx] = cpu->res16;
			break;

		case 0x4B:	/* 4B DEC eBX */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regbx];
			cpu->oper2 = 1;
			op_sub16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regbx] = cpu->res16;
			break;

		case 0x4C:	/* 4C DEC eSP */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regsp];
			cpu->oper2 = 1;
			op_sub16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regsp] = cpu->res16;
			break;

		case 0x4D:	/* 4D DEC eBP */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regbp];
			cpu->oper2 = 1;
			op_sub16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regbp] = cpu->res16;
			break;

		case 0x4E:	/* 4E DEC eSI */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regsi];
			cpu->oper2 = 1;
			op_sub16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regsi] = cpu->res16;
			break;

		case 0x4F:	/* 4F DEC eDI */
			cpu->oldcf = cpu->cf;
			cpu->oper1 = cpu->regs.wordregs[regdi];
			cpu->oper2 = 1;
			op_sub16(cpu);
			cpu->cf = cpu->oldcf;
			cpu->regs.wordregs[regdi] = cpu->res16;
			break;

		case 0x50:	/* 50 PUSH eAX */
			push(cpu, cpu->regs.wordregs[regax]);
			break;

		case 0x51:	/* 51 PUSH eCX */
			push(cpu, cpu->regs.wordregs[regcx]);
			break;

		case 0x52:	/* 52 PUSH eDX */
			push(cpu, cpu->regs.wordregs[regdx]);
			break;

		case 0x53:	/* 53 PUSH eBX */
			push(cpu, cpu->regs.wordregs[regbx]);
			break;

		case 0x54:	/* 54 PUSH eSP */
#ifdef USE_286_STYLE_PUSH_SP
			push(cpu, cpu->regs.wordregs[regsp]);
#else
			push(cpu, cpu->regs.wordregs[regsp] - 2);
#endif
			break;

		case 0x55:	/* 55 PUSH eBP */
			push(cpu, cpu->regs.wordregs[regbp]);
			break;

		case 0x56:	/* 56 PUSH eSI */
			push(cpu, cpu->regs.wordregs[regsi]);
			break;

		case 0x57:	/* 57 PUSH eDI */
			push(cpu, cpu->regs.wordregs[regdi]);
			break;

		case 0x58:	/* 58 POP eAX */
			cpu->regs.wordregs[regax] = pop(cpu);
			break;

		case 0x59:	/* 59 POP eCX */
			cpu->regs.wordregs[regcx] = pop(cpu);
			break;

		case 0x5A:	/* 5A POP eDX */
			cpu->regs.wordregs[regdx] = pop(cpu);
			break;

		case 0x5B:	/* 5B POP eBX */
			cpu->regs.wordregs[regbx] = pop(cpu);
			break;

		case 0x5C:	/* 5C POP eSP */
			cpu->regs.wordregs[regsp] = pop(cpu);
			break;

		case 0x5D:	/* 5D POP eBP */
			cpu->regs.wordregs[regbp] = pop(cpu);
			break;

		case 0x5E:	/* 5E POP eSI */
			cpu->regs.wordregs[regsi] = pop(cpu);
			break;

		case 0x5F:	/* 5F POP eDI */
			cpu->regs.wordregs[regdi] = pop(cpu);
			break;

#ifndef CPU_8086
		case 0x60:	/* 60 PUSHA (80186+) */
			cpu->oldsp = cpu->regs.wordregs[regsp];
			push(cpu, cpu->regs.wordregs[regax]);
			push(cpu, cpu->regs.wordregs[regcx]);
			push(cpu, cpu->regs.wordregs[regdx]);
			push(cpu, cpu->regs.wordregs[regbx]);
			push(cpu, cpu->oldsp);
			push(cpu, cpu->regs.wordregs[regbp]);
			push(cpu, cpu->regs.wordregs[regsi]);
			push(cpu, cpu->regs.wordregs[regdi]);
			break;

		case 0x61:	/* 61 POPA (80186+) */
			cpu->regs.wordregs[regdi] = pop(cpu);
			cpu->regs.wordregs[regsi] = pop(cpu);
			cpu->regs.wordregs[regbp] = pop(cpu);
			cpu->regs.wordregs[regsp] += 2;
			cpu->regs.wordregs[regbx] = pop(cpu);
			cpu->regs.wordregs[regdx] = pop(cpu);
			cpu->regs.wordregs[regcx] = pop(cpu);
			cpu->regs.wordregs[regax] = pop(cpu);
			break;

		case 0x62: /* 62 BOUND Gv, Ev (80186+) */
			modregrm(cpu);
			getea(cpu, cpu->rm);
			if (signext32(getreg16(cpu, cpu->reg)) < signext32(getmem16(cpu, cpu->ea >> 4, cpu->ea & 15))) {
				cpu_intcall(cpu, 5); //bounds check exception
			}
			else {
				cpu->ea += 2;
				if (signext32(getreg16(cpu, cpu->reg)) > signext32(getmem16(cpu, cpu->ea >> 4, cpu->ea & 15))) {
					cpu_intcall(cpu, 5); //bounds check exception
				}
			}
			break;

		case 0x68:	/* 68 PUSH Iv (80186+) */
			push(cpu, getmem16(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 2);
			break;

		case 0x69:	/* 69 IMUL Gv Ev Iv (80186+) */
			modregrm(cpu);
			cpu->temp1 = readrm16(cpu, cpu->rm);
			cpu->temp2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			if ((cpu->temp1 & 0x8000L) == 0x8000L) {
				cpu->temp1 = cpu->temp1 | 0xFFFF0000L;
			}

			if ((cpu->temp2 & 0x8000L) == 0x8000L) {
				cpu->temp2 = cpu->temp2 | 0xFFFF0000L;
			}

			cpu->temp3 = cpu->temp1 * cpu->temp2;
			putreg16(cpu, cpu->reg, cpu->temp3 & 0xFFFFL);
			if (cpu->temp3 & 0xFFFF0000L) {
				cpu->cf = 1;
				cpu->of = 1;
			}
			else {
				cpu->cf = 0;
				cpu->of = 0;
			}
			break;

		case 0x6A:	/* 6A PUSH Ib (80186+) */
			push(cpu, (uint16_t)signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip)));
			StepIP(cpu, 1);
			break;

		case 0x6B:	/* 6B IMUL Gv Eb Ib (80186+) */
			modregrm(cpu);
			cpu->temp1 = readrm16(cpu, cpu->rm);
			cpu->temp2 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if ((cpu->temp1 & 0x8000L) == 0x8000L) {
				cpu->temp1 = cpu->temp1 | 0xFFFF0000L;
			}

			if ((cpu->temp2 & 0x8000L) == 0x8000L) {
				cpu->temp2 = cpu->temp2 | 0xFFFF0000L;
			}

			cpu->temp3 = cpu->temp1 * cpu->temp2;
			putreg16(cpu, cpu->reg, cpu->temp3 & 0xFFFFL);
			if (cpu->temp3 & 0xFFFF0000L) {
				cpu->cf = 1;
				cpu->of = 1;
			}
			else {
				cpu->cf = 0;
				cpu->of = 0;
			}
			break;

		case 0x6C:	/* 6E INSB */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			putmem8(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi], port_read(cpu, cpu->regs.wordregs[regdx]));
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 1;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 1;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 1;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 1;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0x6D:	/* 6F INSW */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			putmem16(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi], port_readw(cpu, cpu->regs.wordregs[regdx]));
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 2;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 2;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 2;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 2;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0x6E:	/* 6E OUTSB */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			port_write(cpu, cpu->regs.wordregs[regdx], getmem8(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 1;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 1;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 1;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 1;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0x6F:	/* 6F OUTSW */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			port_writew(cpu, cpu->regs.wordregs[regdx], getmem16(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 2;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 2;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 2;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 2;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;
#endif

		case 0x70:	/* 70 JO Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (cpu->of) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x71:	/* 71 JNO Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (!cpu->of) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x72:	/* 72 JB Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (cpu->cf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x73:	/* 73 JNB Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (!cpu->cf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x74:	/* 74 JZ Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (cpu->zf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x75:	/* 75 JNZ Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (!cpu->zf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x76:	/* 76 JBE Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (cpu->cf || cpu->zf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x77:	/* 77 JA Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (!cpu->cf && !cpu->zf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x78:	/* 78 JS Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (cpu->sf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x79:	/* 79 JNS Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (!cpu->sf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x7A:	/* 7A JPE Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (cpu->pf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x7B:	/* 7B JPO Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (!cpu->pf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x7C:	/* 7C JL Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (cpu->sf != cpu->of) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x7D:	/* 7D JGE Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (cpu->sf == cpu->of) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x7E:	/* 7E JLE Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if ((cpu->sf != cpu->of) || cpu->zf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x7F:	/* 7F JG Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (!cpu->zf && (cpu->sf == cpu->of)) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0x80:
		case 0x82:	/* 80/82 GRP1 Eb Ib */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			switch (cpu->reg) {
			case 0:
				op_add8(cpu);
				break;
			case 1:
				op_or8(cpu);
				break;
			case 2:
				op_adc8(cpu);
				break;
			case 3:
				op_sbb8(cpu);
				break;
			case 4:
				op_and8(cpu);
				break;
			case 5:
				op_sub8(cpu);
				break;
			case 6:
				op_xor8(cpu);
				break;
			case 7:
				flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
				break;
			default:
				break;	/* to avoid compiler warnings */
			}

			if (cpu->reg < 7) {
				writerm8(cpu, cpu->rm, cpu->res8);
			}
			break;

		case 0x81:	/* 81 GRP1 Ev Iv */
		case 0x83:	/* 83 GRP1 Ev Ib */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			if (cpu->opcode == 0x81) {
				cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
				StepIP(cpu, 2);
			}
			else {
				cpu->oper2 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
				StepIP(cpu, 1);
			}

			switch (cpu->reg) {
			case 0:
				op_add16(cpu);
				break;
			case 1:
				op_or16(cpu);
				break;
			case 2:
				op_adc16(cpu);
				break;
			case 3:
				op_sbb16(cpu);
				break;
			case 4:
				op_and16(cpu);
				break;
			case 5:
				op_sub16(cpu);
				break;
			case 6:
				op_xor16(cpu);
				break;
			case 7:
				flag_sub16(cpu, cpu->oper1, cpu->oper2);
				break;
			default:
				break;	/* to avoid compiler warnings */
			}

			if (cpu->reg < 7) {
				writerm16(cpu, cpu->rm, cpu->res16);
			}
			break;

		case 0x84:	/* 84 TEST Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			cpu->oper2b = readrm8(cpu, cpu->rm);
			flag_log8(cpu, cpu->oper1b & cpu->oper2b);
			break;

		case 0x85:	/* 85 TEST Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			cpu->oper2 = readrm16(cpu, cpu->rm);
			flag_log16(cpu, cpu->oper1 & cpu->oper2);
			break;

		case 0x86:	/* 86 XCHG Gb Eb */
			modregrm(cpu);
			cpu->oper1b = getreg8(cpu, cpu->reg);
			putreg8(cpu, cpu->reg, readrm8(cpu, cpu->rm));
			writerm8(cpu, cpu->rm, cpu->oper1b);
			break;

		case 0x87:	/* 87 XCHG Gv Ev */
			modregrm(cpu);
			cpu->oper1 = getreg16(cpu, cpu->reg);
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
			writerm16(cpu, cpu->rm, cpu->oper1);
			break;

		case 0x88:	/* 88 MOV Eb Gb */
			modregrm(cpu);
			writerm8(cpu, cpu->rm, getreg8(cpu, cpu->reg));
			break;

		case 0x89:	/* 89 MOV Ev Gv */
			modregrm(cpu);
			writerm16(cpu, cpu->rm, getreg16(cpu, cpu->reg));
			break;

		case 0x8A:	/* 8A MOV Gb Eb */
			modregrm(cpu);
			putreg8(cpu, cpu->reg, readrm8(cpu, cpu->rm));
			break;

		case 0x8B:	/* 8B MOV Gv Ev */
			modregrm(cpu);
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
			break;

		case 0x8C:	/* 8C MOV Ew Sw */
			modregrm(cpu);
			writerm16(cpu, cpu->rm, getsegreg(cpu, cpu->reg));
			break;

		case 0x8D:	/* 8D LEA Gv M */
			modregrm(cpu);
			getea(cpu, cpu->rm);
			putreg16(cpu, cpu->reg, cpu->ea - segbase(cpu->useseg));
			break;

		case 0x8E:	/* 8E MOV Sw Ew */
			modregrm(cpu);
			putsegreg(cpu, cpu->reg, readrm16(cpu, cpu->rm));
			break;

		case 0x8F:	/* 8F POP Ev */
			modregrm(cpu);
			writerm16(cpu, cpu->rm, pop(cpu));
			break;

		case 0x90:	/* 90 NOP */
			break;

		case 0x91:	/* 91 XCHG eCX eAX */
			cpu->oper1 = cpu->regs.wordregs[regcx];
			cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regax];
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0x92:	/* 92 XCHG eDX eAX */
			cpu->oper1 = cpu->regs.wordregs[regdx];
			cpu->regs.wordregs[regdx] = cpu->regs.wordregs[regax];
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0x93:	/* 93 XCHG eBX eAX */
			cpu->oper1 = cpu->regs.wordregs[regbx];
			cpu->regs.wordregs[regbx] = cpu->regs.wordregs[regax];
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0x94:	/* 94 XCHG eSP eAX */
			cpu->oper1 = cpu->regs.wordregs[regsp];
			cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regax];
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0x95:	/* 95 XCHG eBP eAX */
			cpu->oper1 = cpu->regs.wordregs[regbp];
			cpu->regs.wordregs[regbp] = cpu->regs.wordregs[regax];
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0x96:	/* 96 XCHG eSI eAX */
			cpu->oper1 = cpu->regs.wordregs[regsi];
			cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regax];
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0x97:	/* 97 XCHG eDI eAX */
			cpu->oper1 = cpu->regs.wordregs[regdi];
			cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regax];
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0x98:	/* 98 CBW */
			if ((cpu->regs.byteregs[regal] & 0x80) == 0x80) {
				cpu->regs.byteregs[regah] = 0xFF;
			}
			else {
				cpu->regs.byteregs[regah] = 0;
			}
			break;

		case 0x99:	/* 99 CWD */
			if ((cpu->regs.byteregs[regah] & 0x80) == 0x80) {
				cpu->regs.wordregs[regdx] = 0xFFFF;
			}
			else {
				cpu->regs.wordregs[regdx] = 0;
			}
			break;

		case 0x9A:	/* 9A CALL Ap */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			push(cpu, cpu->segregs[regcs]);
			push(cpu, cpu->ip);
			cpu->ip = cpu->oper1;
			cpu->segregs[regcs] = cpu->oper2;
			break;

		case 0x9B:	/* 9B WAIT */
			break;

		case 0x9C:	/* 9C PUSHF */
#ifdef CPU_SET_HIGH_FLAGS
			push(cpu, makeflagsword(cpu) | 0xF800);
#else
			push(cpu, makeflagsword(cpu) | 0x0800);
#endif
			break;

		case 0x9D:	/* 9D POPF */
			cpu->temp16 = pop(cpu);
			decodeflagsword(cpu, cpu->temp16);
			break;

		case 0x9E:	/* 9E SAHF */
			decodeflagsword(cpu, (makeflagsword(cpu) & 0xFF00) | cpu->regs.byteregs[regah]);
			break;

		case 0x9F:	/* 9F LAHF */
			cpu->regs.byteregs[regah] = makeflagsword(cpu) & 0xFF;
			break;

		case 0xA0:	/* A0 MOV cpu->regs.byteregs[regal] Ob */
			cpu->regs.byteregs[regal] = getmem8(cpu, cpu->useseg, getmem16(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 2);
			break;

		case 0xA1:	/* A1 MOV eAX Ov */
			cpu->oper1 = getmem16(cpu, cpu->useseg, getmem16(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 2);
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0xA2:	/* A2 MOV Ob cpu->regs.byteregs[regal] */
			putmem8(cpu, cpu->useseg, getmem16(cpu, cpu->segregs[regcs], cpu->ip), cpu->regs.byteregs[regal]);
			StepIP(cpu, 2);
			break;

		case 0xA3:	/* A3 MOV Ov eAX */
			putmem16(cpu, cpu->useseg, getmem16(cpu, cpu->segregs[regcs], cpu->ip), cpu->regs.wordregs[regax]);
			StepIP(cpu, 2);
			break;

		case 0xA4:	/* A4 MOVSB */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			putmem8(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi], getmem8(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 1;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 1;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 1;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 1;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xA5:	/* A5 MOVSW */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			putmem16(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi], getmem16(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 2;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 2;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 2;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 2;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xA6:	/* A6 CMPSB */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			cpu->oper1b = getmem8(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
			cpu->oper2b = getmem8(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi]);
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 1;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 1;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 1;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 1;
			}

			flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			if ((cpu->reptype == 1) && !cpu->zf) {
				break;
			}
			else if ((cpu->reptype == 2) && (cpu->zf == 1)) {
				break;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xA7:	/* A7 CMPSW */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			cpu->oper1 = getmem16(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
			cpu->oper2 = getmem16(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi]);
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 2;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 2;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 2;
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 2;
			}

			flag_sub16(cpu, cpu->oper1, cpu->oper2);
			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			if ((cpu->reptype == 1) && !cpu->zf) {
				break;
			}

			if ((cpu->reptype == 2) && (cpu->zf == 1)) {
				break;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xA8:	/* A8 TEST cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			flag_log8(cpu, cpu->oper1b & cpu->oper2b);
			break;

		case 0xA9:	/* A9 TEST eAX Iv */
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			flag_log16(cpu, cpu->oper1 & cpu->oper2);
			break;

		case 0xAA:	/* AA STOSB */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			putmem8(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi], cpu->regs.byteregs[regal]);
			if (cpu->df) {
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 1;
			}
			else {
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 1;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xAB:	/* AB STOSW */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			putmem16(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi], cpu->regs.wordregs[regax]);
			if (cpu->df) {
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 2;
			}
			else {
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 2;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xAC:	/* AC LODSB */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			cpu->regs.byteregs[regal] = getmem8(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 1;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 1;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xAD:	/* AD LODSW */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			cpu->oper1 = getmem16(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
			cpu->regs.wordregs[regax] = cpu->oper1;
			if (cpu->df) {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] - 2;
			}
			else {
				cpu->regs.wordregs[regsi] = cpu->regs.wordregs[regsi] + 2;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xAE:	/* AE SCASB */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			cpu->oper1b = cpu->regs.byteregs[regal];
			cpu->oper2b = getmem8(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi]);
			flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
			if (cpu->df) {
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 1;
			}
			else {
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 1;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			if ((cpu->reptype == 1) && !cpu->zf) {
				break;
			}
			else if ((cpu->reptype == 2) && (cpu->zf == 1)) {
				break;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xAF:	/* AF SCASW */
			if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
				break;
			}

			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segregs[reges], cpu->regs.wordregs[regdi]);
			flag_sub16(cpu, cpu->oper1, cpu->oper2);
			if (cpu->df) {
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] - 2;
			}
			else {
				cpu->regs.wordregs[regdi] = cpu->regs.wordregs[regdi] + 2;
			}

			if (cpu->reptype) {
				cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			}

			if ((cpu->reptype == 1) && !cpu->zf) {
				break;
			}
			else if ((cpu->reptype == 2) && (cpu->zf == 1)) { //did i fix a typo bug? this used to be & instead of &&
				break;
			}

			loopcount++;
			if (!cpu->reptype) {
				break;
			}

			cpu->ip = firstip;
			break;

		case 0xB0:	/* B0 MOV cpu->regs.byteregs[regal] Ib */
			cpu->regs.byteregs[regal] = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			break;

		case 0xB1:	/* B1 MOV cpu->regs.byteregs[regcl] Ib */
			cpu->regs.byteregs[regcl] = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			break;

		case 0xB2:	/* B2 MOV cpu->regs.byteregs[regdl] Ib */
			cpu->regs.byteregs[regdl] = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			break;

		case 0xB3:	/* B3 MOV cpu->regs.byteregs[regbl] Ib */
			cpu->regs.byteregs[regbl] = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			break;

		case 0xB4:	/* B4 MOV cpu->regs.byteregs[regah] Ib */
			cpu->regs.byteregs[regah] = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			break;

		case 0xB5:	/* B5 MOV cpu->regs.byteregs[regch] Ib */
			cpu->regs.byteregs[regch] = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			break;

		case 0xB6:	/* B6 MOV cpu->regs.byteregs[regdh] Ib */
			cpu->regs.byteregs[regdh] = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			break;

		case 0xB7:	/* B7 MOV cpu->regs.byteregs[regbh] Ib */
			cpu->regs.byteregs[regbh] = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			break;

		case 0xB8:	/* B8 MOV eAX Iv */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			cpu->regs.wordregs[regax] = cpu->oper1;
			break;

		case 0xB9:	/* B9 MOV eCX Iv */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			cpu->regs.wordregs[regcx] = cpu->oper1;
			break;

		case 0xBA:	/* BA MOV eDX Iv */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			cpu->regs.wordregs[regdx] = cpu->oper1;
			break;

		case 0xBB:	/* BB MOV eBX Iv */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			cpu->regs.wordregs[regbx] = cpu->oper1;
			break;

		case 0xBC:	/* BC MOV eSP Iv */
			cpu->regs.wordregs[regsp] = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			break;

		case 0xBD:	/* BD MOV eBP Iv */
			cpu->regs.wordregs[regbp] = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			break;

		case 0xBE:	/* BE MOV eSI Iv */
			cpu->regs.wordregs[regsi] = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			break;

		case 0xBF:	/* BF MOV eDI Iv */
			cpu->regs.wordregs[regdi] = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			break;

		case 0xC0:	/* C0 GRP2 byte imm8 (80186+) */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			writerm8(cpu, cpu->rm, op_grp2_8(cpu, cpu->oper2b));
			break;

		case 0xC1:	/* C1 GRP2 word imm8 (80186+) */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			cpu->oper2 = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			writerm16(cpu, cpu->rm, op_grp2_16(cpu, (uint8_t)cpu->oper2));
			break;

		case 0xC2:	/* C2 RET Iw */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			cpu->ip = pop(cpu);
			cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regsp] + cpu->oper1;
			break;

		case 0xC3:	/* C3 RET */
			cpu->ip = pop(cpu);
			break;

		case 0xC4:	/* C4 LES Gv Mp */
			modregrm(cpu);
			getea(cpu, cpu->rm);
			putreg16(cpu, cpu->reg, cpu_read(cpu, cpu->ea) + cpu_read(cpu, cpu->ea + 1) * 256);
			cpu->segregs[reges] = cpu_read(cpu, cpu->ea + 2) + cpu_read(cpu, cpu->ea + 3) * 256;
			break;

		case 0xC5:	/* C5 LDS Gv Mp */
			modregrm(cpu);
			getea(cpu, cpu->rm);
			putreg16(cpu, cpu->reg, cpu_read(cpu, cpu->ea) + cpu_read(cpu, cpu->ea + 1) * 256);
			cpu->segregs[regds] = cpu_read(cpu, cpu->ea + 2) + cpu_read(cpu, cpu->ea + 3) * 256;
			break;

		case 0xC6:	/* C6 MOV Eb Ib */
			modregrm(cpu);
			writerm8(cpu, cpu->rm, getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			break;

		case 0xC7:	/* C7 MOV Ev Iv */
			modregrm(cpu);
			writerm16(cpu, cpu->rm, getmem16(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 2);
			break;

		case 0xC8:	/* C8 ENTER (80186+) */
			cpu->stacksize = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			cpu->nestlev = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			push(cpu, cpu->regs.wordregs[regbp]);
			cpu->frametemp = cpu->regs.wordregs[regsp];
			if (cpu->nestlev) {
				for (cpu->temp16 = 1; cpu->temp16 < cpu->nestlev; ++cpu->temp16) {
					cpu->regs.wordregs[regbp] = cpu->regs.wordregs[regbp] - 2;
					push(cpu, cpu->regs.wordregs[regbp]);
				}

				push(cpu, cpu->frametemp); //cpu->regs.wordregs[regsp]);
			}

			cpu->regs.wordregs[regbp] = cpu->frametemp;
			cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regbp] - cpu->stacksize;

			break;

		case 0xC9:	/* C9 LEAVE (80186+) */
			cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regbp];
			cpu->regs.wordregs[regbp] = pop(cpu);
			break;

		case 0xCA:	/* CA RETF Iw */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			cpu->ip = pop(cpu);
			cpu->segregs[regcs] = pop(cpu);
			cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regsp] + cpu->oper1;
			break;

		case 0xCB:	/* CB RETF */
			cpu->ip = pop(cpu);
			cpu->segregs[regcs] = pop(cpu);
			break;

		case 0xCC:	/* CC INT 3 */
			cpu_intcall(cpu, 3);
			break;

		case 0xCD:	/* CD INT Ib */
			cpu->oper1b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			cpu_intcall(cpu, cpu->oper1b);
			break;

		case 0xCE:	/* CE INTO */
			if (cpu->of) {
				cpu_intcall(cpu, 4);
			}
			break;

		case 0xCF:	/* CF IRET */
			cpu->ip = pop(cpu);
			cpu->segregs[regcs] = pop(cpu);
			decodeflagsword(cpu, pop(cpu));

			/*
			 * if (net.enabled) net.canrecv = 1;
			 */
			break;

		case 0xD0:	/* D0 GRP2 Eb 1 */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			writerm8(cpu, cpu->rm, op_grp2_8(cpu, 1));
			break;

		case 0xD1:	/* D1 GRP2 Ev 1 */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			writerm16(cpu, cpu->rm, op_grp2_16(cpu, 1));
			break;

		case 0xD2:	/* D2 GRP2 Eb cpu->regs.byteregs[regcl] */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			writerm8(cpu, cpu->rm, op_grp2_8(cpu, cpu->regs.byteregs[regcl]));
			break;

		case 0xD3:	/* D3 GRP2 Ev cpu->regs.byteregs[regcl] */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			writerm16(cpu, cpu->rm, op_grp2_16(cpu, cpu->regs.byteregs[regcl]));
			break;

		case 0xD4:	/* D4 AAM I0 */
			cpu->oper1 = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			if (!cpu->oper1) {
				cpu_intcall(cpu, 0);
				break;
			}	/* division by zero */

			cpu->regs.byteregs[regah] = (cpu->regs.byteregs[regal] / cpu->oper1) & 255;
			cpu->regs.byteregs[regal] = (cpu->regs.byteregs[regal] % cpu->oper1) & 255;
			flag_szp16(cpu, cpu->regs.wordregs[regax]);
			break;

		case 0xD5:	/* D5 AAD I0 */
			cpu->oper1 = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			cpu->regs.byteregs[regal] = (cpu->regs.byteregs[regah] * cpu->oper1 + cpu->regs.byteregs[regal]) & 255;
			cpu->regs.byteregs[regah] = 0;
			flag_szp16(cpu, cpu->regs.byteregs[regah] * cpu->oper1 + cpu->regs.byteregs[regal]);
			cpu->sf = 0;
			break;

		case 0xD6:	/* D6 XLAT on V20/V30, SALC on 8086/8088 */
#ifndef CPU_NO_SALC
			cpu->regs.byteregs[regal] = cpu->cf ? 0xFF : 0x00;
			break;
#endif

		case 0xD7:	/* D7 XLAT */
			cpu->regs.byteregs[regal] = cpu_read(cpu, cpu->useseg * 16 + (cpu->regs.wordregs[regbx]) + cpu->regs.byteregs[regal]);
			break;

		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDE:
		case 0xDD:
		case 0xDF:	/* escape to x87 FPU (unsupported) */
			modregrm(cpu);
			break;

		case 0xE0:	/* E0 LOOPNZ Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			if ((cpu->regs.wordregs[regcx]) && !cpu->zf) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0xE1:	/* E1 LOOPZ Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			if (cpu->regs.wordregs[regcx] && (cpu->zf == 1)) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0xE2:	/* E2 LOOP Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			cpu->regs.wordregs[regcx] = cpu->regs.wordregs[regcx] - 1;
			if (cpu->regs.wordregs[regcx]) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0xE3:	/* E3 JCXZ Jb */
			cpu->temp16 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (!cpu->regs.wordregs[regcx]) {
				cpu->ip = cpu->ip + cpu->temp16;
			}
			break;

		case 0xE4:	/* E4 IN cpu->regs.byteregs[regal] Ib */
			cpu->oper1b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			cpu->regs.byteregs[regal] = (uint8_t)port_read(cpu, cpu->oper1b);
			break;

		case 0xE5:	/* E5 IN eAX Ib */
			cpu->oper1b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			cpu->regs.wordregs[regax] = port_readw(cpu, cpu->oper1b);
			break;

		case 0xE6:	/* E6 OUT Ib cpu->regs.byteregs[regal] */
			cpu->oper1b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			port_write(cpu, cpu->oper1b, cpu->regs.byteregs[regal]);
			break;

		case 0xE7:	/* E7 OUT Ib eAX */
			cpu->oper1b = getmem8(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 1);
			port_writew(cpu, cpu->oper1b, cpu->regs.wordregs[regax]);
			break;

		case 0xE8:	/* E8 CALL Jv */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			push(cpu, cpu->ip);
			cpu->ip = cpu->ip + cpu->oper1;
			break;

		case 0xE9:	/* E9 JMP Jv */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			cpu->ip = cpu->ip + cpu->oper1;
			break;

		case 0xEA:	/* EA JMP Ap */
			cpu->oper1 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			StepIP(cpu, 2);
			cpu->oper2 = getmem16(cpu, cpu->segregs[regcs], cpu->ip);
			cpu->ip = cpu->oper1;
			cpu->segregs[regcs] = cpu->oper2;
			break;

		case 0xEB:	/* EB JMP Jb */
			cpu->oper1 = signext(getmem8(cpu, cpu->segregs[regcs], cpu->ip));
			StepIP(cpu, 1);
			cpu->ip = cpu->ip + cpu->oper1;
			break;

		case 0xEC:	/* EC IN cpu->regs.byteregs[regal] regdx */
			cpu->oper1 = cpu->regs.wordregs[regdx];
			cpu->regs.byteregs[regal] = (uint8_t)port_read(cpu, cpu->oper1);
			break;

		case 0xED:	/* ED IN eAX regdx */
			cpu->oper1 = cpu->regs.wordregs[regdx];
			cpu->regs.wordregs[regax] = port_readw(cpu, cpu->oper1);
			break;

		case 0xEE:	/* EE OUT regdx cpu->regs.byteregs[regal] */
			cpu->oper1 = cpu->regs.wordregs[regdx];
			port_write(cpu, cpu->oper1, cpu->regs.byteregs[regal]);
			break;

		case 0xEF:	/* EF OUT regdx eAX */
			cpu->oper1 = cpu->regs.wordregs[regdx];
			port_writew(cpu, cpu->oper1, cpu->regs.wordregs[regax]);
			break;

		case 0xF0:	/* F0 LOCK */
			break;

		case 0xF4:	/* F4 HLT */
			cpu->hltstate = 1;
			break;

		case 0xF5:	/* F5 CMC */
			if (!cpu->cf) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}
			break;

		case 0xF6:	/* F6 GRP3a Eb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			op_grp3_8(cpu);
			if ((cpu->reg > 1) && (cpu->reg < 4)) {
				writerm8(cpu, cpu->rm, cpu->res8);
			}
			break;

		case 0xF7:	/* F7 GRP3b Ev */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			op_grp3_16(cpu);
			if ((cpu->reg > 1) && (cpu->reg < 4)) {
				writerm16(cpu, cpu->rm, cpu->res16);
			}
			break;

		case 0xF8:	/* F8 CLC */
			cpu->cf = 0;
			break;

		case 0xF9:	/* F9 STC */
			cpu->cf = 1;
			break;

		case 0xFA:	/* FA CLI */
			cpu->ifl = 0;
			break;

		case 0xFB:	/* FB STI */
			cpu->ifl = 1;
			break;

		case 0xFC:	/* FC CLD */
			cpu->df = 0;
			break;

		case 0xFD:	/* FD STD */
			cpu->df = 1;
			break;

		case 0xFE:	/* FE GRP4 Eb */
			modregrm(cpu);
			cpu->oper1b = readrm8(cpu, cpu->rm);
			cpu->oper2b = 1;
			if (!cpu->reg) {
				cpu->tempcf = cpu->cf;
				cpu->res8 = cpu->oper1b + cpu->oper2b;
				flag_add8(cpu, cpu->oper1b, cpu->oper2b);
				cpu->cf = cpu->tempcf;
				writerm8(cpu, cpu->rm, cpu->res8);
			}
			else {
				cpu->tempcf = cpu->cf;
				cpu->res8 = cpu->oper1b - cpu->oper2b;
				flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
				cpu->cf = cpu->tempcf;
				writerm8(cpu, cpu->rm, cpu->res8);
			}
			break;

		case 0xFF:	/* FF GRP5 Ev */
			modregrm(cpu);
			cpu->oper1 = readrm16(cpu, cpu->rm);
			op_grp5(cpu);
			break;

		default:
#ifdef CPU_ALLOW_ILLEGAL_OP_EXCEPTION
			cpu_intcall(cpu, 6); /* trip invalid opcode exception. this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs. */
						   /* technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough. */
			debug_log(DEBUG_INFO, "[CPU] Invalid opcode exception at %04X:%04X\r\n", cpu->segregs[regcs], firstip);
#endif
			break;
		}

	skipexecution:
		;
	}
}

void cpu_registerIntCallback(CPU_t* cpu, uint8_t interrupt, void (*cb)(CPU_t*, uint8_t)) {
	cpu->int_callback[interrupt] = cb;
}
