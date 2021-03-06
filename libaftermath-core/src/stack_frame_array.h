/**
 * Author: Richard Neill <richard.neill@manchester.ac.uk>
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

#ifndef AM_STACK_FRAME_ARRAY_H
#define AM_STACK_FRAME_ARRAY_H

#include <aftermath/core/typed_array.h>
#include <aftermath/core/in_memory.h>
#include <aftermath/core/bsearch.h>
#include <aftermath/core/interval_array.h>

AM_DECL_TYPED_ARRAY_CUSTOM_PREALLOC(am_stack_frame_array, struct am_stack_frame, 16384)

AM_DECL_INTERVAL_EVENT_ARRAY_BSEARCH_FIRST_OVERLAPPING(am_stack_frame_array,
						       struct am_stack_frame,
						       interval)

AM_DECL_INTERVAL_EVENT_ARRAY_BSEARCH_LAST_OVERLAPPING(am_stack_frame_array,
						      struct am_stack_frame,
						      interval)

/* Comparison function to sort stack frames by their starting timestamp */
static inline int am_stack_frame_cmp_interval_start(
	struct am_stack_frame* const * pa,
	struct am_stack_frame* const * pb)
{
	const struct am_stack_frame* a = *pa;
	const struct am_stack_frame* b = *pb;

	return a->interval.start > b->interval.start ? 1 :
		(a->interval.start < b->interval.start ? -1 : 0);
}

/* Generate sorting function to sort the array by starting timestamp */
AM_DECL_QSORT_SUFFIX(am_stack_frame_,
		     _interval_start_ascending,
		     struct am_stack_frame*,
		     am_stack_frame_cmp_interval_start)

#endif
