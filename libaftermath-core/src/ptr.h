/**
 * Author: Andi Drebes <andi@drebesium.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#ifndef AM_PTR_H
#define AM_PTR_H

#include <stddef.h>

/* Safe comparisons of pointers */
#define AM_PTR_LESS(a, b) (((uintptr_t)(a)) < ((uintptr_t)(b)))
#define AM_PTR_LEQ(a, b) (((uintptr_t)(a)) <= ((uintptr_t)(b)))
#define AM_PTR_GREATER(a, b) (((uintptr_t)(a)) > ((uintptr_t)(b)))
#define AM_PTR_GEQ(a, b) (((uintptr_t)(a)) >= ((uintptr_t)(b)))

/* Increases the address p by val bytes regardless of the type of p and returns
 * the new pointer as void* */
#define AM_PTR_ADD(p, val) ((void*)(((char*)p)+(val)))

/* Decreases the address p by val bytes regardless of the type of p and returns
 * the new pointer as void* */
#define AM_PTR_SUB(p, val) ((void*)(((char*)p)-(val)))

/* Calculates the index of an element e in an array arr */
#define AM_ARRAY_INDEX(arr, e) \
	((((uintptr_t)(e)) - ((uintptr_t)(arr))) / sizeof(*(e)))

/* Calculates the distance between two elements of an array. The pointers e1 and
 * e2 must have the same type.  */
#define AM_ARRAY_INDEX_DISTANCE(e1, e2)				\
	(AM_PTR_LEQ(e1, e2) ?						\
	 ((((uintptr_t)(e2)) - ((uintptr_t)(e1))) / sizeof(*(e1))) :	\
	 ((((uintptr_t)(e1)) - ((uintptr_t)(e2))) / sizeof(*(e1))))

#endif
