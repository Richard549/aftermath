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
#include "signals.h"
#include "globals.h"
#include "trace_widget.h"
#include "histogram_widget.h"
#include "multi_histogram_widget.h"
#include "color.h"
#include "matrix_widget.h"
#include "util.h"
#include "dialogs.h"
#include "task_list.h"
#include "frame_list.h"
#include "numa_node_list.h"
#include "counter_list.h"
#include "state_list.h"
#include "ansi_extras.h"
#include "derived_counters.h"
#include "statistics.h"
#include "page.h"
#include "visuals_file.h"
#include "cpu_list.h"
#include "task_graph.h"
#include "export.h"
#include "address_range_tree.h"
#include "filter_expression.h"
#include "omp_for.h"
#include "omp_for_treeview.h"
#include "omp_task_treeview.h"
#include <gtk/gtk.h>
#include <inttypes.h>
#include <stdio.h>
#include <limits.h>
#include <libgen.h>

int build_address_range_tree(void);
void update_statistics(void);

enum comm_matrix_mode {
	COMM_MATRIX_MODE_NODE = 0,
	COMM_MATRIX_MODE_CPU
};

void reset_zoom(void)
{
	uint64_t start = multi_event_set_first_event_start(&g_mes);
	uint64_t end = multi_event_set_last_event_end(&g_mes);

	printf("ZOOM TO %"PRIu64" %"PRIu64"\n", start, end);

	gtk_trace_set_bounds(g_trace_widget, start, end);
	trace_bounds_changed(GTK_TRACE(g_trace_widget), (double)start, (double)end, NULL);
}

void update_indexes(void)
{
	struct event_set* es;

	for_each_event_set(&g_mes, es)
		event_set_update_state_index(es, &g_filter);
}

/**
 * Connected to the "toggled" signal of a checkbutton, widget_toggle()
 * enables / disables another widget specified as the data parameter.
 */
G_MODULE_EXPORT gint widget_toggle(gpointer data, GtkWidget* check)
{
	GtkWidget* dependent = data;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)))
		gtk_widget_set_sensitive(dependent, 1);
	else
		gtk_widget_set_sensitive(dependent, 0);

	return 0;
}

G_MODULE_EXPORT void toolbar_fit_all_cpus_clicked(GtkButton *button, gpointer data)
{
	gtk_trace_fit_all_cpus(g_trace_widget);
}

enum measurement_interval_find_status {
	MEASUREMENT_INTERVAL_FIND_OK = 0,
	MEASUREMENT_INTERVAL_FIND_END_BEFORE_START,
	MEASUREMENT_INTERVAL_FIND_START_AFTER_START,
	MEASUREMENT_INTERVAL_FIND_NO_START,
	MEASUREMENT_INTERVAL_FIND_NO_END,
};

enum measurement_interval_find_status
find_first_measurement_interval_start_and_last_measurement_interval_end(struct global_single_event** first_start,
									struct global_single_event** last_end)
{
	struct global_single_event* _first_start = NULL;
	struct global_single_event* _last_end = NULL;

	for(int i = 0; i < g_mes.num_global_single_events; i++) {
		struct global_single_event* gse = &g_mes.global_single_events[i];

		if(!_first_start && gse->type == GLOBAL_SINGLE_TYPE_MEASURE_START) {
			_first_start = gse;
			continue;
		}

		if(_first_start && gse->type == GLOBAL_SINGLE_TYPE_MEASURE_END) {
			_last_end = gse;
			continue;
		}

		if(!_first_start && gse->type == GLOBAL_SINGLE_TYPE_MEASURE_END)
			return MEASUREMENT_INTERVAL_FIND_END_BEFORE_START;

		if(_first_start && gse->type == GLOBAL_SINGLE_TYPE_MEASURE_START)
			return MEASUREMENT_INTERVAL_FIND_START_AFTER_START;
	}

	if(!_first_start)
		return MEASUREMENT_INTERVAL_FIND_NO_START;

	if(_first_start && !_last_end)
		return MEASUREMENT_INTERVAL_FIND_NO_END;

	*first_start = _first_start;
	*last_end = _last_end;

	return MEASUREMENT_INTERVAL_FIND_OK;
}

void measurement_interval_find_show_error(enum measurement_interval_find_status s)
{
	switch(s) {
		case MEASUREMENT_INTERVAL_FIND_OK:
			break;
		case MEASUREMENT_INTERVAL_FIND_END_BEFORE_START:
			show_error_message("Error: found measurement end before measurement start event.\n");
			break;
		case MEASUREMENT_INTERVAL_FIND_START_AFTER_START:
			show_error_message("Error: found two consecutive measurement start events.\n");
			break;
		case MEASUREMENT_INTERVAL_FIND_NO_START:
			show_error_message("Error: could not find any measurement interval start.\n");
			break;
		case MEASUREMENT_INTERVAL_FIND_NO_END:
			show_error_message("Error: could not find any measurement interval end.\n");
			break;
	}
}

G_MODULE_EXPORT void toolbar_fit_all_measurement_intervals_clicked(GtkButton *button, gpointer data)
{
	struct global_single_event* first_start = NULL;
	struct global_single_event* last_end = NULL;
	enum measurement_interval_find_status st;

	st = find_first_measurement_interval_start_and_last_measurement_interval_end(&first_start, &last_end);

	if(st == MEASUREMENT_INTERVAL_FIND_OK)
		gtk_trace_set_bounds(g_trace_widget, first_start->time, last_end->time);
	else
		measurement_interval_find_show_error(st);
}

G_MODULE_EXPORT void toolbar_zoom100_clicked(GtkButton *button, gpointer data)
{
	reset_zoom();
}

G_MODULE_EXPORT void toolbar_rewind_clicked(GtkButton *button, gpointer data)
{
	uint64_t start = multi_event_set_first_event_start(&g_mes);
	gtk_trace_set_left(g_trace_widget, start);
}

G_MODULE_EXPORT void toolbar_ffwd_clicked(GtkButton *button, gpointer data)
{
	uint64_t end = multi_event_set_last_event_end(&g_mes);
	gtk_trace_set_right(g_trace_widget, end);
}

G_MODULE_EXPORT void use_global_values_toggled(GtkToggleButton *button, gpointer data)
{
	widget_toggle(g_global_values_min_entry, GTK_WIDGET(button));
	widget_toggle(g_global_values_max_entry, GTK_WIDGET(button));
}

G_MODULE_EXPORT void use_global_slopes_toggled(GtkToggleButton *button, gpointer data)
{
	widget_toggle(g_global_slopes_min_entry, GTK_WIDGET(button));
	widget_toggle(g_global_slopes_max_entry, GTK_WIDGET(button));
}

G_MODULE_EXPORT void use_task_length_check_toggle(GtkToggleButton *button, gpointer data)
{
	widget_toggle(g_task_length_min_entry, GTK_WIDGET(button));
	widget_toggle(g_task_length_max_entry, GTK_WIDGET(button));
}

G_MODULE_EXPORT void use_comm_size_check_toggle(GtkToggleButton *button, gpointer data)
{
	widget_toggle(g_comm_size_min_entry, GTK_WIDGET(button));
	widget_toggle(g_comm_size_max_entry, GTK_WIDGET(button));
}

