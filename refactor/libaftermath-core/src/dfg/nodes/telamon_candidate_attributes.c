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

#include <aftermath/core/dfg/nodes/telamon_candidate_attributes.h>
#include <aftermath/core/in_memory.h>
#include <aftermath/core/telamon.h>

int am_dfg_telamon_candidate_attributes_node_process(struct am_dfg_node* n)
{
	struct am_dfg_port* pcandidate_in = &n->ports[0];
	struct am_dfg_port* paction_out = &n->ports[1];
	struct am_dfg_port* pexploration_time_out = &n->ports[2];
	struct am_dfg_port* prollout_time_out = &n->ports[3];
	struct am_dfg_port* pdeadend_time_out = &n->ports[4];
	struct am_dfg_port* pperfmodel_bound_out = &n->ports[5];
	struct am_dfg_port* pperfmodel_bound_valid_out = &n->ports[6];
	struct am_telamon_candidate** candidate_in;
	struct am_telamon_candidate* candidate;
	int bool_val;
	char* str_val;

	if(!am_dfg_port_activated_and_has_data(pcandidate_in))
		return 0;

	if(am_dfg_port_activated(paction_out)) {
		candidate_in = pcandidate_in->buffer->data;

		for(size_t i = 0; i < pcandidate_in->buffer->num_samples; i++) {
			candidate = candidate_in[i];

			if(!(str_val = strdup(candidate->action)))
				return 1;

			if(am_dfg_buffer_write(
				   paction_out->buffer,
				   1,
				   &str_val))
			{
				free(str_val);
				return 1;
			}
		}
	}

	if(am_dfg_port_activated(pexploration_time_out)) {
		candidate_in = pcandidate_in->buffer->data;

		for(size_t i = 0; i < pcandidate_in->buffer->num_samples; i++) {
			candidate = candidate_in[i];

			if(am_dfg_buffer_write(pexploration_time_out->buffer,
					       1,
					       &candidate->exploration_time))
			{
				return 1;
			}
		}
	}

	if(am_dfg_port_activated(prollout_time_out)) {
		candidate_in = pcandidate_in->buffer->data;

		for(size_t i = 0; i < pcandidate_in->buffer->num_samples; i++) {
			candidate = candidate_in[i];

			if(am_dfg_buffer_write(prollout_time_out->buffer,
					       1,
					       &candidate->rollout_time))
			{
				return 1;
			}
		}
	}

	if(am_dfg_port_activated(pdeadend_time_out)) {
		candidate_in = pcandidate_in->buffer->data;

		for(size_t i = 0; i < pcandidate_in->buffer->num_samples; i++) {
			candidate = candidate_in[i];

			if(am_dfg_buffer_write(pdeadend_time_out->buffer,
					       1,
					       &candidate->deadend_time))
			{
				return 1;
			}
		}
	}

	if(am_dfg_port_activated(pperfmodel_bound_out)) {
		candidate_in = pcandidate_in->buffer->data;

		for(size_t i = 0; i < pcandidate_in->buffer->num_samples; i++) {
			candidate = candidate_in[i];

			if(am_dfg_buffer_write(pperfmodel_bound_out->buffer,
					       1,
					       &candidate->perfmodel_bound))
			{
				return 1;
			}
		}
	}

	if(am_dfg_port_activated(pperfmodel_bound_valid_out)) {
		candidate_in = pcandidate_in->buffer->data;

		for(size_t i = 0; i < pcandidate_in->buffer->num_samples; i++) {
			candidate = candidate_in[i];

			bool_val = am_telamon_candidate_perfmodel_bound_valid(
				candidate);

			if(am_dfg_buffer_write(
				   pperfmodel_bound_valid_out->buffer,
				   1,
				   &bool_val))
			{
				return 1;
			}
		}
	}

	return 0;
}
