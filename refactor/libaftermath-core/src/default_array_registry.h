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

#ifndef AM_DEFAULT_ARRAY_REGISTRY_H
#define AM_DEFAULT_ARRAY_REGISTRY_H

/* Declares a set of functions for an array registry entry: allocate, free,
 * init, destroy. */
#define AM_DECL_DEFAULT_ARRAY_REGISTRY_FUNCTIONS(prefix)	\
	static void* prefix##_default_allocate(void)		\
	{							\
		return malloc(sizeof(struct prefix));		\
	}							\
								\
	static void prefix##_default_free(void* a)		\
	{							\
		free(a);					\
	}							\
								\
	static int prefix##_default_init(void* a)		\
	{							\
		prefix##_init(a);				\
		return 0;					\
	}							\
								\
	static void prefix##_default_destroy(void* a)		\
	{							\
		prefix##_destroy(a);				\
	}

/* Registers an array type at an array registry with the functions generated by
 * AM_DECL_DEFAULT_ARRAY_REGISTRY_FUNCTIONS by invoking
 * am_array_registry_add.
 */
#define AM_DEFAULT_ARRAY_REGISTRY_REGISTER(r, prefix, name)	\
	am_array_registry_add(r, name,				\
			      prefix##_default_allocate,	\
			      prefix##_default_free,	\
			      prefix##_default_init,	\
			      prefix##_default_destroy)

#endif