G_MODULE_EXPORT void toolbar_draw_comm_size_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_comm_size(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_steals_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_steals(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_pushes_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_pushes(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_data_reads_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_data_reads(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_data_writes_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_data_writes(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_single_events_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_single_events(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_counters_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_counters(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_annotations_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_annotations(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_measurement_intervals_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_measurement_intervals(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

void update_predecessors(void)
{
	int num_total;

	gtk_trace_reset_highlighted_predecessors(g_trace_widget);

	if(!g_address_range_tree_built)
		if(build_address_range_tree())
			return;

	g_predecessors = realloc(g_predecessors, g_predecessor_max_depth * sizeof(struct list_head));
	g_num_predecessors = realloc(g_num_predecessors, g_predecessor_max_depth * sizeof(int));

	struct state_event* se = gtk_trace_get_highlighted_state_event(g_trace_widget);

	if(se && se->texec_start) {
		struct task_instance* inst = task_instance_tree_find(&g_address_range_tree.all_instances,
								     se->texec_start->active_task->addr,
								     se->event_set->cpu,
								     se->texec_start->time);

		if(!inst) {
			fprintf(stderr, "Warning: Could not find task instance for task %s @ %"PRIu64", cpu %d\n",
				se->texec_start->active_task->symbol_name,
				se->texec_start->time, se->event_set->cpu);
		} else {
			address_range_tree_get_predecessors(&g_address_range_tree, inst, g_predecessor_max_depth, g_predecessors, g_num_predecessors, &num_total);
			gtk_trace_set_highlighted_predecessors(g_trace_widget, g_predecessors, g_num_predecessors, g_predecessor_max_depth);
		}
	}
}

G_MODULE_EXPORT void toolbar_draw_draw_predecessors_toggled(GtkToggleToolButton *button, gpointer data)
{
	g_draw_predecessors = gtk_toggle_tool_button_get_active(button);

	if(g_draw_predecessors)
		update_predecessors();
	else
		gtk_trace_reset_highlighted_predecessors(g_trace_widget);
}

void heatmap_update_params(void)
{
	int num_shades;
	num_shades = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_heatmap_num_shades));
	gtk_trace_set_heatmap_num_shades(g_trace_widget, num_shades);
}

int task_length_heatmap_update_params(void)
{
	const char* txt;
	uint64_t min_length;
	uint64_t max_length;

	txt = gtk_entry_get_text(GTK_ENTRY(g_heatmap_min_cycles));

	if(pretty_read_cycles(txt, &min_length)) {
		show_error_message("\"%s\" is not a correct number of cycles.", txt);
		return 1;
	}

	txt = gtk_entry_get_text(GTK_ENTRY(g_heatmap_max_cycles));

	if(pretty_read_cycles(txt, &max_length)) {
		show_error_message("\"%s\" is not a correct number of cycles.", txt);
		return 1;
	}

	gtk_trace_set_heatmap_task_length_bounds(g_trace_widget, min_length, max_length);

	heatmap_update_params();

	return 0;
}

G_MODULE_EXPORT void task_length_heatmap_update_params_clicked(GtkButton *button, gpointer data)
{
	task_length_heatmap_update_params();
}

G_MODULE_EXPORT void determine_automatically_button_clicked(GtkButton *button, gpointer data)
{
	GtkWidget *widget = GTK_WIDGET(g_trace_widget);
	char buffer[30];
	uint64_t max;
	uint64_t min;
	int64_t left, right;

	if(!gtk_trace_get_range_selection(widget, &left, &right)) {
		multi_event_event_set_get_min_time(&g_mes, &left);
		multi_event_event_set_get_max_time(&g_mes, &right);
	}

	multi_event_set_get_max_task_duration_in_interval(&g_mes, &g_filter, left, right, &max);
	multi_event_set_get_min_task_duration_in_interval(&g_mes, &g_filter, left, right, &min);

	snprintf(buffer, sizeof(buffer), "%"PRIu64, min);
	gtk_entry_set_text(GTK_ENTRY(g_heatmap_min_cycles), buffer);

	snprintf(buffer, sizeof(buffer), "%"PRIu64, max);
	gtk_entry_set_text(GTK_ENTRY(g_heatmap_max_cycles), buffer);
}

G_MODULE_EXPORT void tool_button_use_task_length_statemap_toggled(GtkToggleToolButton *button, gpointer data)
{
	int active = gtk_toggle_tool_button_get_active(button);

	if(active)
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_STATES);
}

G_MODULE_EXPORT void tool_button_use_typemap_toggled(GtkToggleToolButton *button, gpointer data)
{
	int active = gtk_toggle_tool_button_get_active(button);

	if(active)
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_TASK_TYPE);
}

G_MODULE_EXPORT void tool_button_use_task_length_heatmap_toggled(GtkToggleToolButton *button, gpointer data)
{
	int active = gtk_toggle_tool_button_get_active(button);

	if(active) {
		if(task_length_heatmap_update_params())
			gtk_toggle_tool_button_set_active(button, 0);
	}

	gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_HEAT_TASKLEN);
}

G_MODULE_EXPORT void tool_button_numamap_reads_toggled(GtkToggleToolButton *button, gpointer data)
{
	int active = gtk_toggle_tool_button_get_active(button);

	if(active)
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_NUMA_READS);
}

G_MODULE_EXPORT void tool_button_numamap_writes_toggled(GtkToggleToolButton *button, gpointer data)
{
	int active = gtk_toggle_tool_button_get_active(button);

	if(active)
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_NUMA_WRITES);
}

G_MODULE_EXPORT void tool_button_heatmap_numa_toggled(GtkToggleToolButton *button, gpointer data)
{
	int active = gtk_toggle_tool_button_get_active(button);

	heatmap_update_params();

	if(active)
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_HEAT_NUMA);
}

G_MODULE_EXPORT void tool_button_omp_for_toggled(GtkToggleToolButton *button, gpointer data)
{
        /* Restore last OMP mode */
        gtk_trace_set_map_mode(g_trace_widget, g_omp_map_mode);
}

G_MODULE_EXPORT void menubar_double_buffering_toggled(GtkCheckMenuItem *item, gpointer data)
{
	gtk_trace_set_double_buffering(g_trace_widget, gtk_check_menu_item_get_active(item));
}

G_MODULE_EXPORT void menubar_about(GtkMenuItem *item, gpointer data)
{
	show_about_dialog();
}

G_MODULE_EXPORT void menubar_settings(GtkCheckMenuItem *item, gpointer data)
{
	if(show_settings_dialog(&g_settings)) {
		if(write_user_settings(&g_settings) != 0)
			show_error_message("Could not write settings");
	}
}

G_MODULE_EXPORT void menubar_export_task_graph(GtkCheckMenuItem *item, gpointer data)
{
	char* filename;
	int64_t start, end;

	if(!gtk_trace_get_range_selection(g_trace_widget, &start, &end)) {
		show_error_message("No range selected.");
		return;
	}

	if(!(filename = load_save_file_dialog("Save task graph",
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      "GraphViz files",
					      "*.dot",
					      NULL)))
	{
		return;
	}

	if(!g_address_range_tree_built)
		if(build_address_range_tree())
			return;

	if(export_task_graph(filename, &g_mes, &g_address_range_tree, &g_filter, start, end)) {
		show_error_message("Could not save task graph to file %s.", filename);
	}

	free(filename);
}

G_MODULE_EXPORT void menubar_export_task_graph_selected_task_execution(GtkCheckMenuItem *item, gpointer data)
{
	char* filename;
	unsigned int depth_down;
	unsigned int depth_up;

	struct state_event* se = gtk_trace_get_highlighted_state_event(g_trace_widget);

	if(!se) {
		show_error_message("No event selected.");
		return;
	} else if(se->texec_start == NULL) {
		show_error_message("No task associated to selected event.");
		return;
	}

	if(!show_task_graph_texec_dialog(&depth_down, &depth_up))
		return;

	if(!(filename = load_save_file_dialog("Save task graph",
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      "GraphViz files",
					      "*.dot",
					      NULL)))
	{
		return;
	}

	if(!g_address_range_tree_built)
		if(build_address_range_tree())
			return;

	if(export_task_graph_selected_texec(filename, &g_mes, &g_address_range_tree, &g_filter, se->texec_start, depth_down, depth_up)) {
		show_error_message("Could not save task graph to file %s.", filename);
	}

	free(filename);
}

G_MODULE_EXPORT void menubar_add_derived_counter(GtkMenuItem *item, gpointer data)
{
	char buffer[128];
	char buffer_short[20];

	struct derived_counter_options opt;
	struct counter_description* cd;
	int err = 1;

	filter_init(&opt.task_filter, 0, 0, 0, 0, multi_event_get_max_cpu(&g_mes));
	bitvector_init(&opt.cpus, multi_event_get_max_cpu(&g_mes));

	if(show_derived_counter_dialog(&g_mes, &opt)) {
		switch(opt.type) {
			case DERIVED_COUNTER_PARALLELISM:
				err = derive_parallelism_counter(&g_mes, &cd, opt.name, opt.state, opt.num_samples, opt.cpu);
				break;
			case DERIVED_COUNTER_AGGREGATE:
				err = derive_aggregate_counter(&g_mes, &cd, opt.name, opt.counter_idx, opt.num_samples, opt.cpu);
				break;
			case DERIVED_COUNTER_NUMA_CONTENTION:
				err = derive_numa_contention_counter(&g_mes, &cd, opt.name, opt.numa_node, opt.data_direction, opt.contention_type, opt.contention_model, opt.exclude_node, opt.num_samples, opt.cpu);
				break;
			case DERIVED_COUNTER_RATIO:
				err = derive_ratio_counter(&g_mes, &cd, opt.name, opt.ratio_type, opt.counter_idx, opt.divcounter_idx, opt.num_samples, opt.cpu);
				break;
			case DERIVED_COUNTER_TASK_LENGTH:
				err = derive_task_length_counter(&g_mes, &cd, opt.name, &opt.cpus, &opt.task_filter, opt.num_samples, opt.cpu);
				break;
		}

		if(err)
			show_error_message("Could not create derived counter.");
		else {
			counter_list_append(GTK_TREE_VIEW(g_counter_treeview), cd, FALSE);
			strncpy(buffer, cd->name, sizeof(buffer));

			print_short(buffer_short, sizeof(buffer_short), cd->name);
			gtk_combo_box_append_text(GTK_COMBO_BOX(g_counter_list_widget), buffer_short);
		}

		free(opt.name);
	}

	filter_destroy(&opt.task_filter);
	bitvector_destroy(&opt.cpus);
}

G_MODULE_EXPORT void menubar_generate_counter_file(GtkMenuItem *item, gpointer data)
{
	FILE *file;
	char* filename;
	int nb_errors;
	struct counter_description* cd;
	int cpu;
	enum yes_no_cancel_dialog_response response;

	if(!multi_event_set_counters_monotonously_increasing(&g_mes, &g_filter, &cd, &cpu)) {
		show_error_message("Counter \"%s\" is not monotonously increasing on CPU %d\n", cd->name, cpu);
		return;
	}

	if (!multi_event_set_cpus_have_counters(&g_mes, &g_filter)) {
		response = show_yes_no_dialog("Some counters are not available on all CPUs. Continue anyway ?\n");

		if (response == DIALOG_RESPONSE_NO)
			return;
	}

	filename = load_save_file_dialog("Save counters values", GTK_FILE_CHOOSER_ACTION_SAVE, "TEXT files", "*.txt", NULL);

	if(!filename)
		return;

	if(!(file = fopen(filename, "w+"))) {
		show_error_message("Can't open file : %s\n", filename);
		return;
	}

	multi_event_set_dump_per_task_counter_values(&g_mes, &g_filter, file, &nb_errors);

	if(nb_errors)
		show_error_message("Could not determine counter values for %d tasks.", nb_errors);

	fclose(file);
}

void goto_time(double time)
{
	long double left, right, new_left, new_right, range;
	double end = multi_event_set_last_event_end(&g_mes);

	gtk_trace_get_bounds(g_trace_widget, &left, &right);
	range = right - left;

	new_left = time - (range / 2);
	new_right = new_left + range;

	if(new_left < 0) {
		new_right -= new_left;
		new_left = 0;

		if(new_right > end)
			new_right = end;
	} else if(new_right > end) {
		new_left -= new_right - end;
		new_right = end;

		if(new_left < 0)
			new_left = 0;
	}

	gtk_trace_set_bounds(g_trace_widget, new_left, new_right);
	trace_bounds_changed(GTK_TRACE(g_trace_widget), new_left, new_right, NULL);
}

G_MODULE_EXPORT void menubar_goto_time(GtkMenuItem *item, gpointer data)
{
	double time;
	long double left, right;

	double start = multi_event_set_first_event_start(&g_mes);
	double end = multi_event_set_last_event_end(&g_mes);

	gtk_trace_get_bounds(g_trace_widget, &left, &right);

	if(show_goto_dialog(start, end, (double)((left+right)/2.0), &time))
		goto_time(time);
}

G_MODULE_EXPORT void menubar_select_interval(GtkMenuItem *item, gpointer data)
{
	int64_t init_start = 0;
	int64_t init_end = 0;
	uint64_t start;
	uint64_t end;

	if(!gtk_trace_get_range_selection(g_trace_widget, &init_start, &init_end)) {
		init_start = 0;
		init_end = 0;
	}

	if(init_start < 0)
		init_start = 0;

	if(init_end < 0)
		init_end = 0;

	if(show_select_interval_dialog((uint64_t)init_start, (uint64_t)init_end, &start, &end)) {
		gtk_trace_set_range_selection(g_trace_widget, start, end);
		update_statistics();
	}
}

static int react_to_hscrollbar_change = 1;

G_MODULE_EXPORT void trace_bounds_changed(GtkTrace *item, gdouble left, gdouble right, gpointer data)
{
	react_to_hscrollbar_change = 0;
	double start = multi_event_set_first_event_start(&g_mes);
	double end = multi_event_set_last_event_end(&g_mes);
	double page_size = right-left;

	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(g_hscroll_bar));

	gtk_adjustment_set_lower(adj, start + page_size / 2.0);
	gtk_adjustment_set_upper(adj, end + page_size / 2.0);
	gtk_adjustment_set_page_size(adj, page_size);
	gtk_adjustment_set_page_increment(adj, page_size);
	gtk_adjustment_set_step_increment(adj, page_size / 10.0);
	gtk_adjustment_set_value(adj, (left+right)/2.0);

	react_to_hscrollbar_change = 1;
}

static int react_to_vscrollbar_change = 1;

G_MODULE_EXPORT void trace_ybounds_changed(GtkTrace *item, gdouble left, gdouble right, gpointer data)
{
	react_to_vscrollbar_change = 0;
	double start = 0;
	double end = g_mes.num_sets;
	double page_size = right-left;

	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(g_vscroll_bar));

	gtk_adjustment_set_lower(adj, start + page_size / 2.0);
	gtk_adjustment_set_upper(adj, end + page_size / 2.0);
	gtk_adjustment_set_page_size(adj, page_size);
	gtk_adjustment_set_page_increment(adj, page_size);
	gtk_adjustment_set_step_increment(adj, page_size / 10.0);
	gtk_adjustment_set_value(adj, (left+right)/2.0);

	react_to_vscrollbar_change = 1;
}

G_MODULE_EXPORT void trace_state_event_under_pointer_changed(GtkTrace* item, gpointer pstate_event, int cpu, int worker, gpointer data)
{
	static int message_id = -1;
	guint context_id = 0;
	char buffer[256];
	char buf_duration[40];
	struct state_event* se = pstate_event;

	if(message_id != -1)
		gtk_statusbar_remove(GTK_STATUSBAR(g_statusbar), context_id, message_id);

	if(se) {
		pretty_print_cycles(buf_duration, sizeof(buf_duration), se->end - se->start);

		snprintf(buffer, sizeof(buffer), "CPU %d: state %d (%s) from %"PRIu64" to %"PRIu64", duration: %scycles, active task: 0x%"PRIx64" %s",
			 cpu, se->state_id, g_mes.states[se->state_id_seq].name, se->start, se->end, buf_duration, se->active_task->addr, se->active_task->symbol_name);

		message_id = gtk_statusbar_push(GTK_STATUSBAR(g_statusbar), context_id, buffer);
	}
}

G_MODULE_EXPORT void select_range_from_graph_button_clicked(GtkMenuItem *item, gpointer data)
{
	gtk_trace_enter_range_selection_mode(g_trace_widget);
}

G_MODULE_EXPORT void select_measurement_interval_button_clicked(GtkMenuItem *item, gpointer data)
{
	struct global_single_event* first_start = NULL;
	struct global_single_event* last_end = NULL;
	enum measurement_interval_find_status st;

	st = find_first_measurement_interval_start_and_last_measurement_interval_end(&first_start, &last_end);

	if(st == MEASUREMENT_INTERVAL_FIND_OK)
		gtk_trace_set_range_selection(g_trace_widget, first_start->time, last_end->time);
	else
		measurement_interval_find_show_error(st);
}

G_MODULE_EXPORT void clear_range_button_clicked(GtkMenuItem *item, gpointer data)
{
	gtk_widget_set_sensitive(g_button_clear_range, FALSE);
	gtk_trace_clear_range_selection(g_trace_widget);
	gtk_label_set_markup(GTK_LABEL(g_label_range_selection), "<b>No range selected</b>");
	gtk_label_set_markup(GTK_LABEL(g_label_range_selection_omp), "<b>No range selected</b>");

	state_list_clear(GTK_TREE_VIEW(g_state_treeview));
	state_list_fill_name(GTK_TREE_VIEW(g_state_treeview), g_mes.states, g_mes.num_states);

	gtk_label_set_text(GTK_LABEL(g_label_hist_num_tasks), "0 tasks considered");
	gtk_label_set_text(GTK_LABEL(g_label_hist_selection_length), "0 cycles");
	gtk_label_set_text(GTK_LABEL(g_label_hist_avg_task_length), "0 cycles / task (avg)");
	gtk_label_set_text(GTK_LABEL(g_label_hist_num_tcreate), "0 task creation events");
	gtk_label_set_text(GTK_LABEL(g_label_hist_min_cycles), "0");
	gtk_label_set_text(GTK_LABEL(g_label_hist_max_cycles), "MAX");
	gtk_label_set_text(GTK_LABEL(g_label_hist_min_perc), "0");
	gtk_label_set_text(GTK_LABEL(g_label_hist_max_perc), "MAX");

	gtk_label_set_text(GTK_LABEL(g_label_hist_num_chunk_parts), "0 chunk_parts considered");
	gtk_label_set_text(GTK_LABEL(g_label_hist_selection_length_omp), "0 cycles");
	gtk_label_set_text(GTK_LABEL(g_label_hist_avg_chunk_part_length), "0 cycles / chunk_part (avg)");
	gtk_label_set_text(GTK_LABEL(g_label_hist_min_cycles_omp), "0");
	gtk_label_set_text(GTK_LABEL(g_label_hist_max_cycles_omp), "MAX");
	gtk_label_set_text(GTK_LABEL(g_label_hist_min_perc_omp), "0");
	gtk_label_set_text(GTK_LABEL(g_label_hist_max_perc_omp), "MAX");

	gtk_label_set_text(GTK_LABEL(g_label_comm_matrix), "\n");

	gtk_label_set_text(GTK_LABEL(g_label_matrix_local_bytes), "OB local");
	gtk_label_set_text(GTK_LABEL(g_label_matrix_remote_bytes), "OB remote");
	gtk_label_set_text(GTK_LABEL(g_label_matrix_local_perc), "0%\nlocal");

	gtk_histogram_set_data(g_histogram_widget, NULL);
	gtk_multi_histogram_set_data(g_multi_histogram_widget, NULL);

	gtk_matrix_set_data(g_matrix_widget, NULL);
}

