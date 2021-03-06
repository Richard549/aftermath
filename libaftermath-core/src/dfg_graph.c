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

#include <aftermath/core/dfg_graph.h>
#include <limits.h>

void am_dfg_graph_init(struct am_dfg_graph* g, long flags)
{
	am_dfg_node_idtree_init(&g->id_tree);
	INIT_LIST_HEAD(&g->buffers);

	g->flags = flags;
}

/* Reset all buffers used by any node in the graph */
void am_dfg_graph_reset_buffers(const struct am_dfg_graph* g)
{
	struct am_dfg_buffer* b;

	am_dfg_graph_for_each_buffer(g, b)
		am_dfg_buffer_reset(b);
}

/* Destroy a graph. According to the flags buffers and nodes are destroyed /
 * preserved. */
void am_dfg_graph_destroy(struct am_dfg_graph* g)
{
	struct am_dfg_node* n;
	struct am_dfg_node* ntmp;
	struct am_dfg_buffer* b;
	struct am_dfg_buffer* btmp;

	if(g->flags & AM_DFG_GRAPH_DESTROY_NODES) {
		am_dfg_graph_for_each_node_safe(g, n, ntmp) {
			am_dfg_node_destroy(n);
			free(n);
		}
	}

	if(g->flags & AM_DFG_GRAPH_DESTROY_BUFFERS) {
		am_dfg_graph_for_each_buffer_safe(g, b, btmp) {
			am_dfg_buffer_destroy(b);
			free(b);
		}
	}
}

/* Add a node to the graph */
int am_dfg_graph_add_node(struct am_dfg_graph* g, struct am_dfg_node* n)
{
	return am_dfg_node_idtree_insert(&g->id_tree, n);
}

/* Find a node in the graph by id. Returns the node if found or NULL if no such
 * node exists in the graph. */
struct am_dfg_node* am_dfg_graph_find_node(const struct am_dfg_graph* g, long id)
{
	return am_dfg_node_idtree_find(&g->id_tree, id);
}

/*
 * Remove a node from the graph without disconnecting it from other nodes.
 */
void am_dfg_graph_remove_node_no_disconnect(struct am_dfg_graph* g,
					   struct am_dfg_node* n)
{
	am_dfg_node_idtree_remove(&g->id_tree, n);
}

/*
 * Remove a node from the graph. This destroys all of the node's connections,
 * but does not destroy or free the node itself.
 */
int am_dfg_graph_remove_node(struct am_dfg_graph* g, struct am_dfg_node* n)
{
	struct am_dfg_port* p;
	struct am_dfg_port* i;
	size_t c;

	am_dfg_node_for_each_port(n, p)
		am_dfg_port_for_each_connected_port_safe(p, i, c)
			if(am_dfg_port_disconnect(p, i))
				return 1;

	am_dfg_graph_remove_node_no_disconnect(g, n);

	return 0;
}

/* Add a buffer to the graph */
static void am_dfg_graph_add_buffer(struct am_dfg_graph* g,
				    struct am_dfg_buffer* b)
{
	list_add(&b->list, &g->buffers);
}

