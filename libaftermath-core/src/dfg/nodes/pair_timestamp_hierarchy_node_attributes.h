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

#ifndef AM_DFG_NODE_PAIR_TIMESTAMP_HIERARCHY_ATTRIBUTES_H
#define AM_DFG_NODE_PAIR_TIMESTAMP_HIERARCHY_ATTRIBUTES_H

#include <aftermath/core/dfg_node.h>

int am_dfg_pair_timestamp_hierarchy_attributes_node_process(struct am_dfg_node* n);

/**
 * Node that extracts the attributes of a pair composed of a timestamp and a
 * hierarchy node and makes them available at two separate output ports.
 */
AM_DFG_DECL_BUILTIN_NODE_TYPE(
	am_dfg_pair_timestamp_hierarchy_attributes_node_type,
	"am::core::pair<am::core::timestamp,const_am::core::hierarchy_node>::attributes",
	"Pair<Timestamp,Hierarchy Node> Attributes",
	AM_DFG_NODE_DEFAULT_SIZE,
	AM_DFG_DEFAULT_PORT_DEPS_PURE_FUNCTIONAL,
	AM_DFG_NODE_FUNCTIONS({
		.process = am_dfg_pair_timestamp_hierarchy_attributes_node_process
	}),
	AM_DFG_NODE_PORTS(
		{ "pairs", "am::core::pair<am::core::timestamp,const am::core::hierarchy_node*>", AM_DFG_PORT_IN },
		{ "timestamp", "am::core::timestamp", AM_DFG_PORT_OUT },
		{ "hierarchy node", "const am::core::hierarchy_node*", AM_DFG_PORT_OUT }),
	AM_DFG_PORT_DEPS(),
	AM_DFG_NODE_PROPERTIES())

AM_DFG_ADD_BUILTIN_NODE_TYPES(&am_dfg_pair_timestamp_hierarchy_attributes_node_type)

#endif