void update_comm_matrix(void)
{
	int comm_mask = 0;
	int exclude_reflexive = 0;
	int ignore_direction = 0;
	int num_only = 0;
	enum comm_matrix_mode mode = COMM_MATRIX_MODE_NODE;
	int64_t left, right;
	uint64_t local_bytes = 0;
	uint64_t remote_bytes = 0;
	char buffer[128];
	long double local_ratio;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_radio_matrix_mode_cpu)))
		mode = COMM_MATRIX_MODE_CPU;
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_radio_matrix_mode_node)))
		mode = COMM_MATRIX_MODE_NODE;

	if(!gtk_trace_get_range_selection(g_trace_widget, &left, &right))
		return;

	if(left < 0)
		left = 0;
	if(right < 0)
		right = 0;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_reads)))
		comm_mask |= 1 << COMM_TYPE_DATA_READ;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_writes)))
		comm_mask |= 1 << COMM_TYPE_DATA_WRITE;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_steals)))
		comm_mask |= 1 << COMM_TYPE_STEAL;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_pushes)))
		comm_mask |= 1 << COMM_TYPE_PUSH;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_reflexive)))
		exclude_reflexive = 1;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_direction)))
		ignore_direction = 1;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_numonly)))
		num_only = 1;

	intensity_matrix_destroy(&g_comm_matrix);

	switch(mode) {
		case COMM_MATRIX_MODE_NODE:
			if(numa_node_exchange_matrix_gather(&g_mes, &g_filter, &g_comm_matrix, left, right, comm_mask, exclude_reflexive, ignore_direction, num_only)) {
				show_error_message("Cannot gather communication matrix statistics.");
				return;
			}
			break;

		case COMM_MATRIX_MODE_CPU:
			if(cpu_exchange_matrix_gather(&g_mes, &g_filter, &g_comm_matrix, left, right, comm_mask, exclude_reflexive, ignore_direction, num_only)) {
				show_error_message("Cannot gather communication matrix statistics.");
				return;
			}
			break;
	}

	intensity_matrix_destroy(&g_comm_summary_matrix);
	intensity_matrix_init(&g_comm_summary_matrix, g_comm_matrix.height, 1);

	for(int x = 0; x < g_comm_matrix.width; x++) {
		for(int y = 0; y < g_comm_matrix.height; y++) {
			double val = intensity_matrix_absolute_value_at(&g_comm_matrix, x, y);

			if(x == y)
				local_bytes += val;
			else
				remote_bytes += val;

			intensity_matrix_add_absolute_value_at(&g_comm_summary_matrix, y, 0, val);
		}
	}

	if(num_only) {
		pretty_print_bytes(buffer, sizeof(buffer), local_bytes, " local");
		gtk_label_set_text(GTK_LABEL(g_label_matrix_local_bytes), buffer);

		pretty_print_bytes(buffer, sizeof(buffer), remote_bytes, " remote");
		gtk_label_set_text(GTK_LABEL(g_label_matrix_remote_bytes), buffer);
	} else {
		pretty_print_number(buffer, sizeof(buffer), local_bytes, " local");
		gtk_label_set_text(GTK_LABEL(g_label_matrix_local_bytes), buffer);

		pretty_print_number(buffer, sizeof(buffer), remote_bytes, " remote");
		gtk_label_set_text(GTK_LABEL(g_label_matrix_remote_bytes), buffer);
	}

	if(local_bytes + remote_bytes != 0)
		local_ratio = ((long double)local_bytes) / (((long double)local_bytes) + ((long double)remote_bytes));
	else
		local_ratio = 0;

	snprintf(buffer, sizeof(buffer), "%.2Lf%%\nlocal", local_ratio*100.0);
	gtk_label_set_text(GTK_LABEL(g_label_matrix_local_perc), buffer);

	gtk_matrix_set_data(g_matrix_widget, &g_comm_matrix);

	intensity_matrix_update_intensity(&g_comm_summary_matrix);
	gtk_matrix_set_data(g_matrix_summary_widget, &g_comm_summary_matrix);
}

G_MODULE_EXPORT gint comm_matrix_mode_changed(gpointer data, GtkWidget* radio)
{
	update_comm_matrix();
	return 0;
}

G_MODULE_EXPORT gint comm_matrix_reflexive_toggled(gpointer data, GtkWidget* check)
{
	update_comm_matrix();
	return 0;
}

G_MODULE_EXPORT void comm_matrix_min_threshold_changed(GtkRange* item, gpointer data)
{
	gtk_matrix_set_min_threshold(g_matrix_widget, gtk_range_get_value(item));
	gtk_matrix_set_min_threshold(g_matrix_summary_widget, gtk_range_get_value(item));
}

G_MODULE_EXPORT void comm_matrix_max_threshold_changed(GtkRange* item, gpointer data)
{
	gtk_matrix_set_max_threshold(g_matrix_widget, gtk_range_get_value(item));
	gtk_matrix_set_max_threshold(g_matrix_summary_widget, gtk_range_get_value(item));
}

G_MODULE_EXPORT gint comm_matrix_comm_type_toggled(gpointer data, GtkWidget* check)
{
	update_comm_matrix();
	return 0;
}

G_MODULE_EXPORT gint comm_matrix_direction_toggled(gpointer data, GtkWidget* check)
{
	update_comm_matrix();
	return 0;
}

G_MODULE_EXPORT gint comm_matrix_numonly_toggled(gpointer data, GtkWidget* check)
{
	update_comm_matrix();
	return 0;
}

void update_statistics_labels(uint64_t cycles, uint64_t min_cycles, uint64_t max_cycles, unsigned int max_hist, unsigned int num_tasks)
{
	char buffer[128];
	char buffer2[128];

	if(num_tasks > 0) {
		pretty_print_cycles(buffer, sizeof(buffer), cycles / num_tasks);
		snprintf(buffer2, sizeof(buffer2), "%scycles / task (avg)", buffer);
		gtk_label_set_text(GTK_LABEL(g_label_hist_avg_task_length), buffer2);
	}

	snprintf(buffer, sizeof(buffer), "%d tasks considered", num_tasks);
	gtk_label_set_text(GTK_LABEL(g_label_hist_num_tasks), buffer);

	pretty_print_cycles(buffer, sizeof(buffer), min_cycles);
	gtk_label_set_text(GTK_LABEL(g_label_hist_min_cycles), buffer);

	pretty_print_cycles(buffer, sizeof(buffer), max_cycles);
	gtk_label_set_text(GTK_LABEL(g_label_hist_max_cycles), buffer);

	gtk_label_set_text(GTK_LABEL(g_label_hist_min_perc), "0%");
	snprintf(buffer, sizeof(buffer), "%.2f%%", 100.0*((double)max_hist / (double)num_tasks));
	gtk_label_set_text(GTK_LABEL(g_label_hist_max_perc), buffer);
}

void update_statistics_labels_omp(uint64_t cycles, uint64_t min_cycles, uint64_t max_cycles, unsigned int max_hist, unsigned int num_chunk_parts)
{
	char buffer[128];
	char buffer2[128];

	int num_cpus = 0;
	double par_ratio;
	struct event_set* es;

	int64_t left, right;

	if(!gtk_trace_get_range_selection(g_trace_widget, &left, &right))
		return;

	if(left < 0)
		left = 0;

	if(right < 0)
		right = 0;

	for_each_event_set(&g_mes, es)
		if(filter_has_cpu(&g_filter, es->cpu))
			num_cpus++;

	par_ratio = ((double)cycles) / ((double)(right - left) * num_cpus);

	if(num_chunk_parts > 0) {
		pretty_print_cycles(buffer, sizeof(buffer), cycles / num_chunk_parts);
		snprintf(buffer2, sizeof(buffer2), "%s cycles / chunk_part (avg)", buffer);
		gtk_label_set_text(GTK_LABEL(g_label_hist_avg_chunk_part_length), buffer2);
	}

	snprintf(buffer, sizeof(buffer), "%d chunk_parts considered || par ratio %lf", num_chunk_parts, par_ratio);
	gtk_label_set_text(GTK_LABEL(g_label_hist_num_chunk_parts), buffer);

	pretty_print_cycles(buffer, sizeof(buffer), min_cycles);
	gtk_label_set_text(GTK_LABEL(g_label_hist_min_cycles_omp), buffer);

	pretty_print_cycles(buffer, sizeof(buffer), max_cycles);
	gtk_label_set_text(GTK_LABEL(g_label_hist_max_cycles_omp), buffer);

	gtk_label_set_text(GTK_LABEL(g_label_hist_min_perc_omp), "0%");
	snprintf(buffer, sizeof(buffer), "%.2f%%", 100.0*((double)max_hist / (double)num_chunk_parts));
	gtk_label_set_text(GTK_LABEL(g_label_hist_max_perc_omp), buffer);
}

void update_chunk_set_part_statistics(void)
{
	int64_t left, right;
	struct chunk_set_part_statistics cps;

	gtk_widget_show(g_histogram_widget_omp);

	if(!gtk_trace_get_range_selection(g_trace_widget, &left, &right))
		return;

	if(left < 0)
		left = 0;

	if(right < 0)
		right = 0;

	if(chunk_set_part_statistics_init(&cps, HISTOGRAM_DEFAULT_NUM_BINS) != 0) {
		show_error_message("Cannot allocate task statistics structure.");
		return;
	}

	chunk_set_part_statistics_gather(&g_mes, &g_filter, &cps, left, right);

	chunk_set_part_statistics_to_chunk_set_part_length_histogram(&cps, &g_task_histogram);
	gtk_histogram_set_data(g_histogram_widget_omp, &g_task_histogram);

	update_statistics_labels_omp(cps.cycles, cps.min_cycles, cps.max_cycles, cps.max_hist, cps.num_chunk_set_parts);

	chunk_set_part_statistics_destroy(&cps);
}

void update_task_statistics(void)
{
	int64_t left, right;
	struct task_statistics ts;

	gtk_widget_hide(g_multi_histogram_widget);
	gtk_widget_show(g_histogram_widget);

	if(!gtk_trace_get_range_selection(g_trace_widget, &left, &right))
		return;

	if(task_statistics_init(&ts, HISTOGRAM_DEFAULT_NUM_BINS) != 0) {
		show_error_message("Cannot allocate task statistics structure.");
		return;
	}

	task_statistics_gather(&g_mes, &g_filter, &ts, left, right);

	task_statistics_to_task_length_histogram(&ts, &g_task_histogram);
	gtk_histogram_set_data(g_histogram_widget, &g_task_histogram);

	update_statistics_labels(ts.cycles, ts.min_cycles, ts.max_cycles, ts.max_hist, ts.num_tasks);

	task_statistics_destroy(&ts);
}

void get_max_hist_and_num_tasks(struct multi_task_statistics* mts, unsigned int* max_hist_out, unsigned int* num_tasks_out, uint64_t* cycles_out)
{
	unsigned int max_hist = 0;
	unsigned int num_tasks = 0;
	uint64_t cycles = 0;

	for(int idx = 0; idx < mts->num_tasks_stats; idx++) {
		if(mts->stats[idx]->max_hist > max_hist)
			max_hist = mts->stats[idx]->max_hist;
		num_tasks += mts->stats[idx]->num_tasks;
		cycles += mts->stats[idx]->cycles;
	}

	*max_hist_out = max_hist;
	*num_tasks_out = num_tasks;
	*cycles_out = cycles;
}

void update_multi_task_statistics(void)
{
	int64_t left, right;
	unsigned int max_hist, num_tasks;
	uint64_t cycles;
	struct multi_task_statistics mts;

	gtk_widget_hide(g_histogram_widget);
	gtk_widget_show(g_multi_histogram_widget);

	if(!gtk_trace_get_range_selection(g_trace_widget, &left, &right))
		return;

	if(multi_task_statistics_init(&mts, g_mes.num_tasks, HISTOGRAM_DEFAULT_NUM_BINS) != 0) {
		show_error_message("Cannot allocate task statistics structure.");
		return;
	}

	if(multi_task_statistics_gather(&g_mes, &g_filter, &mts, left, right)) {
		show_error_message("Cannot gather task statistics.");
		multi_task_statistics_destroy(&mts);
		return;
	}

	multi_histogram_destroy(&g_task_multi_histogram);

	if(multi_histogram_init(&g_task_multi_histogram, mts.num_tasks_stats, HISTOGRAM_DEFAULT_NUM_BINS, 0, 1)) {
		show_error_message("Cannot initialize task length multi histogram structure.");
		multi_task_statistics_destroy(&mts);
		return;
	}

	multi_task_statistics_to_task_length_multi_histogram(&mts, &g_task_multi_histogram);
	gtk_multi_histogram_set_data(g_multi_histogram_widget, &g_task_multi_histogram);

	get_max_hist_and_num_tasks(&mts, &max_hist, &num_tasks, &cycles);

	update_statistics_labels(cycles, mts.min_all, mts.max_all, max_hist, num_tasks);

	multi_task_statistics_destroy(&mts);
}