/*
 * Connect two ports. This includes a check if the port types and directions are
 * compatible, but does not check for cycles. If the out port did not have a
 * buffer associated to it prior to the connection, a new buffer is allocated.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_dfg_graph_connectp(struct am_dfg_graph* g,
			  struct am_dfg_type_registry* tr,
			  struct am_dfg_port* src_port,
			  struct am_dfg_port* dst_port)
{
	struct am_dfg_buffer* buffer;
	const struct am_dfg_node_type_functions* src_node_funs;
	const struct am_dfg_node_type_functions* dst_node_funs;

	/* Port directions compatible? */
	if(!(src_port->type->flags & AM_DFG_PORT_OUT) ||
	   !(dst_port->type->flags & AM_DFG_PORT_IN))
	{
		goto out_err;
	}

	/* Port data types compatible? */
	if(!am_dfg_type_registry_types_compatible(
		   tr, src_port->type->type, dst_port->type->type))
	{
		goto out_err;
	}

	/* Destination already connected? */
	if(dst_port->num_connections != 0)
		goto out_err;

	if(am_dfg_port_connect_onesided(src_port, dst_port))
		goto out_err;

	if(am_dfg_port_connect_onesided(dst_port, src_port))
		goto out_err_unc_dst;

	/* Assign buffer or allocate a new one */
	if(src_port->buffer) {
		dst_port->buffer = src_port->buffer;
		am_dfg_buffer_inc_ref(dst_port->buffer);

		dst_node_funs = &dst_port->node->type->functions;

		if(dst_node_funs->connect)
			dst_node_funs->connect(dst_port->node, dst_port);
	} else {
		if(!(buffer = malloc(sizeof(*buffer))))
			goto out_err_unc_src;

		am_dfg_buffer_init(buffer, src_port->type->type);
		am_dfg_graph_add_buffer(g, buffer);

		dst_port->buffer = buffer;
		src_port->buffer = buffer;

		src_node_funs = &src_port->node->type->functions;
		dst_node_funs = &dst_port->node->type->functions;

		if(src_node_funs->connect)
			src_node_funs->connect(src_port->node, src_port);

		if(dst_node_funs->connect)
			dst_node_funs->connect(dst_port->node, dst_port);
	}

	return 0;

out_err_unc_src:
	am_dfg_port_disconnect_onesided(dst_port, src_port);
out_err_unc_dst:
	am_dfg_port_disconnect_onesided(src_port, dst_port);
out_err:
	return 1;
}

/*
 * Disconnects src_port and old_dst_port and reconnects src_port to
 * new_dst_port.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_dfg_graph_reconnectp(struct am_dfg_graph* g,
			    struct am_dfg_type_registry* tr,
			    struct am_dfg_port* src_port,
			    struct am_dfg_port* old_dst_port,
			    struct am_dfg_port* new_dst_port)
{
	/* Prevent buffer from being freed by the disconnect */
	am_dfg_buffer_inc_ref(src_port->buffer);

	if(am_dfg_port_disconnect(src_port, old_dst_port) ||
	   am_dfg_graph_connectp(g, tr, src_port, new_dst_port))
	{
		am_dfg_buffer_dec_ref(src_port->buffer);
		return 1;
	}

	am_dfg_buffer_dec_ref(src_port->buffer);
	return 0;
}

/*
 * Connect two nodes src and dest using the ports identified by src_port_name
 * and dst_port_name, respectively.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_dfg_graph_connectn(struct am_dfg_graph* g,
			  struct am_dfg_type_registry* tr,
			  struct am_dfg_node* src, const char* src_port_name,
			  struct am_dfg_node* dst, const char* dst_port_name)
{
	struct am_dfg_port* src_pi;
	struct am_dfg_port* dst_pi;

	if(!(src_pi = am_dfg_node_find_port(src, src_port_name)))
		return 1;

	if(!(dst_pi = am_dfg_node_find_port(dst, dst_port_name)))
		return 1;

	return am_dfg_graph_connectp(g, tr, src_pi, dst_pi);
}

enum {
	AM_DFG_CYCLE_UNMARKED = 0,
	AM_DFG_CYCLE_MARKED = 1,
	AM_DFG_CYCLE_VISITED = 2
};

/*
 * Checks if the subgraph rooted by n has a cycle. A hypothetical edge
 * (extra_src, extra_dst) is included in the check and any connection between
 * the ports ignore_src and ignore_dst is not taken into account. Extra_src,
 * extra_dst, ignore_src and ignore_dst may be set to NULL to check only
 * existing edges.
 *
 * If cycle is non-NULL and a cycle is detected, the connections of the detected
 * cycle will also be returned (in reverse order) while *add_to_cycle is 1.
 *
 * Returns true if at least one cycle is found, otherwise 0.
 */
