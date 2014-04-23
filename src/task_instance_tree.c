/**
 * Copyright (C) 2014 Andi Drebes <andi.drebes@lip6.fr>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "task_instance_tree.h"

void task_instance_tree_init(struct task_instance_tree* t)
{
	t->root = RB_ROOT;
}

int task_instance_tree_insert(struct task_instance_tree* t, struct task_instance* n)
{
	struct rb_node** new = &t->root.rb_node;
	struct rb_node* parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct task_instance* this = container_of(*new, struct task_instance, rb_all_instances);
		parent = *new;

		if (n->start < this->start ||
		    (n->start == this->start &&
		     n->task->addr < this->task->addr))
		{
			new = &((*new)->rb_left);
		} else if (n->start > this->start ||
			   (n->start == this->start &&
			    n->task->addr > this->task->addr))
		{
			new = &((*new)->rb_right);
		} else {
			return 1;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&n->rb_all_instances, parent, new);
	rb_insert_color(&n->rb_all_instances, &t->root);

	return 0;
}

void task_instance_tree_remove(struct task_instance_tree* t, struct task_instance* n)
{
	rb_erase(&n->rb_all_instances, &t->root);
}


struct task_instance* task_instance_tree_iter_first(struct task_instance_tree* t)
{
	struct rb_node* node = rb_first(&t->root);

	if(!node)
		return NULL;

	struct task_instance* this_node = rb_entry(node, struct task_instance, rb_all_instances);
	return this_node;
}

struct task_instance* task_instance_tree_iter_next(struct task_instance* n)
{
	struct rb_node* node = rb_next(&n->rb_all_instances);

	if(!node)
		return NULL;

	struct task_instance* this_node = rb_entry(node, struct task_instance, rb_all_instances);
	return this_node;
}

struct task_instance* task_instance_tree_iter_first_postorder(struct task_instance_tree* t)
{
	struct rb_node* node = rb_first_postorder(&t->root);

	if(!node)
		return NULL;

	struct task_instance* this_node = rb_entry(node, struct task_instance, rb_all_instances);
	return this_node;
}

struct task_instance* task_instance_tree_iter_next_postorder(struct task_instance* n)
{
	struct rb_node* node = rb_next_postorder(&n->rb_all_instances);

	if(!node)
		return NULL;

	struct task_instance* this_node = rb_entry(node, struct task_instance, rb_all_instances);
	return this_node;
}