void update_counter_statistics()
{
	int64_t left, right;
	struct task_statistics counter_stats;
	int counter_idx;
	struct event_set* es;
	struct counter_event_set* ces;
	char buffer[128];

	gtk_widget_hide(g_multi_histogram_widget);
	gtk_widget_show(g_histogram_widget);

	counter_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(g_counter_list_widget));

	if(counter_idx == -1)
		return;

	if(!gtk_trace_get_range_selection(g_trace_widget, &left, &right))
		return;

	if(task_statistics_init(&counter_stats, HISTOGRAM_DEFAULT_NUM_BINS) != 0) {
		show_error_message("Cannot allocate task statistics structure (for counters)");
		return;
	}

	/* Check for each CPU that the counter is monotonously increasing */
	for_each_event_set(&g_mes, es) {
		ces = event_set_find_counter_event_set(es, &g_mes.counters[counter_idx]);

		if(!ces)
			continue;

		if(!counter_event_set_is_monotonously_increasing(ces)) {
			snprintf(buffer, sizeof(buffer), "Counter \"%s\" is not monotonously increasing on CPU %d\n", ces->desc->name, es->cpu);
			gtk_label_set_text(GTK_LABEL(g_label_info_counter), buffer);
		} else
			gtk_label_set_text(GTK_LABEL(g_label_info_counter), "");
	}

	counter_statistics_gather(&g_mes, &g_filter, &counter_stats, &g_mes.counters[counter_idx], left, right);
	counter_statistics_to_counter_histogram(&counter_stats, &g_counter_histogram);

	gtk_histogram_set_data(g_histogram_widget, &g_counter_histogram);
	update_statistics_labels(counter_stats.cycles, counter_stats.min_cycles, counter_stats.max_cycles, counter_stats.max_hist, counter_stats.num_tasks);

	task_statistics_destroy(&counter_stats);
}

G_MODULE_EXPORT void omp_map_mode_for_loops_selected(GtkRadioButton *button, gpointer data)
{
	int active = gtk_check_menu_item_get_active((GtkCheckMenuItem*)(button));

	if(active) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(g_toggle_omp_for), true);
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_OMP_FOR_LOOPS);
		g_omp_map_mode = GTK_TRACE_MAP_MODE_OMP_FOR_LOOPS;
	}
}

G_MODULE_EXPORT void omp_map_mode_for_instances_selected(GtkRadioButton *button, gpointer data)
{
	int active = gtk_check_menu_item_get_active((GtkCheckMenuItem*)(button));

	if(active) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(g_toggle_omp_for), true);
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_OMP_FOR_INSTANCES);
		g_omp_map_mode = GTK_TRACE_MAP_MODE_OMP_FOR_INSTANCES;
	}
}

G_MODULE_EXPORT void omp_map_mode_chunks_selected(GtkRadioButton *button, gpointer data)
{
	int active = gtk_check_menu_item_get_active((GtkCheckMenuItem*)(button));

	if(active) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(g_toggle_omp_for), true);
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_OMP_FOR_CHUNK_SETS);
		g_omp_map_mode = GTK_TRACE_MAP_MODE_OMP_FOR_CHUNK_SETS;
	}
}

G_MODULE_EXPORT void omp_map_mode_chunk_parts_selected(GtkRadioButton *button, gpointer data)
{
	int active = gtk_check_menu_item_get_active((GtkCheckMenuItem*)(button));

	if(active) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(g_toggle_omp_for), true);
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_OMP_FOR_CHUNK_SET_PARTS);
		g_omp_map_mode = GTK_TRACE_MAP_MODE_OMP_FOR_CHUNK_SET_PARTS;
	}
}

G_MODULE_EXPORT void omp_map_mode_tasks_selected(GtkRadioButton *button, gpointer data)
{
	int active = gtk_check_menu_item_get_active((GtkCheckMenuItem*)(button));

	if(active) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(g_toggle_omp_for), true);
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_OMP_TASKS);
	}
}

G_MODULE_EXPORT void omp_map_mode_task_instances_selected(GtkRadioButton *button, gpointer data)
{
	int active = gtk_check_menu_item_get_active((GtkCheckMenuItem*)(button));

	if(active) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(g_toggle_omp_for), true);
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_OMP_TASK_INSTANCES);
	}
}

G_MODULE_EXPORT void omp_map_mode_task_parts_selected(GtkRadioButton *button, gpointer data)
{
	int active = gtk_check_menu_item_get_active((GtkCheckMenuItem*)(button));

	if(active) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(g_toggle_omp_for), true);
		gtk_trace_set_map_mode(g_trace_widget, GTK_TRACE_MAP_MODE_OMP_TASK_PARTS);
	}
}

G_MODULE_EXPORT void global_hist_view_activated(GtkRadioButton *button, gpointer data)
{
	int active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

	gtk_histogram_enable_range_selection(g_histogram_widget);

	if(active) {
		gtk_widget_set_sensitive(g_counter_list_widget, 0);
		update_task_statistics();
	}
}

G_MODULE_EXPORT void per_task_hist_view_activated(GtkRadioButton *button, gpointer data)
{
	int active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

	if(active) {
		gtk_widget_set_sensitive(g_counter_list_widget, 0);
		update_multi_task_statistics();
	}
}

G_MODULE_EXPORT void counter_hist_view_activated(GtkRadioButton *button, gpointer data)
{
	int active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

	gtk_histogram_disable_range_selection(g_histogram_widget);

	if(active) {
		gtk_widget_set_sensitive(g_counter_list_widget, 1);
		update_counter_statistics();
	}
}

G_MODULE_EXPORT void counter_changed(GtkComboBox *combo, gpointer data)
{
	update_counter_statistics();
}

void update_statistics(void)
{
	char buffer[128];
	char buffer2[128];
	struct state_statistics sts;
	struct state_description* sd;
	int i;

	int64_t left, right;
	int64_t length;

	if(!gtk_trace_get_range_selection(g_trace_widget, &left, &right))
		return;

	length = right - left;

	gtk_widget_set_sensitive(g_button_clear_range, TRUE);
	gtk_widget_set_sensitive(g_button_clear_range_omp, TRUE);

	snprintf(buffer, sizeof(buffer), "<b>%"PRId64" - %"PRId64"</b>", left, right);
	gtk_label_set_markup(GTK_LABEL(g_label_range_selection), buffer);
	gtk_label_set_markup(GTK_LABEL(g_label_range_selection_omp), buffer);

	pretty_print_cycles(buffer, sizeof(buffer), right - left);
	snprintf(buffer2, sizeof(buffer2), "%scycles selected", buffer);
	gtk_label_set_text(GTK_LABEL(g_label_hist_selection_length), buffer2);
	gtk_label_set_text(GTK_LABEL(g_label_hist_selection_length_omp), buffer2);

	if(left < 0)
		left = 0;

	if(right < 0)
		right = 0;

	state_statistics_init(&sts, g_mes.num_states);
	state_statistics_gather_cycles(&g_mes, &g_filter, &sts, left, right);

	for_each_statedesc_i(&g_mes, sd, i) {
                sd->per = (100*(double)sts.state_cycles[i]) / (double)(length*g_mes.num_sets);
                sd->par = (double)sts.state_cycles[i] / (double)length;
        }

	struct single_event_statistics single_stats;
	single_event_statistics_init(&single_stats);
	single_event_statistics_gather(&g_mes, &g_filter, &single_stats, left, right);

	snprintf(buffer, sizeof(buffer), "%"PRId64" task creation events", single_stats.num_tcreate_events);
	gtk_label_set_text(GTK_LABEL(g_label_hist_num_tcreate), buffer);
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_global_hist_radio_button))) {
		if(g_mes.num_omp_fors != 0)
			update_chunk_set_part_statistics();
		else
			update_task_statistics();
	}
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_per_task_hist_radio_button)))
		update_multi_task_statistics();
	else
		update_counter_statistics();

        state_list_clear(GTK_TREE_VIEW(g_state_treeview));
        state_list_fill(GTK_TREE_VIEW(g_state_treeview), g_mes.states, g_mes.num_states);

	update_comm_matrix();
}

G_MODULE_EXPORT void trace_range_selection_changed(GtkTrace *item, gdouble left, gdouble right, gpointer data)
{
	update_statistics();
}

G_MODULE_EXPORT void trace_create_annotation(GtkTrace *item, int cpu, gdouble time)
{
	struct annotation a;
	struct event_set* es = multi_event_set_find_cpu(&g_mes, cpu);

	annotation_init(&a, cpu, time, "");

	if(show_annotation_dialog(&a, 0) == ANNOTATION_DIALOG_RESPONSE_OK) {
		event_set_add_annotation(es, &a);
		gtk_widget_queue_draw(g_trace_widget);

		g_visuals_modified = 1;
	}

	annotation_destroy(&a);
}

G_MODULE_EXPORT void trace_edit_annotation(GtkTrace *item, struct annotation* a)
{
	switch(show_annotation_dialog(a, 1)) {
		case ANNOTATION_DIALOG_RESPONSE_CANCEL:
			break;
		case ANNOTATION_DIALOG_RESPONSE_DELETE:
			event_set_delete_annotation(a->event_set, a);
		case ANNOTATION_DIALOG_RESPONSE_OK:
			gtk_widget_queue_draw(g_trace_widget);
			g_visuals_modified = 1;
			break;
	}
}

void task_link_activated(uint64_t addr)
{
	struct task* t = multi_event_set_find_task_by_addr(&g_mes, addr);

	if(t && t->source_filename)
		show_task_code(t);
}

void time_link_activated(uint64_t time)
{
	goto_time(time);
}

G_MODULE_EXPORT gint link_activated(GtkLabel *label, gchar *uri, gpointer user_data)
{
	uint64_t work_fn;
	uint64_t time;

	if(strstr(uri, "task://") == uri) {
		sscanf(uri, "task://0x%"PRIx64"", &work_fn);
		task_link_activated(work_fn);
	} else if(strstr(uri, "time://") == uri) {
		sscanf(uri, "time://%"PRIu64"", &time);
		time_link_activated(time);
	}

	return 1;
}

#define DEFINE_MARKER_MARKUP_FUNCTION(name, r, g, b) \
	const char* name(void) \
	{								\
		static char buf[128] = { '\0' };			\
									\
		if(buf[0] == '\0')					\
			snprintf(buf, sizeof(buf), "<span background=\"#%02X%02X%02X\">     </span>", \
				 (int)(r*255.0), \
				 (int)(g*255.0), \
				 (int)(b*255.0)); \
									\
		return buf;						\
	}

DEFINE_MARKER_MARKUP_FUNCTION(markup_prev_tcreate_marker,
			      PREV_TCREATE_TRACE_MARKER_COLOR_R,
			      PREV_TCREATE_TRACE_MARKER_COLOR_G,
			      PREV_TCREATE_TRACE_MARKER_COLOR_B)

DEFINE_MARKER_MARKUP_FUNCTION(markup_ready_marker,
			      READY_TRACE_MARKER_COLOR_R,
			      READY_TRACE_MARKER_COLOR_G,
			      READY_TRACE_MARKER_COLOR_B);

DEFINE_MARKER_MARKUP_FUNCTION(markup_first_write_marker,
			      FIRSTWRITE_TRACE_MARKER_COLOR_R,
			      FIRSTWRITE_TRACE_MARKER_COLOR_G,
			      FIRSTWRITE_TRACE_MARKER_COLOR_B)

DEFINE_MARKER_MARKUP_FUNCTION(markup_first_max_write_marker,
			      FIRSTMAXWRITE_TRACE_MARKER_COLOR_R,
			      FIRSTMAXWRITE_TRACE_MARKER_COLOR_G,
			      FIRSTMAXWRITE_TRACE_MARKER_COLOR_B)

DEFINE_MARKER_MARKUP_FUNCTION(markup_first_tcreate_marker,
			 TCREATE_TRACE_MARKER_COLOR_R,
			 TCREATE_TRACE_MARKER_COLOR_G,
			 TCREATE_TRACE_MARKER_COLOR_B)

DEFINE_MARKER_MARKUP_FUNCTION(markup_tdestroy_marker,
			 TDESTROY_TRACE_MARKER_COLOR_R,
			 TDESTROY_TRACE_MARKER_COLOR_G,
			 TDESTROY_TRACE_MARKER_COLOR_B)