static int am_dfg_graph_has_cycle_rec(struct am_dfg_node* n,
				      const struct am_dfg_port* extra_src,
				      const struct am_dfg_port* extra_dst,
				      const struct am_dfg_port* ignore_src,
				      const struct am_dfg_port* ignore_dst,
				      struct am_dfg_path* cycle,
				      int* add_to_cycle)
{
	struct am_dfg_node* d;
	struct am_dfg_port* p;

	/* Already checked, prune this subgraph */
	if(n->marking == AM_DFG_CYCLE_VISITED)
		return 0;

	/* All nodes marked, but not visited are on the current path, so this
	 * must be a cycle */
	if(n->marking == AM_DFG_CYCLE_MARKED)
		return 1;

	n->marking = AM_DFG_CYCLE_MARKED;

	/* Evaluate extra edge */
	if(n == extra_src->node) {
		if(am_dfg_graph_has_cycle_rec(extra_dst->node,
					      extra_src, extra_dst,
					      ignore_src, ignore_dst,
					      cycle,
					      add_to_cycle))
		{
			if(cycle && add_to_cycle && *add_to_cycle) {
				am_dfg_path_append_leg(cycle,
						       extra_src,
						       extra_dst);

				if(am_dfg_revpath_ends_closed(cycle))
					*add_to_cycle = 0;
			}

			return 1;
		}
	}

	/* Check all actual, outgoing edges of the current node */
	am_dfg_node_for_each_output_port(n, p) {
		for(size_t j = 0; j < p->num_connections; j++) {
			d = p->connections[j]->node;

			/* Edge ignored? */
			if(p == ignore_src &&
			   p->connections[j] == ignore_dst)
			{
				continue;
			}

			if(am_dfg_graph_has_cycle_rec(d,
						      extra_src,
						      extra_dst,
						      ignore_src,
						      ignore_dst,
						      cycle,
						      add_to_cycle))
			{
				if(cycle && add_to_cycle && *add_to_cycle) {
					am_dfg_path_append_leg(
						cycle,
						p, p->connections[j]);

					if(am_dfg_revpath_ends_closed(cycle))
						*add_to_cycle = 0;
				}

				return 1;
			}
		}
	}

	n->marking = AM_DFG_CYCLE_VISITED;

	return 0;
}

/*
 * Checks if the graph has a cycle. A hypothetical edge (extra_src, extra_dst)
 * is included in the check and any connection between the ports ignore_src and
 * ignore_dst is not taken into account. Extra_src, extra_dst, ignore_src and
 * ignore_dst may be set to NULL to check only existing edges.
 *
 * If cycle is non-NULL and a cycle is detected, the connections of the detected
 * cycle will also be returned in reverse order.
 *
 * Returns true if at least one cycle is found, otherwise 0.
 */
int am_dfg_graph_has_cycle(const struct am_dfg_graph* g,
			   const struct am_dfg_port* extra_src,
			   const struct am_dfg_port* extra_dst,
			   const struct am_dfg_port* ignore_src,
			   const struct am_dfg_port* ignore_dst,
			   struct am_dfg_path* cycle)
{
	struct am_dfg_node* n;
	int add_to_cycle = 1;

	am_dfg_graph_for_each_node(g, n)
		n->marking = AM_DFG_CYCLE_UNMARKED;

	if(cycle)
		am_dfg_path_reset(cycle);

	/* Check all roots */
	am_dfg_graph_for_each_node(g, n) {
		if(am_dfg_node_is_root_ign(n, ignore_src, ignore_dst)) {
			if(am_dfg_graph_has_cycle_rec(n, extra_src, extra_dst,
						      ignore_src, ignore_dst,
						      cycle,
						      &add_to_cycle))
			{
				return 1;
			}
		}
	}

	/* Unmarked nodes belong to cycles that do not have any entry point. The
	 * presence of any such node indicates that there is a cycle. */
	am_dfg_graph_for_each_node(g, n)
		if(n->marking == AM_DFG_CYCLE_UNMARKED)
			return 1;

