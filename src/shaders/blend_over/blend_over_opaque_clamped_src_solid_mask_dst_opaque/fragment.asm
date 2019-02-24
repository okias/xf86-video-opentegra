/*
 * Copyright (c) GRATE-DRIVER project 2019
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

pseq_to_dw_exec_nb = 1	// the number of 'EXEC' block where DW happens
alu_buffer_size = 1	// number of .rgba regs carried through pipeline

.uniforms
	[2].l = "mask_color.r";
	[2].h = "mask_color.g";
	[3].l = "mask_color.b";
	[3].h = "mask_color.a";

.asm

EXEC
	// fetch dst pixel to r2,r3
	PSEQ:	0x0081000A

	MFU:	sfu:  rcp r4
		mul0: bar, sfu, bar0
		mul1: bar, sfu, bar1
		ipl:  t0.fp20, t0.fp20, NOP, NOP

	// sample tex0 (src)
	TEX:	tex r0, r1, tex0, r0, r1, r2

	// r0,r1 = (mask.bgra - 1) * -dst.bgra + src.bgra * mask.bgra
	ALU:
		ALU0:	MAD  lp.lh, -r2.l, u2.l-1, #0
		ALU1:	MAD  lp.lh, -r2.h, u2.h-1, #0
		ALU2:	MAD  lp.lh, -r3.l, u3.l-1, #0

	ALU:
		ALU0:	MAD  r0.l, u2.l, r0.l, alu0 (sat)
		ALU1:	MAD  r0.h, u2.h, r0.h, alu1 (sat)
		ALU2:	MAD  r1.l, u3.l, r1.l, alu2 (sat)
		ALU3:	MAD  r1.h, #0, #0, #0

	DW:	store rt1, r0, r1
;
