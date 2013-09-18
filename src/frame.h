/**
 * Copyright (C) 2013 Andi Drebes <andi.drebes@lip6.fr>
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

#ifndef FRAME_H
#define FRAME_H

#define _GNU_SOURCE
#include <stdint.h>
#include <malloc.h>
#include <search.h>
#include <sys/types.h>

void tdestroy(void *root, void (*free_node)(void *nodep));

struct frame {
	uint64_t addr;
	unsigned int num_pushes;
	unsigned int num_steals;
	int32_t numa_node;
	uint32_t size;
	struct single_event* first_texec_start;
	struct comm_event* first_write;
	struct comm_event* first_max_write;
};

struct frame_tree {
	void* root;
	int num_frames;
};

static inline void frame_tree_init(struct frame_tree* ft)
{
	ft->root = NULL;
	ft->num_frames = 0;
}

int compare_frames(const void *pt1, const void *pt2);
int compare_framesp(const void *pt1, const void *pt2);

static inline struct frame* frame_tree_find(struct frame_tree* ft, uint64_t addr)
{
	struct frame key = { .addr = addr };
	struct frame** pret = tfind(&key, &ft->root, compare_frames);

	if(pret)
		return *pret;

	return NULL;
}

static inline struct frame* frame_tree_add(struct frame_tree* ft, uint64_t addr)
{
	struct frame* key = malloc(sizeof(struct frame));
	struct frame** f;

	if(!key)
		return NULL;

	key->addr = addr;

	if((f = tsearch(key, &ft->root, compare_frames)) == NULL) {
		free(key);
		return NULL;
	}

	(*f)->num_steals = 0;
	(*f)->num_pushes = 0;
	(*f)->numa_node = -1;
	(*f)->first_texec_start = NULL;
	(*f)->first_max_write = NULL;
	(*f)->first_write = NULL;
	(*f)->size = 0;

	ft->num_frames++;

	return key;
}

void frame_tree_walk(const void* p, const VISIT which, const int depth);
int frame_tree_to_array(struct frame_tree* ft, struct frame** arr);

static inline void frame_tree_destroy(struct frame_tree* ft)
{
	tdestroy(ft->root, free);
}

#endif