G_MODULE_EXPORT void trace_state_event_selection_changed(GtkTrace* item, gpointer pstate_event, int cpu, int worker, gpointer data)
{
	struct state_event* se = pstate_event;
	char buffer[4096];
	char buf_duration[40];
	char buf_first_tcreate[128];
	char buf_prev_tcreate[128];
	char buf_next_tdestroy[128];
	char buf_ready[128];
	char buf_exec[128];
	char buf_first_writer[128];
	char buf_first_max_writer[128];
	char buf_first_texec_start[128];
	char production_info[1024];
	char consumption_info[1024];
	char consumer_info[1024];

	uint64_t task_length;
	int valid;
	int num_markers = 0;

	if(se) {
		pretty_print_cycles(buf_duration, sizeof(buf_duration), se->end - se->start);

		snprintf(buffer, sizeof(buffer),
			 "CPU:\t\t%d\n"
			 "State\t\t%d (%s)\n"
			 "From\t\t<a href=\"time://%"PRIu64"\">%"PRIu64"</a> to <a href=\"time://%"PRIu64"\">%"PRIu64"</a>\n"
			 "Duration:\t%scycles\n",
			 cpu,
			 se->state_id,
			 g_mes.states[se->state_id_seq].name,
			 se->start,
			 se->start,
			 se->end,
			 se->end,
			 buf_duration);

		gtk_label_set_markup(GTK_LABEL(g_task_selected_event_label), buffer);

		task_length = task_length_of_active_frame(se, &valid);
		if(valid) {
			gtk_trace_set_highlighted_task(g_trace_widget, se->texec_start);

			pretty_print_cycles(buf_duration, sizeof(buf_duration), task_length);

			snprintf(buf_first_tcreate, sizeof(buf_first_tcreate),
				 "CPU %d at  <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a>, %"PRId32" bytes",
				 se->active_frame->first_tcreate->event_set->cpu,
				 se->active_frame->first_tcreate->time,
				 se->active_frame->first_tcreate->time,
				 se->active_frame->size);

			struct single_event* prev_tcreate = multi_event_set_find_prev_tcreate_for_frame(&g_mes, se->texec_start->time, se->active_frame);
			snprintf(buf_prev_tcreate, sizeof(buf_prev_tcreate),
				 "CPU %d at  <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a>",
				 prev_tcreate->event_set->cpu,
				 prev_tcreate->time,
				 prev_tcreate->time);

			g_trace_markers[num_markers].time = prev_tcreate->time;
			g_trace_markers[num_markers].cpu = prev_tcreate->event_set->cpu;
			g_trace_markers[num_markers].color_r = PREV_TCREATE_TRACE_MARKER_COLOR_R;
			g_trace_markers[num_markers].color_g = PREV_TCREATE_TRACE_MARKER_COLOR_G;
			g_trace_markers[num_markers].color_b = PREV_TCREATE_TRACE_MARKER_COLOR_B;
			num_markers++;

			int64_t rdy_time;
			int rdy_cpu;

			int err = multi_event_set_get_prev_ready_time(&g_mes, se->texec_start->time, se->active_frame, &rdy_time, &rdy_cpu);
			if(!err) {
				char ppbuf[20];
				pretty_print_cycles(ppbuf, sizeof(ppbuf), rdy_time - prev_tcreate->time);

				snprintf(buf_ready, sizeof(buf_ready),
					 "CPU %d at  <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a> (%scycles to become ready)",
					 rdy_cpu,
					 rdy_time,
					 rdy_time,
					 ppbuf);

				pretty_print_cycles(ppbuf, sizeof(ppbuf), se->texec_start->time - rdy_time);

				snprintf(buf_exec, sizeof(buf_exec),
					 "%scycles from readiness to execution",
					 ppbuf);

				g_trace_markers[num_markers].time = rdy_time;
				g_trace_markers[num_markers].cpu = rdy_cpu;
				g_trace_markers[num_markers].color_r = READY_TRACE_MARKER_COLOR_R;
				g_trace_markers[num_markers].color_g = READY_TRACE_MARKER_COLOR_G;
				g_trace_markers[num_markers].color_b = READY_TRACE_MARKER_COLOR_B;
				num_markers++;
			} else {
				snprintf(buf_ready, sizeof(buf_ready),
					 "Could not find ready time stamp\n");
				snprintf(buf_exec, sizeof(buf_exec),
					 "Could not find ready time stamp\n");
			}

			g_trace_markers[num_markers].time = se->active_frame->first_tcreate->time;
			g_trace_markers[num_markers].cpu = se->active_frame->first_tcreate->event_set->cpu;
			g_trace_markers[num_markers].color_r = TCREATE_TRACE_MARKER_COLOR_R;
			g_trace_markers[num_markers].color_g = TCREATE_TRACE_MARKER_COLOR_G;
			g_trace_markers[num_markers].color_b = TCREATE_TRACE_MARKER_COLOR_B;
			num_markers++;

			struct single_event* next_tdestroy = multi_event_set_find_next_tdestroy_for_frame(&g_mes, se->texec_start->time, se->active_frame);

			if(next_tdestroy) {
				char ppbuf[20];
				int64_t diff = next_tdestroy->time - se->texec_end->time;

				if(diff > 0)
					pretty_print_cycles(ppbuf, sizeof(ppbuf), diff);

				snprintf(buf_next_tdestroy, sizeof(buf_next_tdestroy),
					 "CPU %d at  <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a> (%s%s)",
					 next_tdestroy->event_set->cpu,
					 next_tdestroy->time,
					 next_tdestroy->time,
					 (diff > 0) ? ppbuf : "",
					 (diff > 0) ? "cycles after termination" : "at task termination");

				g_trace_markers[num_markers].time = next_tdestroy->time;
				g_trace_markers[num_markers].cpu = next_tdestroy->event_set->cpu;
				g_trace_markers[num_markers].color_r = TDESTROY_TRACE_MARKER_COLOR_R;
				g_trace_markers[num_markers].color_g = TDESTROY_TRACE_MARKER_COLOR_G;
				g_trace_markers[num_markers].color_b = TDESTROY_TRACE_MARKER_COLOR_B;
				num_markers++;
			} else {
				snprintf(buf_next_tdestroy, sizeof(buf_next_tdestroy),
					 "Could not find destruction event\n");
			}

			if(se->active_frame->first_write) {
				snprintf(buf_first_writer, sizeof(buf_first_writer),
					 "CPU %d at <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a>, %d bytes",
					 se->active_frame->first_write->event_set->cpu,
					 se->active_frame->first_write->time,
					 se->active_frame->first_write->time,
					 se->active_frame->first_write->size);

				g_trace_markers[num_markers].time = se->active_frame->first_write->time;
				g_trace_markers[num_markers].cpu = se->active_frame->first_write->event_set->cpu;
				g_trace_markers[num_markers].color_r = FIRSTWRITE_TRACE_MARKER_COLOR_R;
				g_trace_markers[num_markers].color_g = FIRSTWRITE_TRACE_MARKER_COLOR_G;
				g_trace_markers[num_markers].color_b = FIRSTWRITE_TRACE_MARKER_COLOR_B;
				num_markers++;
			} else {
				strncpy(buf_first_writer, "Task has no input data", sizeof(buf_first_writer));
			}

			if(se->active_frame->first_max_write) {
				snprintf(buf_first_max_writer, sizeof(buf_first_max_writer),
					 "CPU %d at <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a>, %d bytes",
					 se->active_frame->first_max_write->event_set->cpu,
					 se->active_frame->first_max_write->time,
					 se->active_frame->first_max_write->time,
					 se->active_frame->first_max_write->size);

				g_trace_markers[num_markers].time = se->active_frame->first_max_write->time;
				g_trace_markers[num_markers].cpu = se->active_frame->first_max_write->event_set->cpu;
				g_trace_markers[num_markers].color_r = FIRSTMAXWRITE_TRACE_MARKER_COLOR_R;
				g_trace_markers[num_markers].color_g = FIRSTMAXWRITE_TRACE_MARKER_COLOR_G;
				g_trace_markers[num_markers].color_b = FIRSTMAXWRITE_TRACE_MARKER_COLOR_B;
				num_markers++;
			} else {
				strncpy(buf_first_max_writer, "Task has no input data", sizeof(buf_first_writer));
			}

			if(se->active_frame->first_texec_start) {
				snprintf(buf_first_texec_start, sizeof(buf_first_texec_start),
					 "Node %d",
					 se->active_frame->numa_node);
			} else {
				strncpy(buf_first_texec_start, "Task never executed", sizeof(buf_first_texec_start));
			}

			int consumer_info_offs = 0;
			consumer_info[0] = '\0';

			int production_info_offs = 0;
			production_info[0] = '\0';

			int consumption_info_offs = 0;
			consumption_info[0] = '\0';

			struct comm_event* ce;
			struct single_event* cons_texec_start;
			int has_consumers = 0;
			for_each_comm_event_in_interval(se->event_set,
							se->texec_start->time,
							se->texec_start->next_texec_end->time,
							ce)
			{
				if(ce->type == COMM_TYPE_DATA_WRITE) {
					snprintf(production_info+production_info_offs,
							 sizeof(production_info)-production_info_offs-1,
							 "Node %d, %d bytes, <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a>\n",
							 ce->what->numa_node,
							 ce->size,
							 ce->time,
							 ce->time);
					production_info_offs += strlen(production_info+production_info_offs);

					if((cons_texec_start = multi_event_set_find_next_texec_start_for_frame(&g_mes, ce->time, ce->what))) {
						snprintf(consumer_info+consumer_info_offs,
							 sizeof(consumer_info)-consumer_info_offs-1,
							 "CPU %d, %d bytes, <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a>\n",
							 cons_texec_start->event_set->cpu,
							 ce->size,
							 cons_texec_start->time,
							 cons_texec_start->time);

						consumer_info_offs += strlen(consumer_info+consumer_info_offs);
						has_consumers = 1;
					}
				} else if(ce->type == COMM_TYPE_DATA_READ) {
					snprintf(consumption_info+consumption_info_offs,
							 sizeof(consumption_info)-consumption_info_offs-1,
							 "Node %d, %d bytes, <a href=\"time://%"PRIu64"\">%"PRIu64" cycles</a>\n",
							 ce->what->numa_node,
							 ce->size,
							 ce->time,
							 ce->time);
					consumption_info_offs += strlen(consumption_info+consumption_info_offs);
				}
			}

			if(!has_consumers)
				snprintf(consumer_info+consumer_info_offs,
					 sizeof(consumer_info)-consumer_info_offs-1,
					 "No consumers found");
		} else {
			gtk_trace_set_highlighted_task(g_trace_widget, NULL);
		}

		snprintf(buffer, sizeof(buffer),
			 "Active task:\t0x%"PRIx64" <a href=\"task://0x%"PRIx64"\">%s</a>\n"
			 "Task duration:\t%s\n"
			 "%s Task creation: %s\n"
			 "%s Ready:\t\t%s\n"
			 "%s Task destruction: %s\n"
			 "Execution:\t\t%s\n\n"

			 "Active frame: 0x%"PRIx64"\n"
			 "4K page:\t\t0x%"PRIx64"\n"
			 "2M page:\t0x%"PRIx64"\n"
			 "Owner:\t\t%s\n\n"
			 "%s 1st allocation: %s\n"
			 "%s 1st writer:\t %s\n"
			 "%s 1st max writer: %s\n\n"

			 "Reads:\n"
			 "%s\n"
			 "Writes:\n"
			 "%s\n"
			 "Consumer info:\n"
			 "%s",
			 se->active_task->addr,
			 se->active_task->addr,
			 se->active_task->symbol_name,
			 (valid) ? buf_duration : "Invalid active task",
			 markup_prev_tcreate_marker(),
			 (valid) ? buf_prev_tcreate : "Invalid active task",
			 markup_ready_marker(),
			 (valid) ? buf_ready : "Invalid active task",
			 markup_tdestroy_marker(),
			 (valid) ? buf_next_tdestroy : "Invalid active task",
			 (valid) ? buf_exec : "Invalid active task",
			 se->active_frame->addr,
			 get_base_address(se->active_frame->addr, 1 << 12),
			 get_base_address(se->active_frame->addr, 1 << 21),
			 (valid) ? buf_first_texec_start : "Invalid active task",
			 markup_first_tcreate_marker(),
			 (valid) ? buf_first_tcreate : "Invalid active task",
			 markup_first_write_marker(),
			 (valid) ? buf_first_writer : "Invalid active task",
			 markup_first_max_write_marker(),
			 (valid) ? buf_first_max_writer : "Invalid active task",
			 (valid) ? consumption_info : "No consumer information available",
			 (valid) ? production_info : "No consumer information available",
			 (valid) ? consumer_info : "No consumer information available");

		gtk_label_set_markup(GTK_LABEL(g_active_task_label), buffer);
		gtk_trace_set_markers(g_trace_widget, g_trace_markers, num_markers);
	} else {
		gtk_label_set_markup(GTK_LABEL(g_task_selected_event_label), "");
	}

	if(g_draw_predecessors)
		update_predecessors();
}

G_MODULE_EXPORT void omp_update_highlighted_part(GtkTreeView* item, gpointer pomp_chunk_part, gpointer data)
{
	GtkTrace* trace = GTK_TRACE(g_trace_widget);
	trace->highlight_omp_chunk_set_part = pomp_chunk_part;
	gtk_widget_queue_draw(GTK_WIDGET(trace));
	trace_omp_chunk_set_part_selection_changed(trace, pomp_chunk_part,
						   ((struct omp_for_chunk_set_part*)pomp_chunk_part)->cpu,
						   ((struct omp_for_chunk_set_part*)pomp_chunk_part)->cpu,
						   NULL);
}