	return 0;
}

/* Returns the node with the lowest ID. If the graph is empty, NULL is
 * returned. */
struct am_dfg_node* am_dfg_graph_lowest_id_node(const struct am_dfg_graph* g)
{
	return am_dfg_node_idtree_first(&g->id_tree);
}

/* Returns the node with the highest ID. If the graph is empty, NULL is
 * returned. */
struct am_dfg_node* am_dfg_graph_highest_id_node(const struct am_dfg_graph* g)
{
	return am_dfg_node_idtree_last(&g->id_tree);
}

/* Generates a new ID. On success, the function returns 0 and sets *id to the
 * newly generated id. On failure, the function return 1 and leaves *id
 * untouched.
 */
int am_dfg_graph_generate_id(const struct am_dfg_graph* g, long* id)
{
	struct am_dfg_node* n;
	long curr_id = 0;

	if((n = am_dfg_node_idtree_last(&g->id_tree)))
		curr_id = n->id;

	if(curr_id == LONG_MAX)
		return 1;

	*id = curr_id + 1;

	return 0;
}

/* Merge the graph g into dst. Returns 0 on success, 1 otherwise. */
int am_dfg_graph_merge(struct am_dfg_graph* dst, struct am_dfg_graph* g)
{
	struct am_dfg_node* ndst_hid;
	struct am_dfg_node* nsrc_lid;
	struct am_dfg_node* n;
	long id = 0;

	if(dst->flags != g->flags)
		return 1;

	ndst_hid = am_dfg_graph_highest_id_node(dst);
	nsrc_lid = am_dfg_graph_lowest_id_node(dst);

	/* Shift source IDs if source and target graph have overlapping ID
	 * intervals */
	if(ndst_hid && nsrc_lid && ndst_hid->id > nsrc_lid->id) {
		id = ndst_hid->id;

		am_dfg_graph_for_each_node(g, n) {
			if(id == LONG_MAX)
				return 1;

			n->id = id++;
		}
	}

	/* Remove nodes from old graph and add to new one */
	am_dfg_graph_for_each_remove_node_safe(g, n) {
		am_dfg_graph_remove_node_no_disconnect(g, n);

		/* This cannot fail unless we're trying to insert a node with
		 * the same id into the idtree of the target graph. However,
		 * we've already made sure above that the id space of the two
		 * graphs is distinct.*/
		if(am_dfg_graph_add_node(dst, n))
			return 1;
	}

	list_splice(&g->buffers, &dst->buffers);

	return 0;
}

/* Convert a connection into its representation in object notation. Returns a newly
 * created object notation node of the form:
 *
 *    [<id>, <port_name>, <id>, <port_name>]
 *
 * In case of an error NULL is returned.
 */
static struct am_object_notation_node*
am_dfg_graph_connection_to_object_notation(struct am_dfg_port* psrc,
					struct am_dfg_port* pdst)
{
	struct am_object_notation_node_list* n_list;
	struct am_object_notation_node_string* n_src_port;
	struct am_object_notation_node_uint64* n_src_id;
	struct am_object_notation_node_string* n_dst_port;
	struct am_object_notation_node_uint64* n_dst_id;

	if(!(n_list = am_object_notation_node_list_create()))
		goto out_err;

	if(!(n_src_id = am_object_notation_node_uint64_create(psrc->node->id)))
		goto out_err_dlist;

	am_object_notation_node_list_add_item(
		n_list,
		(struct am_object_notation_node*)n_src_id);

	if(!(n_src_port = am_object_notation_node_string_create(psrc->type->name, 1)))
		goto out_err_dlist;

	am_object_notation_node_list_add_item(
		n_list,
		(struct am_object_notation_node*)n_src_port);

	if(!(n_dst_id = am_object_notation_node_uint64_create(pdst->node->id)))
		goto out_err_dlist;

	am_object_notation_node_list_add_item(
		n_list,
		(struct am_object_notation_node*)n_dst_id);

