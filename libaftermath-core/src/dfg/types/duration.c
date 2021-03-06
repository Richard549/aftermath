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

#include "duration.h"
#include <aftermath/core/dfg/types/timestamp.h>
#include <stdio.h>

int am_dfg_type_duration_to_string(const struct am_dfg_type* t,
				   void* ptr,
				   char** out,
				   int* cst)
{
	char* ret;
	struct am_time_offset* d = ptr;

	if(!(ret = malloc(AM_TIMESTAMP_T_MAX_DECIMAL_DIGITS + 2)))
		return 1;

	snprintf(ret, AM_TIMESTAMP_T_MAX_DECIMAL_DIGITS,
		 "%s%" AM_TIMESTAMP_T_FMT,
		 (d->sign) ? "-" : "",
		 d->abs);

	*out = ret;
	*cst = 0;

	return 0;
}

int am_dfg_type_duration_from_string(const struct am_dfg_type* t,
				     const char* str,
				     void* out)
{
	struct am_time_offset* d = out;
	am_timestamp_t abs;
	const char* rstr = str;
	int sign = 0;

	if(str[0] == '-') {
		sign = 1;
		rstr = str + 1;
	}

	if(am_dfg_type_timestamp_from_string(t, rstr, &abs))
		return 1;

	d->abs = abs;
	d->sign = sign;

	return 0;
}

int am_dfg_type_duration_check_string(const struct am_dfg_type* t,
				      const char* str)
{
	const char* rstr = str;
	am_timestamp_t abs;

	if(str[0] == '-')
		rstr = str + 1;

	if(am_dfg_type_timestamp_from_string(t, rstr, &abs))
		return 1;

	return 0;
}