G_MODULE_EXPORT void trace_omp_chunk_set_part_selection_changed(GtkTrace* item, gpointer pomp_chunk_set_part, int cpu, int worker, gpointer data)
{
	struct omp_for_chunk_set_part* ofcp = pomp_chunk_set_part;
	struct omp_for_chunk_set* ofc;
	struct omp_for_instance* ofi;
	struct omp_for* of;
	char buffer[4096];
	char buf_duration[40];
	char buf_chunk_set_iter[400] = {'\0'};
	char buf_start_iter[100];
	char buf_end_iter[100];
	char buf_inc[100];
	char buf_total_time_ofc[100];
	char buf_total_time_ofi[100];
	char buf_wallclock_time[100];

	if(ofcp) {
		ofc = ofcp->chunk_set;
		ofi = ofc->for_instance;
		of = ofi->for_loop;
		pretty_print_cycles(buf_duration, sizeof(buf_duration), ofcp->end - ofcp->start);
		pretty_print_cycles(buf_total_time_ofc, sizeof(buf_total_time_ofc), ofc->total_time);
		pretty_print_cycles(buf_total_time_ofi, sizeof(buf_total_time_ofi), ofi->total_time);
		pretty_print_cycles(buf_wallclock_time, sizeof(buf_wallclock_time), ofi->real_loop_time);

		snprintf(buffer, sizeof(buffer),
			 "CPU:\t\t%d\n"
			 "From\t\t<a href=\"time://%"PRIu64"\">%"PRIu64"</a> to <a href=\"time://%"PRIu64"\">%"PRIu64"</a>\n"
			 "Duration:\t%scycles\n",
			 cpu,
			 ofcp->start,
			 ofcp->start,
			 ofcp->end,
			 ofcp->end,
			 buf_duration);

		gtk_label_set_markup(GTK_LABEL(g_openmp_selected_event_label), buffer);

		if(ofi->flags & OMP_FOR_SIGNED_ITERATION_SPACE) {
			if (ofi->flags & OMP_FOR_MULTI_CHUNK_SETS) {
				int64_t chunk_size = (ofc->iter_end - ofc->iter_start) + 1;
				int nb_char_wrote = 0;
				for(int64_t j = ofc->iter_start; j <= ofi->iter_end; j += ofi->increment)
					nb_char_wrote += snprintf(&buf_chunk_set_iter[nb_char_wrote],
							sizeof(buf_chunk_set_iter),\
							"[%"PRId64", %"PRId64"] ",
							j, (j + chunk_size > ofi->iter_end)? ofi->iter_end + 1: (j + chunk_size));
			} else
				snprintf(buf_chunk_set_iter, sizeof(buf_chunk_set_iter),
					 "[%"PRId64", %"PRId64"] ", ofc->iter_start, ofc->iter_end);

			snprintf(buf_start_iter, sizeof(buf_start_iter), "Start iteration:\t%"PRId64, ofi->iter_start);
			snprintf(buf_end_iter, sizeof(buf_end_iter), "End iteration:\t\t%"PRId64, ofi->iter_end);
		} else {
			if (ofi->flags & OMP_FOR_MULTI_CHUNK_SETS) {
				uint64_t chunk_size = ofc->iter_end - ofc->iter_start;
				int nb_char_wrote = 0;
				for(uint64_t j = ofc->iter_start; j < ofi->iter_end; j += ofi->increment)
					nb_char_wrote += snprintf(&buf_chunk_set_iter[nb_char_wrote],
							sizeof(buf_chunk_set_iter),\
							"[%"PRIu64", %"PRIu64"] ",
							j, (j + chunk_size - 1 > ofi->iter_end)? ofi->iter_end: (j + chunk_size));
			} else
				snprintf(buf_chunk_set_iter, sizeof(buf_chunk_set_iter),
					 "[%"PRIu64", %"PRIu64"] ", ofc->iter_start, ofc->iter_end);

			snprintf(buf_start_iter, sizeof(buf_start_iter), "Start iteration: %"PRIu64, ofi->iter_start);
			snprintf(buf_end_iter, sizeof(buf_end_iter), "End iteration: %"PRIu64, ofi->iter_end);
		}

		if(ofi->flags & OMP_FOR_SIGNED_INCREMENT)
			snprintf(buf_inc, sizeof(buf_inc), "Increment: %"PRId64, ofi->increment);
		else
			snprintf(buf_inc, sizeof(buf_inc), "Increment: %"PRIu64, ofi->increment);

		snprintf(buffer, sizeof(buffer),
			 "<b>Chunk part</b>\n"
			 "CPU: %d\n"
			 "Start: %"PRIu64"\n"
			 "End: %"PRIu64"\n\n"

			 "<b>Chunk</b>\n"
			 "Iterations: %s\n"
			 "#Chunk parts: %d\n"
			 "Total time: %s\n\n"

			 "<b>For loop instance</b>\n"
			 "#Chunks: %d\n"
			 "%s\n"
			 "%s\n"
			 "%s\n"
			 "#Workers: %d\n"
			 "Total time: %s -- %"PRIu64"\n"
			 "Chunk load balance: %f%%\n"
			 "Parallelsim efficiency: %f%%\n"
			 "Wall clock time: %s\n\n"

			 "<b>For loop</b>\n"
			 "For addr: 0x%"PRIx64"\n"
			 "#Instances: %d\n",
			 ofcp->cpu,
			 ofcp->start,
			 ofcp->end,
			 buf_chunk_set_iter,
			 ofc->num_chunk_set_parts,
			 buf_total_time_ofc,
			 ofi->num_chunk_sets,
			 buf_start_iter,
			 buf_end_iter,
			 buf_inc,
			 ofi->num_workers,
			 buf_total_time_ofi,
			 ofi->total_time,
			 ofi->chunk_load_balance,
			 ofi->parallelism_efficiency,
			 buf_wallclock_time,
			 of->addr,
			 of->num_instances);

		gtk_label_set_markup(GTK_LABEL(g_active_for_label), buffer);
	} else {
		gtk_label_set_markup(GTK_LABEL(g_openmp_selected_event_label), "");
		gtk_label_set_markup(GTK_LABEL(g_active_for_label), "");
	}
}

G_MODULE_EXPORT void omp_task_update_highlighted_part(GtkTreeView* item, gpointer pomp_task_part, gpointer data)
{
	GtkTrace* trace = GTK_TRACE(g_trace_widget);
	trace->highlight_omp_task_part = pomp_task_part;
	gtk_widget_queue_draw(GTK_WIDGET(trace));
	trace_omp_task_part_selection_changed(trace, pomp_task_part,
						   ((struct omp_task_part*)pomp_task_part)->cpu,
						   ((struct omp_task_part*)pomp_task_part)->cpu,
						   NULL);
}

G_MODULE_EXPORT void trace_omp_task_part_selection_changed(GtkTrace* item, gpointer pomp_task_part, int cpu, int worker, gpointer data)
{
	struct omp_task_part* otp = pomp_task_part;
	struct omp_task_instance* oti;
	struct omp_task* ot;
	char buffer[4096];
	char buf_duration[40];

	if(otp) {
		int n = 0;
		oti = otp->task_instance;
		ot = oti->task;
		pretty_print_cycles(buf_duration, sizeof(buf_duration), otp->end - otp->start);

		snprintf(buffer, sizeof(buffer),
			 "CPU:\t\t%d\n"
			 "From\t\t<a href=\"time://%"PRIu64"\">%"PRIu64"</a> to <a href=\"time://%"PRIu64"\">%"PRIu64"</a>\n"
			 "Duration:\t%scycles\n",
			 cpu,
			 otp->start,
			 otp->start,
			 otp->end,
			 otp->end,
			 buf_duration);

		gtk_label_set_markup(GTK_LABEL(g_openmp_task_selected_event_label), buffer);

		n = snprintf(buffer, sizeof(buffer),
			     "[Task part info]\n"
			     "CPU:\t\t\t%d\n"
			     "Start:\t\t\t%"PRIu64"\n"
			     "End:\t\t\t%"PRIu64"\n",
			     otp->cpu,
			     otp->start,
			     otp->end);

		n += snprintf(buffer+n, sizeof(buffer),
			      "\n[Task instance info]\n"
			      "nb task part %d\n",
			      oti->num_task_parts);

		n += snprintf(buffer+n, sizeof(buffer),
			      "\n[Task info]\n"
			      "Task addr:\t\t\t0X%"PRIX64"\n"
			      "nb instances:\t\t%d\n",
			      ot->addr,
			      ot->num_instances);

		gtk_label_set_markup(GTK_LABEL(g_active_omp_task_label), buffer);
	} else {
		gtk_label_set_markup(GTK_LABEL(g_openmp_task_selected_event_label), "");
		gtk_label_set_markup(GTK_LABEL(g_active_omp_task_label), "");
	}
}

G_MODULE_EXPORT void hscrollbar_value_changed(GtkHScrollbar *item, gdouble value, gpointer data)
{
	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(item));
	double page_size = gtk_adjustment_get_page_size(adj);
	double curr_value = gtk_adjustment_get_value(adj);

	if(react_to_hscrollbar_change)
		gtk_trace_set_bounds(g_trace_widget, curr_value - page_size / 2.0, curr_value + page_size / 2.0);
}

G_MODULE_EXPORT void vscrollbar_value_changed(GtkHScrollbar *item, gdouble value, gpointer data)
{
	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(item));
	double page_size = gtk_adjustment_get_page_size(adj);
	double curr_value = gtk_adjustment_get_value(adj);

	if(react_to_hscrollbar_change)
		gtk_trace_set_cpu_offset(g_trace_widget, curr_value - page_size / 2.0);
}

G_MODULE_EXPORT void task_filter_update(void)
{
	int use_task_length_filter;
	const char* txt;
	uint64_t min_task_length;
	uint64_t max_task_length;
	int64_t min_write_to_node_size;

	filter_clear_tasks(&g_filter);
	filter_clear_ofcps(&g_filter);
	filter_clear_otps(&g_filter);
	task_list_build_filter(GTK_TREE_VIEW(g_task_treeview), &g_filter);
	omp_for_treeview_build_filter(GTK_TREE_VIEW(g_omp_for_treeview), &g_filter);
	omp_task_treeview_build_filter(GTK_TREE_VIEW(g_omp_task_treeview), &g_filter);

	filter_clear_writes_to_numa_nodes_nodes(&g_filter);
	numa_node_list_build_writes_to_numa_nodes_filter(GTK_TREE_VIEW(g_writes_to_numa_nodes_treeview), &g_filter);

	txt = gtk_entry_get_text(GTK_ENTRY(g_writes_to_numa_nodes_min_size_entry));
	if(sscanf(txt, "%"PRId64, &min_write_to_node_size) != 1) {
		show_error_message("\"%s\" is not a correct integer value.", txt);
		return;
	}
	filter_set_writes_to_numa_nodes_minsize(&g_filter, min_write_to_node_size);
	if(min_write_to_node_size > 0)
		filter_set_writes_to_numa_nodes_filtering(&g_filter, 1);

	use_task_length_filter = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_use_task_length_check));

	if(use_task_length_filter) {
		txt = gtk_entry_get_text(GTK_ENTRY(g_task_length_min_entry));
		if(pretty_read_cycles(txt, &min_task_length)) {
			show_error_message("\"%s\" is not a correct number of cycles.", txt);
			return;
		}

		txt = gtk_entry_get_text(GTK_ENTRY(g_task_length_max_entry));
		if(pretty_read_cycles(txt, &max_task_length)) {
			show_error_message("\"%s\" is not a correct number of cycles.", txt);
			return;
		}

		filter_set_task_length_filtering_range(&g_filter, min_task_length, max_task_length);
	}

	filter_set_task_length_filtering(&g_filter, use_task_length_filter);

	gtk_trace_set_filter(g_trace_widget, &g_filter);
	update_statistics();
	update_indexes();
}

G_MODULE_EXPORT void task_filter_button_clicked(GtkMenuItem *item, gpointer data)
{
	task_filter_update();
}

G_MODULE_EXPORT void task_length_entry_activated(GtkEntry *e, gpointer data)
{
	task_filter_update();
}

G_MODULE_EXPORT void histogram_range_selection_changed(GtkHistogram *item, gdouble left, gdouble right, gpointer data)
{
	char txt[20];

	task_filter_update();

	pretty_print_cycles(txt, 20, left);
	gtk_entry_set_text(GTK_ENTRY(g_task_length_min_entry), txt);
	pretty_print_cycles(txt, 20, right);
	gtk_entry_set_text(GTK_ENTRY(g_task_length_max_entry), txt);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_use_task_length_check), TRUE);

	filter_set_task_length_filtering_range(&g_filter, left, right);
	filter_set_task_length_filtering(&g_filter, 1);

	update_statistics();
	update_indexes();
}

