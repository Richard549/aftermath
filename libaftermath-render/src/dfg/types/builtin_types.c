/**
 * Author: Andi Drebes <andi@drebesium.org>
 * Author: Igor Wodiany <igor.wodiany@manchester.ac.uk>
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

#include <aftermath/core/dfg_builtin_type_impl.h>
#include <aftermath/core/dfg/types/generic.h>
#include <aftermath/render/cairo_extras.h>
#include <aftermath/render/dfg/types/builtin_types.h>
#include <aftermath/render/dfg/timeline_layer_common.h>
#include <aftermath/render/timeline/layer.h>

AM_DFG_DECL_BUILTIN_TYPE(
	am_render_dfg_type_timeline_layer,
	"const am::render::timeline::layer*",
	sizeof(struct am_timeline_render_layer*),
	NULL,
	am_dfg_type_generic_plain_copy_samples,
	NULL, NULL, NULL)

#undef DEFS_NAME
#define DEFS_NAME() am_render_dfg_type_rgba_defs
#include <aftermath/render/dfg/types/rgba.h>

AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(axes, "axes")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(background, "background")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(hierarchy, "hierarchy")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(measurement_intervals, "measurement_intervals")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(stack_frame_period, "core::stack_frame_period")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(function_symbol, "core::function_symbol")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_for_loop_type, "openmp::for_loop_type")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_for_loop_instance, "openmp::for_loop_instance")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_iteration_set, "openmp::iteration_set")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_iteration_period, "openmp::iteration_period")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_task_type, "openmp::task_type")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_task_instance, "openmp::task_instance")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_task_period, "openmp::task_period")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_thread, "openmp::thread")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_parallel, "openmp::parallel")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_task_create, "openmp::task_create")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_task_schedule, "openmp::task_schedule")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_implicit_task, "openmp::implicit_task")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_sync_region_wait, "openmp::sync_region_wait")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_mutex_released, "openmp::mutex_released")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_dependences, "openmp::dependences")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_task_dependence, "openmp::task_dependence")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_work, "openmp::work")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_master, "openmp::master")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_sync_region, "openmp::sync_region")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_lock_init, "openmp::lock_init")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_lock_destroy, "openmp::lock_destroy")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_mutex_acquire, "openmp::mutex_acquire")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_mutex_acquired, "openmp::mutex_acquired")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_nest_lock, "openmp::nest_lock")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_flush, "openmp::flush")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(openmp_cancel, "openmp::cancel")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(selection, "selection")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(state, "state")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(telamon_evaluation, "telamon::evaluation")
AM_RENDER_DFG_DECL_TIMELINE_LAYER_TYPE(tensorflow_node_execution, "tensorflow::node_execution")

static struct am_dfg_static_type_def* builtin_defs[] = {
	&am_render_dfg_type_timeline_layer,
	&am_render_dfg_type_timeline_axes_layer,
	&am_render_dfg_type_timeline_background_layer,
	&am_render_dfg_type_timeline_hierarchy_layer,
	&am_render_dfg_type_timeline_measurement_intervals_layer,
	&am_render_dfg_type_timeline_stack_frame_period_layer,
	&am_render_dfg_type_timeline_function_symbol_layer,
	&am_render_dfg_type_timeline_openmp_for_loop_type_layer,
	&am_render_dfg_type_timeline_openmp_for_loop_instance_layer,
	&am_render_dfg_type_timeline_openmp_iteration_set_layer,
	&am_render_dfg_type_timeline_openmp_iteration_period_layer,
	&am_render_dfg_type_timeline_openmp_task_type_layer,
	&am_render_dfg_type_timeline_openmp_task_instance_layer,
	&am_render_dfg_type_timeline_openmp_task_period_layer,
	&am_render_dfg_type_timeline_openmp_thread_layer,
	&am_render_dfg_type_timeline_openmp_parallel_layer,
	&am_render_dfg_type_timeline_openmp_task_create_layer,
	&am_render_dfg_type_timeline_openmp_task_schedule_layer,
	&am_render_dfg_type_timeline_openmp_implicit_task_layer,
	&am_render_dfg_type_timeline_openmp_sync_region_wait_layer,
	&am_render_dfg_type_timeline_openmp_mutex_released_layer,
	&am_render_dfg_type_timeline_openmp_dependences_layer,
	&am_render_dfg_type_timeline_openmp_task_dependence_layer,
	&am_render_dfg_type_timeline_openmp_work_layer,
	&am_render_dfg_type_timeline_openmp_master_layer,
	&am_render_dfg_type_timeline_openmp_sync_region_layer,
	&am_render_dfg_type_timeline_openmp_lock_init_layer,
	&am_render_dfg_type_timeline_openmp_lock_destroy_layer,
	&am_render_dfg_type_timeline_openmp_mutex_acquire_layer,
	&am_render_dfg_type_timeline_openmp_mutex_acquired_layer,
	&am_render_dfg_type_timeline_openmp_nest_lock_layer,
	&am_render_dfg_type_timeline_openmp_flush_layer,
	&am_render_dfg_type_timeline_openmp_cancel_layer,
	&am_render_dfg_type_timeline_selection_layer,
	&am_render_dfg_type_timeline_state_layer,
	&am_render_dfg_type_timeline_telamon_evaluation_layer,
	&am_render_dfg_type_timeline_tensorflow_node_execution_layer,
	NULL
};

static struct am_dfg_static_type_def** defsets[] = {
	builtin_defs,
	am_render_dfg_type_rgba_defs,
	NULL
};

/* Registers all builtin types for libaftermath-render.
 *
 * Returns 0 on success, otherwise 1.
 */
int am_render_dfg_builtin_types_register(struct am_dfg_type_registry* tr)
{
	return am_dfg_type_registry_add_static(tr, defsets);
}
