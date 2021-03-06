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

#ifndef AM_STATEFUL_COLOR_MAP_H
#define AM_STATEFUL_COLOR_MAP_H

#include <aftermath/render/cairo_extras.h>
#include <aftermath/core/typed_array.h>
#include <aftermath/core/ansi_extras.h>
#include <aftermath/render/color_map.h>
#include <pthread.h>

extern struct am_color_map static_colors;
extern pthread_mutex_t lock; // TODO is this necessary?

#define SCM_CAPACITY_CHUNK_SIZE 128

/* all hard-coded for now */
struct am_stateful_color_map_indexer {

	size_t capacity;
	size_t num_colors_allocated;
	size_t* allocated_colors;
	uint64_t* allocated_identifiers;

};

extern struct am_stateful_color_map_indexer stateful_cmi;

static inline void am_stateful_color_map_indexer_init(
	struct am_stateful_color_map_indexer*	stateful_color_map)
{
	stateful_color_map->capacity = SCM_CAPACITY_CHUNK_SIZE;
	stateful_color_map->num_colors_allocated = 0;
	
	stateful_color_map->allocated_colors = (size_t*)
		malloc((stateful_color_map->capacity)*sizeof(size_t));
	
	stateful_color_map->allocated_identifiers = (uint64_t*)
		malloc((stateful_color_map->capacity)*sizeof(uint64_t));

	if(pthread_mutex_init(&lock, NULL) != 0)
	{
		fprintf(stderr,"Mutex init failed\n");
		exit(1);
	}

}

// TODO add a function to destroy and reset the allocations
static inline size_t am_stateful_color_map_indexer_get_color_index(uint64_t identifier)
{
	pthread_mutex_lock(&lock);
	// has this identifier already been allocated a color?
	for(size_t i=0; i<stateful_cmi.num_colors_allocated; i++){
		// TODO should be a hashmap for fast access
		if(stateful_cmi.allocated_identifiers[i] == identifier){
			pthread_mutex_unlock(&lock);
			return stateful_cmi.allocated_colors[i];
		}
	}

	// if here, then we need to allocate a new color
	if(stateful_cmi.num_colors_allocated == stateful_cmi.capacity){
		// must expand the array
		size_t* new_allocated_colors = (size_t*)
			malloc((stateful_cmi.capacity + SCM_CAPACITY_CHUNK_SIZE)*sizeof(size_t));

		uint64_t* new_allocated_identifiers = (uint64_t*)
			malloc((stateful_cmi.capacity + SCM_CAPACITY_CHUNK_SIZE)*sizeof(uint64_t));

		// copy the old memory across to the new memory
		memcpy(new_allocated_colors, stateful_cmi.allocated_colors, stateful_cmi.num_colors_allocated * sizeof(size_t));
		memcpy(new_allocated_identifiers, stateful_cmi.allocated_identifiers, stateful_cmi.num_colors_allocated * sizeof(uint64_t));

		// free the old memory
		free(stateful_cmi.allocated_colors);
		free(stateful_cmi.allocated_identifiers);

		// replace the pointers to my new memory
		stateful_cmi.allocated_colors = new_allocated_colors;
		stateful_cmi.allocated_identifiers = new_allocated_identifiers;

		stateful_cmi.capacity += SCM_CAPACITY_CHUNK_SIZE;
	}

	// now assign a new color index
	size_t next_color_idx = (stateful_cmi.num_colors_allocated % (static_colors.num_elements-1)) + 1; // first color is transparent

	// update the state so we can re-use it next time
	stateful_cmi.allocated_colors[stateful_cmi.num_colors_allocated] = next_color_idx;
	stateful_cmi.allocated_identifiers[stateful_cmi.num_colors_allocated] = identifier;
	stateful_cmi.num_colors_allocated++;

	pthread_mutex_unlock(&lock);
	return next_color_idx;
}

#endif