G_MODULE_EXPORT void multi_histogram_range_selection_changed(GtkMultiHistogram *item, gdouble left, gdouble right, gpointer data)
{
	char txt[20];
	task_filter_update();

	pretty_print_cycles(txt, 20, left);
	gtk_entry_set_text(GTK_ENTRY(g_task_length_min_entry), txt);
	pretty_print_cycles(txt, 20, right);
	gtk_entry_set_text(GTK_ENTRY(g_task_length_max_entry), txt);

	filter_set_task_length_filtering_range(&g_filter, left, right);
	filter_set_task_length_filtering(&g_filter, 1);

	update_statistics();
	update_indexes();
}

G_MODULE_EXPORT void comm_filter_update(void)
{
	int use_comm_size_filter;
	const char* txt;
	int64_t min_comm_size;
	int64_t max_comm_size;

	use_comm_size_filter = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_use_comm_size_check));

	if(use_comm_size_filter) {
		txt = gtk_entry_get_text(GTK_ENTRY(g_comm_size_min_entry));
		if(sscanf(txt, "%"PRId64, &min_comm_size) != 1) {
			show_error_message("\"%s\" is not a correct integer value.", txt);
			return;
		}

		txt = gtk_entry_get_text(GTK_ENTRY(g_comm_size_max_entry));
		if(sscanf(txt, "%"PRId64, &max_comm_size) != 1) {
			show_error_message("\"%s\" is not a correct integer value.", txt);
			return;
		}

		filter_set_comm_size_filtering_range(&g_filter, min_comm_size, max_comm_size);
	}

	filter_set_comm_size_filtering(&g_filter, use_comm_size_filter);

	filter_clear_comm_numa_nodes(&g_filter);
	numa_node_list_build_comm_filter(GTK_TREE_VIEW(g_comm_numa_node_treeview), &g_filter);

	gtk_trace_set_filter(g_trace_widget, &g_filter);
	update_indexes();
}

G_MODULE_EXPORT void comm_size_length_entry_activated(GtkEntry *e, gpointer data)
{
	comm_filter_update();
}

G_MODULE_EXPORT void comm_filter_button_clicked(GtkMenuItem *item, gpointer data)
{
	comm_filter_update();
}

G_MODULE_EXPORT void task_check_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	task_list_check_all(GTK_TREE_VIEW(g_task_treeview));
}

G_MODULE_EXPORT void task_uncheck_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	task_list_uncheck_all(GTK_TREE_VIEW(g_task_treeview));
}

G_MODULE_EXPORT void writes_to_node_uncheck_all_clicked(GtkMenuItem *item, gpointer data)
{
	numa_node_list_uncheck_all(GTK_TREE_VIEW(g_writes_to_numa_nodes_treeview));
}

G_MODULE_EXPORT void writes_to_node_check_all_clicked(GtkMenuItem *item, gpointer data)
{
	numa_node_list_check_all(GTK_TREE_VIEW(g_writes_to_numa_nodes_treeview));
}

G_MODULE_EXPORT void cpu_check_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	cpu_list_check_all(GTK_TREE_VIEW(g_cpu_treeview));
}

G_MODULE_EXPORT void cpu_uncheck_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	cpu_list_uncheck_all(GTK_TREE_VIEW(g_cpu_treeview));
}

G_MODULE_EXPORT void frame_numa_node_check_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	numa_node_list_check_all(GTK_TREE_VIEW(g_frame_numa_node_treeview));
}

G_MODULE_EXPORT void frame_numa_node_uncheck_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	numa_node_list_uncheck_all(GTK_TREE_VIEW(g_frame_numa_node_treeview));
}

G_MODULE_EXPORT void comm_numa_node_check_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	numa_node_list_check_all(GTK_TREE_VIEW(g_comm_numa_node_treeview));
}

G_MODULE_EXPORT void comm_numa_node_uncheck_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	numa_node_list_uncheck_all(GTK_TREE_VIEW(g_comm_numa_node_treeview));
}

G_MODULE_EXPORT void frame_filter_button_clicked(GtkMenuItem *item, gpointer data)
{
	filter_clear_frames(&g_filter);
	frame_list_build_filter(GTK_TREE_VIEW(g_frame_treeview), &g_filter);

	filter_clear_frame_numa_nodes(&g_filter);
	numa_node_list_build_frame_filter(GTK_TREE_VIEW(g_frame_numa_node_treeview), &g_filter);

	gtk_trace_set_filter(g_trace_widget, &g_filter);
	update_statistics();
	update_indexes();
}

G_MODULE_EXPORT void frame_check_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	frame_list_check_all(GTK_TREE_VIEW(g_frame_treeview));
}

G_MODULE_EXPORT void frame_uncheck_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	frame_list_uncheck_all(GTK_TREE_VIEW(g_frame_treeview));
}

G_MODULE_EXPORT void counter_filter_button_clicked(GtkMenuItem *item, gpointer data)
{
	int use_global_values = 0;
	int use_global_slopes = 0;
	const char* txt;
	int64_t min;
	int64_t max;
	long double min_slope;
	long double max_slope;

	use_global_values = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_use_global_values_check));
	use_global_slopes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_use_global_slopes_check));

	if(use_global_values) {
		txt = gtk_entry_get_text(GTK_ENTRY(g_global_values_min_entry));
		if(sscanf(txt, "%"PRId64, &min) != 1) {
			show_error_message("\"%s\" is not a correct integer value.", txt);
			return;
		}

		txt = gtk_entry_get_text(GTK_ENTRY(g_global_values_max_entry));
		if(sscanf(txt, "%"PRId64, &max) != 1) {
			show_error_message("\"%s\" is not a correct integer value.", txt);
			return;
		}

		if(min >= max) {
			show_error_message("Maximum value must be greater than the minimum value.");
			return;
		}
	}

	if(use_global_slopes) {
		txt = gtk_entry_get_text(GTK_ENTRY(g_global_slopes_min_entry));
		if(sscanf(txt, "%Lf", &min_slope) != 1) {
			show_error_message("\"%s\" is not a correct integer value.", txt);
			return;
		}

		txt = gtk_entry_get_text(GTK_ENTRY(g_global_slopes_max_entry));
		if(sscanf(txt, "%Lf", &max_slope) != 1) {
			show_error_message("\"%s\" is not a correct integer value.", txt);
			return;
		}

		if(min_slope >= max_slope) {
			show_error_message("Maximum value for slopes must be greater than the minimum value.");
			return;
		}
	}

	g_filter.filter_counter_values = use_global_values;
	g_filter.filter_counter_slopes = use_global_slopes;

	if(use_global_values) {
		g_filter.min = min;
		g_filter.max = max;
	}

	if(use_global_slopes) {
		g_filter.min_slope = min_slope;
		g_filter.max_slope = max_slope;
	}

	filter_clear_counters(&g_filter);
	counter_list_build_filter(GTK_TREE_VIEW(g_counter_treeview), &g_filter);

	gtk_trace_set_filter(g_trace_widget, &g_filter);
	update_statistics();
	update_indexes();
}

G_MODULE_EXPORT void counter_check_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	counter_list_check_all(GTK_TREE_VIEW(g_counter_treeview));
}

G_MODULE_EXPORT void counter_uncheck_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	counter_list_uncheck_all(GTK_TREE_VIEW(g_counter_treeview));
}

void show_task_code_in_external_editor(struct task* t)
{
	char editor_cmd[FILENAME_MAX];
	char source_line_str[10];

	snprintf(editor_cmd, sizeof(editor_cmd), "%s", g_settings.external_editor_command);
	snprintf(source_line_str, sizeof(source_line_str), "%d", t->source_line);

	strreplace(editor_cmd, "%f", t->source_filename);
	strreplace(editor_cmd, "%l", source_line_str);

	system(editor_cmd);
}

void show_task_code_in_internal_editor(struct task* t)
{
	FILE* fp;
	int file_size;
	char* buffer;
	GtkTextIter text_iter_start;
	GtkTextIter text_iter_end;
	GtkTextMark* mark;
	GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_code_view));

	if(!(fp = fopen(t->source_filename, "r"))) {
		show_error_message("Could not open file %s\n", t->source_filename);
		return;
	}

	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if(!(buffer = malloc(file_size+1))) {
		show_error_message("Could not allocate buffer for file %s\n", t->source_filename);
		goto out;
	}

	if(fread(buffer, file_size, 1, fp) != 1) {
		show_error_message("Could read file %s\n", t->source_filename);
		goto out;
	}

	gtk_text_buffer_set_text(text_buffer, buffer, file_size);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(g_main_notebook), 1);

	gtk_text_buffer_get_iter_at_line_offset(text_buffer, &text_iter_start, t->source_line, 0);
	gtk_text_buffer_get_iter_at_line_offset(text_buffer, &text_iter_end, t->source_line+1, 0);
	gtk_text_buffer_select_range(text_buffer, &text_iter_start, &text_iter_end);

	mark = gtk_text_buffer_create_mark(text_buffer, "foo", &text_iter_start, TRUE);
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(g_code_view), mark, 0.0, TRUE, 0.2, 0.2);
	gtk_text_buffer_delete_mark(text_buffer, mark);

	free(buffer);

out:
	fclose(fp);
}

void show_task_code(struct task* t)
{
	if(!g_settings.use_external_editor)
		show_task_code_in_internal_editor(t);
	else
		show_task_code_in_external_editor(t);
}

G_MODULE_EXPORT void task_treeview_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data)
{
	GtkTreeView* task_treeview = tree_view;
	GtkTreeModel* model = gtk_tree_view_get_model(task_treeview);
	GtkTreeIter tree_iter;
	struct task* t;

	gtk_tree_model_get_iter(model, &tree_iter, path);
	gtk_tree_model_get(model, &tree_iter, TASK_LIST_COL_TASK_POINTER, &t, -1);

	if(!t->source_filename)
		return;

	show_task_code(t);
}

int store_visuals_with_dialog(void)
{
	if(!g_visuals_filename) {
		if(!(g_visuals_filename = load_save_file_dialog("Save visuals",
						    GTK_FILE_CHOOSER_ACTION_OPEN,
						    "OpenStream visuals",
						    "*.osv",
						      NULL)))
		{
			return 1;
		}
	}

	if(store_visuals(g_visuals_filename, &g_mes)) {
		show_error_message("Could not save visuals to \"%s\".", g_visuals_filename);
		return 1;
	}

	g_visuals_modified = 0;

	return 0;
}

gint check_quit(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if(g_visuals_modified) {
		switch(show_yes_no_cancel_dialog("There are unsaved annotations.\n"
						 "Would you like to save them before quitting?"))
		{
			case DIALOG_RESPONSE_YES:
				if(store_visuals_with_dialog())
					return TRUE;
				break;
			case DIALOG_RESPONSE_CANCEL:
				return TRUE;
			case DIALOG_RESPONSE_NO:
				break;
		}
	}

	gtk_main_quit();
	return FALSE;
}

G_MODULE_EXPORT void menubar_save_visuals(GtkMenuItem *item, gpointer data)
{
	store_visuals_with_dialog();
}

G_MODULE_EXPORT void menubar_save_visuals_as(GtkMenuItem *item, gpointer data)
{
	char* filename;

	if(!(filename = load_save_file_dialog("Save visuals",
						    GTK_FILE_CHOOSER_ACTION_OPEN,
						    "OpenStream visuals",
						    "*.osv",
						      NULL)))
	{
		return;
	}

	free(g_visuals_filename);
	g_visuals_filename = filename;

	store_visuals_with_dialog();
}

G_MODULE_EXPORT void define_counter_offset_clicked(GtkMenuItem *item, gpointer data)
{
	struct counter_description* selection = counter_list_get_highlighted_entry(GTK_TREE_VIEW(g_counter_treeview));
	int64_t offset;

	if(!selection) {
		show_error_message("No counter selected");
		return;
	}

	show_counter_offset_dialog(&g_mes, selection, g_trace_widget, &offset);
}

G_MODULE_EXPORT void comm_matrix_pair_under_pointer_changed(GtkMatrix *item, int node_x, int node_y, int64_t absolute, double relative)
{
	char buffer[128];
	char pretty_bytes[32];
	const char* entity = NULL;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_numonly)))
		pretty_print_number(pretty_bytes, sizeof(pretty_bytes), absolute, "");
	else
		pretty_print_bytes(pretty_bytes, sizeof(pretty_bytes), absolute, "");

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_radio_matrix_mode_cpu)))
		entity = "CPU";
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_radio_matrix_mode_node)))
		entity = "Node";

	snprintf(buffer, sizeof(buffer), "%s %d to %d:\n%s (%.3f%% max.)\n", entity, node_x, node_y, pretty_bytes, 100.0*relative);
	gtk_label_set_text(GTK_LABEL(g_label_comm_matrix), buffer);
}

G_MODULE_EXPORT void comm_summary_matrix_pair_under_pointer_changed(GtkMatrix *item, int node_x, int node_y, int64_t absolute, double relative)
{
	char buffer[128];
	char pretty_bytes[32];
	const char* entity = NULL;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_matrix_numonly)))
		pretty_print_number(pretty_bytes, sizeof(pretty_bytes), absolute, "");
	else
		pretty_print_bytes(pretty_bytes, sizeof(pretty_bytes), absolute, "");

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_radio_matrix_mode_cpu)))
		entity = "CPU";
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_radio_matrix_mode_node)))
		entity = "Node";

	snprintf(buffer, sizeof(buffer), "%s %d:\n%s (%.3f%% max.)\n", entity, node_x, pretty_bytes, 100.0*relative);
	gtk_label_set_text(GTK_LABEL(g_label_comm_matrix), buffer);
}

