/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 by Ralf Baechle
 */
#ifndef __ASM_MACH_AR531X_PARAM_H
#define __ASM_MACH_AR531X_PARAM_H

/*
 * AR531X is currently using 10ms ticks instead of generic kernel 2.6 1ms ticks to
 * minimize irq processing
 */
#define HZ		100		/* Internal kernel timer frequency */

#endif /* __ASM_MACH_AR531X_PARAM_H */
