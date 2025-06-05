/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MINMAX_H
#define _LINUX_MINMAX_H

#ifndef __is_constexpr
#define __is_constexpr(x) (0) 
#endif
#define min_t(type, x, y) ((type)(x) < (type)(y) ? (type)(x) : (type)(y))

#endif	/* _LINUX_MINMAX_H */