void cpu_filter_update(void)
{
	filter_clear_cpus(&g_filter);
	cpu_list_build_filter(GTK_TREE_VIEW(g_cpu_treeview), &g_filter);

	gtk_trace_set_filter(g_trace_widget, &g_filter);
	update_statistics();
	update_indexes();
}

G_MODULE_EXPORT void cpu_filter_button_clicked(GtkMenuItem *item, gpointer data)
{
	cpu_filter_update();
}

G_MODULE_EXPORT void single_event_filter_button_clicked(GtkMenuItem *item, gpointer data)
{
	filter_clear_single_event_types(&g_filter);

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_single_c)))
		filter_add_single_event_type(&g_filter, SINGLE_TYPE_TCREATE);

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_single_es)))
		filter_add_single_event_type(&g_filter, SINGLE_TYPE_TEXEC_START);

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_single_ee)))
		filter_add_single_event_type(&g_filter, SINGLE_TYPE_TEXEC_END);

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check_single_d)))
		filter_add_single_event_type(&g_filter, SINGLE_TYPE_TDESTROY);

	filter_set_single_event_type_filtering(&g_filter, 1);

	gtk_trace_set_filter(g_trace_widget, &g_filter);
	update_statistics();
	update_indexes();
}

char* export_to_file_with_dialog(enum export_file_format format)
{
	char last_dir[PATH_MAX];
	char* filename;
	const char* file_type_filter = NULL;
	const char* file_type_name = NULL;

	switch(format) {
		case EXPORT_FORMAT_PDF:
			file_type_filter = "*.pdf";
			file_type_name = "PDF files";
			break;
		case EXPORT_FORMAT_PNG:
			file_type_filter = "*.png";
			file_type_name = "PNG files";
		case EXPORT_FORMAT_SVG:
			file_type_filter = "*.svg";
			file_type_name = "SVG files";
			break;
	}

	if(!(filename = load_save_file_dialog("Save task graph",
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      file_type_name,
					      file_type_filter,
					      g_settings.last_export_dir)))
	{
		return NULL;
	}

	strncpy(last_dir, filename, PATH_MAX);
	dirname(last_dir);
	settings_set_string(&g_settings.last_export_dir, last_dir);

	return filename;
}

void export_timeline(enum export_file_format format)
{
	char* filename = export_to_file_with_dialog(format);

	if(filename) {
		if(gtk_trace_save_to_file(g_trace_widget, format, filename))
			show_error_message("Could not export to file \"%s\".", filename);

		free(filename);
	}
}

G_MODULE_EXPORT void menubar_export_timeline_pdf(GtkMenuItem *item, gpointer data)
{
	export_timeline(EXPORT_FORMAT_PDF);
}

G_MODULE_EXPORT void menubar_export_timeline_png(GtkMenuItem *item, gpointer data)
{
	export_timeline(EXPORT_FORMAT_PNG);
}

G_MODULE_EXPORT void menubar_export_timeline_svg(GtkMenuItem *item, gpointer data)
{
	export_timeline(EXPORT_FORMAT_SVG);
}

void export_task_histogram(enum export_file_format format)
{
	char* filename = export_to_file_with_dialog(format);

	if(filename) {
		/* Do we need to export the multi histogram widget? */
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_per_task_hist_radio_button))) {
			if(gtk_multi_histogram_save_to_file(g_multi_histogram_widget, format, filename))
				show_error_message("Could not export to file \"%s\".", filename);
		} else {
			if(gtk_histogram_save_to_file(g_histogram_widget, format, filename))
				show_error_message("Could not export to file \"%s\".", filename);
		}


		free(filename);
	}
}

G_MODULE_EXPORT void menubar_export_task_histogram_pdf(GtkMenuItem *item, gpointer data)
{
	export_task_histogram(EXPORT_FORMAT_PDF);
}

G_MODULE_EXPORT void menubar_export_task_histogram_png(GtkMenuItem *item, gpointer data)
{
	export_task_histogram(EXPORT_FORMAT_PNG);
}

G_MODULE_EXPORT void menubar_export_task_histogram_svg(GtkMenuItem *item, gpointer data)
{
	export_task_histogram(EXPORT_FORMAT_SVG);
}

void export_openmp_histogram(enum export_file_format format)
{
	char* filename = export_to_file_with_dialog(format);

	if(filename) {
		if(gtk_histogram_save_to_file(g_histogram_widget_omp, format, filename))
			show_error_message("Could not export to file \"%s\".", filename);

		free(filename);
	}
}

G_MODULE_EXPORT void menubar_export_openmp_histogram_pdf(GtkMenuItem *item, gpointer data)
{
	export_openmp_histogram(EXPORT_FORMAT_PDF);
}

G_MODULE_EXPORT void menubar_export_openmp_histogram_png(GtkMenuItem *item, gpointer data)
{
	export_openmp_histogram(EXPORT_FORMAT_PNG);
}

G_MODULE_EXPORT void menubar_export_openmp_histogram_svg(GtkMenuItem *item, gpointer data)
{
	export_openmp_histogram(EXPORT_FORMAT_SVG);
}

void export_comm_matrix(enum export_file_format format)
{
	char* filename = export_to_file_with_dialog(format);

	if(filename) {
		if(gtk_matrix_save_to_file(g_matrix_widget, format, filename))
			show_error_message("Could not export to file \"%s\".", filename);

		free(filename);
	}
}

G_MODULE_EXPORT void menubar_export_comm_matrix_pdf(GtkMenuItem *item, gpointer data)
{
	export_comm_matrix(EXPORT_FORMAT_PDF);
}

G_MODULE_EXPORT void menubar_export_comm_matrix_png(GtkMenuItem *item, gpointer data)
{
	export_comm_matrix(EXPORT_FORMAT_PNG);
}

G_MODULE_EXPORT void menubar_export_comm_matrix_svg(GtkMenuItem *item, gpointer data)
{
	export_comm_matrix(EXPORT_FORMAT_SVG);
}

void export_comm_summary_matrix(enum export_file_format format)
{
	char* filename = export_to_file_with_dialog(format);

	if(filename) {
		if(gtk_matrix_save_to_file(g_matrix_summary_widget, format, filename))
			show_error_message("Could not export to file \"%s\".", filename);

		free(filename);
	}
}

G_MODULE_EXPORT void menubar_export_comm_summary_matrix_pdf(GtkMenuItem *item, gpointer data)
{
	export_comm_summary_matrix(EXPORT_FORMAT_PDF);
}

G_MODULE_EXPORT void menubar_export_comm_summary_matrix_png(GtkMenuItem *item, gpointer data)
{
	export_comm_summary_matrix(EXPORT_FORMAT_PNG);
}

G_MODULE_EXPORT void menubar_export_comm_summary_matrix_svg(GtkMenuItem *item, gpointer data)
{
	export_comm_summary_matrix(EXPORT_FORMAT_SVG);
}

int __build_address_range_tree(void* data)
{
	address_range_tree_init(&g_address_range_tree);

	if(address_range_tree_from_multi_event_set(&g_address_range_tree, &g_mes)) {
		address_range_tree_destroy(&g_address_range_tree);
		return 1;
	}

	return 0;
}

int build_address_range_tree(void)
{
	if(background_task_with_modal_dialog("Analyzing dependences...", "Operation in progress", __build_address_range_tree, NULL)) {
		show_error_message("Could not build address range tree.");
		return 1;
	}

	g_address_range_tree_built = 1;
	return 0;
}

G_MODULE_EXPORT void menubar_show_parallelism_histogram(GtkMenuItem* item, gpointer data)
{
	struct histogram hist;

	if(!g_address_range_tree_built)
		if(build_address_range_tree())
			return;

	if(address_range_tree_build_parallelism_histogram(&g_address_range_tree, &hist)) {
		show_error_message("Could not build parallelism histogram.");
		goto out_hist;
	}

	show_parallelism_dialog(&hist);

out_hist:
	histogram_destroy(&hist);
}

G_MODULE_EXPORT gint comm_matrix_detach_toggled(GtkWidget* button, gpointer data)
{
	int detached = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	static GtkWidget* dlg;

	if(detached) {
		dlg = detach_dialog_create("Aftermath: Communication", g_vbox_comm);
		gtk_widget_set_size_request(g_matrix_widget, -1, -1);
	} else {
		gtk_widget_set_size_request(g_matrix_widget, -1, 100);
		detach_dialog_destroy(dlg, g_vbox_comm, g_vbox_comm_pos);
	}

	return 0;
}

void color_scheme_applied(struct color_scheme* cs)
{
	struct color_scheme_rule* csr;
	struct task* t;
	struct state_description* sd;
	struct counter_description* cd;

	for_each_task(&g_mes, t) {
		if(!t->symbol_name)
			continue;

		for(int i = 0; i < cs->num_rules; i++) {
			csr = cs->rules[i];

			if(csr->type != COLOR_SCHEME_RULE_TYPE_TASKNAME)
				continue;

			if(t->symbol_name && color_scheme_rule_regex_matches(csr, t->symbol_name)) {
				t->color_r = csr->color_r;
				t->color_g = csr->color_g;
				t->color_b = csr->color_b;
			}
		}
	}

	for_each_statedesc(&g_mes, sd) {
		for(int i = 0; i < cs->num_rules; i++) {
			csr = cs->rules[i];

			if(csr->type != COLOR_SCHEME_RULE_TYPE_STATENAME)
				continue;

			if(sd->name && color_scheme_rule_regex_matches(csr, sd->name)) {
				sd->color_r = csr->color_r;
				sd->color_g = csr->color_g;
				sd->color_b = csr->color_b;
			}
		}
	}

	for_each_counterdesc(&g_mes, cd) {
		for(int i = 0; i < cs->num_rules; i++) {
			csr = cs->rules[i];

			if(csr->type != COLOR_SCHEME_RULE_TYPE_COUNTERNAME)
				continue;

			if(cd->name && color_scheme_rule_regex_matches(csr, cd->name)) {
				cd->color_r = csr->color_r;
				cd->color_g = csr->color_g;
				cd->color_b = csr->color_b;
			}
		}
	}

	gtk_widget_queue_draw(g_trace_widget);
	task_list_update_colors(GTK_TREE_VIEW(g_task_treeview));
	state_list_update_colors(GTK_TREE_VIEW(g_state_treeview));
	counter_list_update_colors(GTK_TREE_VIEW(g_counter_treeview));
}

int color_scheme_set_imported(const char* filename)
{
	if(color_scheme_set_load(&g_color_scheme_set, filename)) {
		show_error_message("Could not load color scheme set from file %s.", filename);
		return 0;
	}

	return 1;
}

G_MODULE_EXPORT void menubar_edit_color_schemes(GtkMenuItem* item)
{
	show_color_schemes_dialog(&g_color_scheme_set,
				  color_scheme_applied,
				  color_scheme_set_imported);
}

G_MODULE_EXPORT void menubar_evaluate_filter_expression(GtkMenuItem* item)
{
	char* expr;
	char buffer[32];

	if((expr = show_filter_expression_dialog(g_last_filter_expr))) {
		g_last_filter_expr = expr;

		if(filter_is_valid_expression(expr)) {
			if(filter_eval_expression(&g_filter, &g_mes, expr))
				show_error_message("Could not apply filter expression");

			/* Update task duration entries + checkbox */
			if(g_filter.filter_task_length) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_use_task_length_check), TRUE);

				snprintf(buffer, sizeof(buffer), "%"PRId64, g_filter.min_task_length);
				gtk_entry_set_text(GTK_ENTRY(g_task_length_min_entry), buffer);

				snprintf(buffer, sizeof(buffer), "%"PRId64, g_filter.max_task_length);
				gtk_entry_set_text(GTK_ENTRY(g_task_length_max_entry), buffer);
			}

			/* Update check boxes for single event types */
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_check_single_c),
						     filter_has_single_event_type(&g_filter, SINGLE_TYPE_TCREATE));

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_check_single_es),
						     filter_has_single_event_type(&g_filter, SINGLE_TYPE_TEXEC_START));

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_check_single_ee),
						     filter_has_single_event_type(&g_filter, SINGLE_TYPE_TEXEC_END));

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_check_single_d),
						     filter_has_single_event_type(&g_filter, SINGLE_TYPE_TDESTROY));

			/* Update lists */
			cpu_list_update_from_filter(GTK_TREE_VIEW(g_cpu_treeview), &g_filter);
			task_list_update_from_filter(GTK_TREE_VIEW(g_task_treeview), &g_filter);
			counter_list_update_from_filter(GTK_TREE_VIEW(g_counter_treeview), &g_filter);

			gtk_trace_set_filter(g_trace_widget, &g_filter);
			update_statistics();
			update_indexes();
		} else {
			show_error_message("Not a valid filter expression");
		}
	}
}