	if(!(n_dst_port = am_object_notation_node_string_create(pdst->type->name, 1)))
		goto out_err_dlist;

	am_object_notation_node_list_add_item(
		n_list,
		(struct am_object_notation_node*)n_dst_port);

	return (struct am_object_notation_node*)n_list;

out_err_dlist:
	am_object_notation_node_destroy((struct am_object_notation_node*)n_list);
out_err:
	return NULL;
}

/* Convert a graph into its representation in object notation. Returns a newly
 * create object notation node of the form:
 *
 *    am_dfg_graph {
 *       nodes : [
 *          <node_type> {
 *             id: <integer>,
 *             ...
 *          },
 *          <node_type> {
 *             id: <integer>,
 *             ...
 *          },
 *          ...
 *      ],
 *
 *      connections: [
 *         [<id>, <port_name>, <id>, <port_name>],
 *         [<id>, <port_name>, <id>, <port_name>],
 *         ...
 *      ]
 *    }
 *
 * In case of an error NULL is returned.
 */
struct am_object_notation_node*
am_dfg_graph_to_object_notation(const struct am_dfg_graph* g)
{
	struct am_object_notation_node_group* n_graph;
	struct am_object_notation_node_member* n_memb_nodes;
	struct am_object_notation_node_list* n_nodes;
	struct am_object_notation_node_member* n_memb_connections;
	struct am_object_notation_node_list* n_connections;
	struct am_object_notation_node* n_connection;
	struct am_object_notation_node* n_node;
	struct am_dfg_node* n;
	struct am_dfg_port* p;

	if(!(n_graph = am_object_notation_node_group_create("am_dfg_graph")))
		goto out_err;

	if(!(n_nodes = am_object_notation_node_list_create()))
		goto out_err_dest;

	if(!(n_memb_nodes = am_object_notation_node_member_create(
		     "nodes",
		     (struct am_object_notation_node*)n_nodes)))
	{
		am_object_notation_node_destroy(
			(struct am_object_notation_node*)n_nodes);
		free(n_nodes);
		goto out_err_dest;
	}

	am_object_notation_node_group_add_member(n_graph, n_memb_nodes);

	if(!(n_connections = am_object_notation_node_list_create()))
		goto out_err_dest;

	if(!(n_memb_connections = am_object_notation_node_member_create(
		     "connections",
		     (struct am_object_notation_node*)n_connections)))
	{
		am_object_notation_node_destroy(
			(struct am_object_notation_node*)n_connections);
		free(n_connections);
		goto out_err_dest;
	}

	am_object_notation_node_group_add_member(n_graph, n_memb_connections);

	am_dfg_graph_for_each_node(g, n) {
		if(!(n_node = am_dfg_node_to_object_notation(n)))
			goto out_err_dest;

		am_object_notation_node_list_add_item(n_nodes, n_node);

		am_dfg_node_for_each_input_port(n, p) {
			if(p->num_connections > 0) {
				if(!(n_connection = am_dfg_graph_connection_to_object_notation(p->connections[0], p)))
					goto out_err_dest;

				am_object_notation_node_list_add_item(n_connections, n_connection);
			}
		}
	}

	return (struct am_object_notation_node*)n_graph;

out_err_dest:
	am_object_notation_node_destroy((struct am_object_notation_node*)n_graph);
	free(n_graph);
out_err:
	return NULL;
}

/* Saves a graph to a file using object notation. Returns 0 on success,
 * otherwise 1. */
int am_dfg_graph_save_fp(struct am_dfg_graph* g, FILE* fp)
{
	struct am_object_notation_node* n_graph;

	if(!(n_graph = am_dfg_graph_to_object_notation(g)))
		return 1;

	if(am_object_notation_save_fp(n_graph, fp)) {
		am_object_notation_node_destroy(n_graph);
		return 1;
	}

	am_object_notation_node_destroy(n_graph);

	return 0;
}

