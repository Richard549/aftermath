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

#ifndef AM_RENDER_DFG_BUILTIN_NODES_H
#define AM_RENDER_DFG_BUILTIN_NODES_H

#include <aftermath/core/dfg_node_type_registry.h>

int am_render_dfg_builtin_node_types_register(
	struct am_dfg_node_type_registry* ntr,
	struct am_dfg_type_registry* tr);

#endif