/* Saves a graph to a file using object notation. The file will be overwritten
 * or created if necessary. Returns 0 on success, otherwise 1. */
int am_dfg_graph_save(struct am_dfg_graph* g, const char* filename)
{
	FILE* fp = fopen(filename, "w+");
	int ret = 0;

	if(!fp)
		return 1;

	ret = am_dfg_graph_save_fp(g, fp);
	fclose(fp);

	return ret;
}

/* Connects two nodes of a graph g based on the connection description
 * n_connection in object notation. The description must be a list of the
 * form:
 *
 *   [<src_id>, <src_port_name>, <dst_id>, <dst_port_name>],
 *
 * Where src_id and dst_id are integers referring to the IDs of the source and
 * destination node. src_port_name and dst_port_name are strings that refer to
 * the names of the output port of the source node and the input port of the
 * destination node, respectively.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_dfg_graph_connection_from_object_notation(
	struct am_dfg_graph* g,
	struct am_dfg_type_registry* tr,
	struct am_object_notation_node* n_connection)
{
	struct am_object_notation_node_list* n_lconnection;
	struct am_object_notation_node* item;
	struct am_object_notation_node* items[4];
	struct am_object_notation_node* n_src_id;
	struct am_object_notation_node_uint64* n_src_iid;
	struct am_object_notation_node* n_src_port;
	struct am_object_notation_node_string* n_src_sport;
	struct am_object_notation_node* n_dst_id;
	struct am_object_notation_node_uint64* n_dst_iid;
	struct am_object_notation_node* n_dst_port;
	struct am_object_notation_node_string* n_dst_sport;
	struct am_dfg_node* src;
	struct am_dfg_node* dst;
	struct am_dfg_port* psrc;
	struct am_dfg_port* pdst;
	size_t i = 0;

	if(n_connection->type !=  AM_OBJECT_NOTATION_NODE_TYPE_LIST)
		return 1;

	n_lconnection = (struct am_object_notation_node_list*)n_connection;

	am_object_notation_for_each_list_item(n_lconnection, item) {
		if(i > 3)
			return 1;

		items[i] = item;
		i++;
	}

	if(i != 4)
		return 1;

	n_src_id = items[0];
	n_src_port = items[1];
	n_dst_id = items[2];
	n_dst_port = items[3];

	if(n_src_id->type != AM_OBJECT_NOTATION_NODE_TYPE_UINT64 ||
	   n_src_port->type != AM_OBJECT_NOTATION_NODE_TYPE_STRING ||
	   n_dst_id->type != AM_OBJECT_NOTATION_NODE_TYPE_UINT64 ||
	   n_dst_port->type != AM_OBJECT_NOTATION_NODE_TYPE_STRING)
	{
		return 1;
	}

	n_src_iid = (struct am_object_notation_node_uint64*)n_src_id;
	n_src_sport = (struct am_object_notation_node_string*)n_src_port;
	n_dst_iid = (struct am_object_notation_node_uint64*)n_dst_id;
	n_dst_sport = (struct am_object_notation_node_string*)n_dst_port;

	if(!(src = am_dfg_graph_find_node(g, n_src_iid->value)))
		return 1;

	if(!(dst = am_dfg_graph_find_node(g, n_dst_iid->value)))
		return 1;

	if(!(psrc = am_dfg_node_find_port(src, n_src_sport->value)))
		return 1;

	if(!(pdst = am_dfg_node_find_port(dst, n_dst_sport->value)))
		return 1;

	if(am_dfg_graph_connectp(g, tr, psrc, pdst))
		return 1;

	return 0;
}

/* Connects nodes of a graph g according to a list of connections specified in
 * object notation. The object notation ode n_connections must refer to a list
 * of connection each matching the requirements for a connection in object
 * notation of am_dfg_graph_connection_from_object_notation.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_dfg_graph_connections_from_object_notation(
	struct am_dfg_graph* g,
	struct am_dfg_type_registry* tr,
	struct am_object_notation_node* n_connections)
{
	struct am_object_notation_node_list* n_lconnections;
	struct am_object_notation_node* item;

	if(n_connections->type !=  AM_OBJECT_NOTATION_NODE_TYPE_LIST)
		return 1;

	n_lconnections = (struct am_object_notation_node_list*)n_connections;

	am_object_notation_for_each_list_item(n_lconnections, item) {
		if(am_dfg_graph_connection_from_object_notation(g, tr, item))
			return 1;
	}

	return 0;
}

/* Adds all nodes specified as a list of object notation graph node objects
 * n_nodes to a graph g. Nodes are instantiated using the node type registry
 * ntr.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_dfg_graph_nodes_from_object_notation(
	struct am_dfg_graph* g,
	struct am_object_notation_node* n_nodes,
	struct am_dfg_node_type_registry* ntr)
{
	struct am_object_notation_node_list* n_lnodes;
	struct am_object_notation_node* item;
	struct am_dfg_node* n;

	if(n_nodes->type != AM_OBJECT_NOTATION_NODE_TYPE_LIST)
		return 1;

	n_lnodes = (struct am_object_notation_node_list*)n_nodes;

	am_object_notation_for_each_list_item(n_lnodes, item) {
		n = am_dfg_node_type_registry_node_from_object_notation(ntr, item);

		if(!n)
			return 1;

		if(am_dfg_graph_add_node(g, n)) {
			am_dfg_node_destroy(n);
			return 1;
		}
	}

	return 0;
}

/* Adds all nodes and connections of a graph specified as an object notation
 * object n_graph to a graph g. Nodes are instantiated using the node type
 * registry ntr.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_dfg_graph_from_object_notation(struct am_dfg_graph* g,
				      struct am_object_notation_node* n_graph,
				      struct am_dfg_type_registry* tr,
				      struct am_dfg_node_type_registry* ntr)
{
	struct am_object_notation_node_group* n_ggraph;
	struct am_object_notation_node* n_nodes;
	struct am_object_notation_node* n_connections;
	struct am_dfg_graph tmp;

	am_dfg_graph_init(&tmp, g->flags);

	if(n_graph->type != AM_OBJECT_NOTATION_NODE_TYPE_GROUP)
		return 1;

	n_ggraph = (struct am_object_notation_node_group*)n_graph;

	if(strcmp(n_ggraph->name, "am_dfg_graph") != 0)
		return 1;

	if(!am_object_notation_node_group_has_at_least_members(n_ggraph,
						      "nodes", "connections",
						      NULL))
	{
		return 1;
	}

	n_nodes = am_object_notation_node_group_get_member_def(n_ggraph,
							       "nodes");

	if(am_dfg_graph_nodes_from_object_notation(&tmp, n_nodes, ntr))
		return 1;

	n_connections = am_object_notation_node_group_get_member_def(
		n_ggraph,
		"connections");

	if(am_dfg_graph_connections_from_object_notation(&tmp, tr, n_connections))
		return 1;

	if(am_dfg_graph_merge(g, &tmp)) {
		tmp.flags = AM_DFG_GRAPH_DESTROY_ALL;
		am_dfg_graph_destroy(&tmp);
		return 1;
	}

	return 0;
}

/* Opens the file in object notation specified by filename and adds the nodes
 * and connections from the file to the graph g. Nodes are instantiated using
 * the node type registry ntr.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_dfg_graph_load(struct am_dfg_graph* g,
		      const char* filename,
		      struct am_dfg_type_registry* tr,
		      struct am_dfg_node_type_registry* ntr)
{
	struct am_object_notation_node* n_graph;
	int ret = 1;

	if(!(n_graph = am_object_notation_load(filename)))
		goto out_err;

	if(am_dfg_graph_from_object_notation(g, n_graph, tr, ntr))
		goto out_err_dest;

	ret = 0;

out_err_dest:
	am_object_notation_node_destroy(n_graph);
out_err:
	return ret;
}
