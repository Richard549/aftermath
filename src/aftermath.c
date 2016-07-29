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

#include <stdio.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include <inttypes.h>
#include "glade_extras.h"
#include "gtk_extras.h"
#include "trace_widget.h"
#include "histogram_widget.h"
#include "multi_histogram_widget.h"
#include "matrix_widget.h"
#include "globals.h"
#include "signals.h"
#include "dialogs.h"
#include "task_list.h"
#include "omp_for_treeview.h"
#include "omp_task_treeview.h"
#include "frame_list.h"
#include "numa_node_list.h"
#include "counter_list.h"
#include "state_list.h"
#include "debug.h"
#include "ansi_extras.h"
#include "util.h"
#include "multi_event_set.h"
#include "visuals_file.h"
#include "cpu_list.h"
#include "uncompress.h"

struct load_thread_data {
	char* tracefile;
	off_t bytes_read;
	off_t last_bytes_read;
	off_t trace_size;
	int error;
	int done;
	int ignore_progress;
	uint64_t start_timestamp;
	struct progress_window_widgets* progress_widgets;
};

void* load_thread(void* pdata)
{
	struct load_thread_data* data = pdata;

	if(read_trace_sample_file(&g_mes, data->tracefile, &data->bytes_read) != 0) {
		data->error = 1;
		return NULL;
	}

	data->done = 1;

	return NULL;
}

gboolean update_progress(gpointer pdata)
{
	struct load_thread_data* ltd = pdata;
	char buffer[100];

	struct timeval tv;
	uint64_t timestamp;

	double seconds_elapsed;
	double throughput = 0;
	double seconds_remaining = 0;

	gettimeofday(&tv, NULL);
	timestamp = ((uint64_t)tv.tv_sec)*1000000+tv.tv_usec;

	pretty_print_bytes(buffer, sizeof(buffer), ltd->bytes_read, "");
	gtk_label_set_text(ltd->progress_widgets->label_bytes_loaded, buffer);

	if(ltd->bytes_read == ltd->last_bytes_read) {
		gtk_label_set_text(ltd->progress_widgets->label_throughput, "--");
	} else {
		seconds_elapsed = (double)(timestamp - ltd->start_timestamp) / 1000000.0;

		if(seconds_elapsed > 0) {
			throughput = (double)ltd->bytes_read / seconds_elapsed;
			seconds_remaining = (ltd->trace_size - ltd->bytes_read) / throughput;
		}

		pretty_print_bytes(buffer, sizeof(buffer), throughput, "/s");
		gtk_label_set_text(ltd->progress_widgets->label_throughput, buffer);
	}

	if(!ltd->ignore_progress) {
		pretty_print_bytes(buffer, sizeof(buffer), ltd->trace_size, "");
		gtk_label_set_text(ltd->progress_widgets->label_trace_bytes, buffer);

		snprintf(buffer, sizeof(buffer), "%.0fs", round(seconds_remaining));
		gtk_label_set_text(ltd->progress_widgets->label_seconds_remaining, buffer);

		gtk_progress_bar_set_fraction(ltd->progress_widgets->progressbar,
					      (double)ltd->bytes_read / (double)ltd->trace_size);
	} else {
		gtk_label_set_text(ltd->progress_widgets->label_trace_bytes, "?? [compressed]");
		gtk_label_set_text(ltd->progress_widgets->label_seconds_remaining, "?? [compressed]");
		gtk_progress_bar_set_fraction(ltd->progress_widgets->progressbar, 0.0);
	}

	if(ltd->bytes_read == ltd->trace_size || ltd->error || ltd->done) {
		gtk_widget_hide(GTK_WIDGET(ltd->progress_widgets->window));
		gtk_main_quit();
		return FALSE;
	}

	ltd->last_bytes_read = ltd->bytes_read;

	return TRUE;
}

void set_activate_link_handler_if_label(GtkWidget* widget, void* callback)
{
	if(GTK_IS_LABEL(widget)) {
		g_signal_connect(G_OBJECT(widget), "activate-link",
				 G_CALLBACK(callback),
				 widget);
	}
}

int main(int argc, char** argv)
{
	GladeXML *xml;
	char* tracefile = NULL;
	char* executable = NULL;
	char buffer[30];
	char buffer_short[20];
	char title[PATH_MAX+10];
	enum compression_type compression_type;
	struct counter_description* cd;

	g_visuals_filename = NULL;

	if(argc < 2 || argc > 4) {
		fprintf(stderr, "Usage: %s trace_file [executable [visuals]]\n", argv[0]);
		return 1;
	}

	gtk_init(&argc, &argv);
	tracefile = argv[1];

	if(argc > 2)
		executable = argv[2];

	if(argc > 3)
		g_visuals_filename = strdup(argv[3]);

	g_visuals_modified = 0;

	multi_event_set_init(&g_mes);

	settings_init(&g_settings);

	if(read_user_settings(&g_settings) != 0) {
		show_error_message("Could not read settings");
		return 1;
	}

	off_t trace_size = file_size(tracefile);

	if(trace_size == -1) {
		show_error_message("Cannot determine size of file %s", tracefile);
		return 1;
	}

	if(uncompress_detect_type(tracefile, &compression_type)) {
		show_error_message("Cannot detect compression type of file %s", tracefile);
		return 1;
	}

	struct progress_window_widgets progress_widgets;
	pthread_t tid;
	struct timeval tv;

	show_progress_window_persistent(&progress_widgets);
	gettimeofday(&tv, NULL);

	struct load_thread_data load_thread_data = {
		.tracefile = tracefile,
		.bytes_read = 0,
		.last_bytes_read = 0,
		.ignore_progress = 0,
		.error = 0,
		.done = 0,
		.trace_size = trace_size,
		.start_timestamp = ((uint64_t)tv.tv_sec)*1000000+tv.tv_usec,
		.progress_widgets = &progress_widgets,
	};

	if(compression_type == COMPRESSION_TYPE_UNCOMPRESSED) {
		load_thread_data.ignore_progress = 0;
	} else {
		if(uncompress_get_size(tracefile, &load_thread_data.trace_size))
			load_thread_data.ignore_progress = 1;
		else
			load_thread_data.ignore_progress = 0;
	}

	pthread_create(&tid, NULL, load_thread, &load_thread_data);

	g_timeout_add(100, update_progress, &load_thread_data);

	gtk_main();

	pthread_join(tid, NULL);

	if(load_thread_data.error != 0) {
		show_error_message("Cannot read samples from %s", tracefile);
		return load_thread_data.error;
	}

	if(executable && debug_read_task_symbols(executable, &g_mes) != 0)
		show_error_message("Could not read debug symbols from %s", executable);

	if(g_visuals_filename) {
		if(load_visuals(g_visuals_filename, &g_mes) != 0)
			show_error_message("Could not read visuals from %s", g_visuals_filename);
	}

	xml = glade_xml_new(DATA_PATH "/aftermath.glade", NULL, NULL);
	glade_xml_signal_autoconnect(xml);
	IMPORT_GLADE_WIDGET(xml, toplevel_window);
	IMPORT_GLADE_WIDGET(xml, graph_box);
	IMPORT_GLADE_WIDGET(xml, hist_box);
	IMPORT_GLADE_WIDGET(xml, hist_box_omp);
	IMPORT_GLADE_WIDGET(xml, matrix_box);
	IMPORT_GLADE_WIDGET(xml, matrix_summary_box);
	IMPORT_GLADE_WIDGET(xml, hscroll_bar);
	IMPORT_GLADE_WIDGET(xml, vscroll_bar);
	IMPORT_GLADE_WIDGET(xml, omp_for_treeview);
	IMPORT_GLADE_WIDGET(xml, omp_task_treeview);
	IMPORT_GLADE_WIDGET(xml, task_treeview);
	IMPORT_GLADE_WIDGET(xml, cpu_treeview);
	IMPORT_GLADE_WIDGET(xml, frame_treeview);
	IMPORT_GLADE_WIDGET(xml, frame_numa_node_treeview);
	IMPORT_GLADE_WIDGET(xml, counter_treeview);
	IMPORT_GLADE_WIDGET(xml, state_treeview);
	IMPORT_GLADE_WIDGET(xml, writes_to_numa_nodes_treeview);
	IMPORT_GLADE_WIDGET(xml, code_view);
	IMPORT_GLADE_WIDGET(xml, main_notebook);
	IMPORT_GLADE_WIDGET(xml, statusbar);
	IMPORT_GLADE_WIDGET(xml, task_selected_event_label);
	IMPORT_GLADE_WIDGET(xml, active_task_label);
	IMPORT_GLADE_WIDGET(xml, active_for_label);
	IMPORT_GLADE_WIDGET(xml, active_for_instance_label);
	IMPORT_GLADE_WIDGET(xml, active_for_chunk_label);
	IMPORT_GLADE_WIDGET(xml, active_for_chunk_part_label);
	IMPORT_GLADE_WIDGET(xml, openmp_task_selected_event_label);
	IMPORT_GLADE_WIDGET(xml, active_omp_task_label);
	IMPORT_GLADE_WIDGET(xml, toggle_tool_button_draw_steals);
	IMPORT_GLADE_WIDGET(xml, toggle_tool_button_draw_pushes);
	IMPORT_GLADE_WIDGET(xml, toggle_tool_button_draw_data_reads);
	IMPORT_GLADE_WIDGET(xml, toggle_tool_button_draw_data_writes);
	IMPORT_GLADE_WIDGET(xml, toggle_tool_button_draw_size);
	IMPORT_GLADE_WIDGET(xml, use_global_values_check);
	IMPORT_GLADE_WIDGET(xml, global_values_min_entry);
	IMPORT_GLADE_WIDGET(xml, global_values_max_entry);
	IMPORT_GLADE_WIDGET(xml, use_global_slopes_check);
	IMPORT_GLADE_WIDGET(xml, global_slopes_min_entry);
	IMPORT_GLADE_WIDGET(xml, global_slopes_max_entry);
	IMPORT_GLADE_WIDGET(xml, writes_to_numa_nodes_min_size_entry);
	IMPORT_GLADE_WIDGET(xml, omp_select_level);
	IMPORT_GLADE_WIDGET(xml, omp_select_level_menu);
	IMPORT_GLADE_WIDGET(xml, toggle_omp_for);

	IMPORT_GLADE_WIDGET(xml, label_hist_selection_length);
	IMPORT_GLADE_WIDGET(xml, label_hist_selection_length_omp);
	IMPORT_GLADE_WIDGET(xml, label_hist_avg_task_length);
	IMPORT_GLADE_WIDGET(xml, label_hist_avg_chunk_part_length);
	IMPORT_GLADE_WIDGET(xml, label_hist_num_tasks);
	IMPORT_GLADE_WIDGET(xml, label_hist_num_chunk_parts);
	IMPORT_GLADE_WIDGET(xml, label_hist_num_tcreate);
	IMPORT_GLADE_WIDGET(xml, label_hist_min_cycles);
	IMPORT_GLADE_WIDGET(xml, label_hist_max_cycles);
	IMPORT_GLADE_WIDGET(xml, label_hist_min_perc);
	IMPORT_GLADE_WIDGET(xml, label_hist_max_perc);
	IMPORT_GLADE_WIDGET(xml, label_hist_min_cycles_omp);
	IMPORT_GLADE_WIDGET(xml, label_hist_max_cycles_omp);
	IMPORT_GLADE_WIDGET(xml, label_hist_min_perc_omp);
	IMPORT_GLADE_WIDGET(xml, label_hist_max_perc_omp);

	IMPORT_GLADE_WIDGET(xml, use_task_length_check);
	IMPORT_GLADE_WIDGET(xml, task_length_min_entry);
	IMPORT_GLADE_WIDGET(xml, task_length_max_entry);

	IMPORT_GLADE_WIDGET(xml, use_comm_size_check);
	IMPORT_GLADE_WIDGET(xml, comm_size_min_entry);
	IMPORT_GLADE_WIDGET(xml, comm_size_max_entry);
	IMPORT_GLADE_WIDGET(xml, comm_numa_node_treeview);
	IMPORT_GLADE_WIDGET(xml, button_clear_range);
	IMPORT_GLADE_WIDGET(xml, button_clear_range_omp);
	IMPORT_GLADE_WIDGET(xml, label_range_selection);
	IMPORT_GLADE_WIDGET(xml, label_range_selection_omp);

	IMPORT_GLADE_WIDGET(xml, heatmap_min_cycles);
	IMPORT_GLADE_WIDGET(xml, heatmap_max_cycles);
	IMPORT_GLADE_WIDGET(xml, heatmap_num_shades);

	IMPORT_GLADE_WIDGET(xml, check_matrix_reads);
	IMPORT_GLADE_WIDGET(xml, check_matrix_writes);
	IMPORT_GLADE_WIDGET(xml, check_matrix_steals);
	IMPORT_GLADE_WIDGET(xml, check_matrix_pushes);
	IMPORT_GLADE_WIDGET(xml, check_matrix_reflexive);
	IMPORT_GLADE_WIDGET(xml, check_matrix_direction);
	IMPORT_GLADE_WIDGET(xml, check_matrix_numonly);
	IMPORT_GLADE_WIDGET(xml, label_matrix_local_bytes);
	IMPORT_GLADE_WIDGET(xml, label_matrix_remote_bytes);
	IMPORT_GLADE_WIDGET(xml, label_matrix_local_perc);
	IMPORT_GLADE_WIDGET(xml, label_comm_matrix);

	IMPORT_GLADE_WIDGET(xml, check_single_c);
	IMPORT_GLADE_WIDGET(xml, check_single_es);
	IMPORT_GLADE_WIDGET(xml, check_single_ee);
	IMPORT_GLADE_WIDGET(xml, check_single_d);

	IMPORT_GLADE_WIDGET(xml, counter_combo_box);
	IMPORT_GLADE_WIDGET(xml, label_info_counter);
	IMPORT_GLADE_WIDGET(xml, global_hist_radio_button);
	IMPORT_GLADE_WIDGET(xml, per_task_hist_radio_button);
	IMPORT_GLADE_WIDGET(xml, counter_hist_radio_button);

	IMPORT_GLADE_WIDGET(xml, radio_matrix_mode_node);
	IMPORT_GLADE_WIDGET(xml, radio_matrix_mode_cpu);

	IMPORT_GLADE_WIDGET(xml, vbox_stats_all);
	IMPORT_GLADE_WIDGET(xml, vbox_comm);
	IMPORT_GLADE_WIDGET(xml, vbox_comm_pos);

	g_trace_widget = gtk_trace_new(&g_mes);
	gtk_container_add(GTK_CONTAINER(graph_box), g_trace_widget);

	g_histogram_widget = gtk_histogram_new();
	gtk_container_add(GTK_CONTAINER(hist_box), g_histogram_widget);

	g_multi_histogram_widget = gtk_multi_histogram_new(&g_mes);
	gtk_container_add(GTK_CONTAINER(hist_box), g_multi_histogram_widget);

	g_histogram_widget_omp = gtk_histogram_new();
	gtk_container_add(GTK_CONTAINER(hist_box_omp), g_histogram_widget_omp);

	g_counter_list_widget = counter_combo_box;

	for_each_counterdesc(&g_mes, cd) {
		print_short(buffer_short, sizeof(buffer_short), cd->name);
		gtk_combo_box_append_text(GTK_COMBO_BOX(g_counter_list_widget), buffer_short);
	}

	g_label_info_counter = label_info_counter;
	g_global_hist_radio_button = global_hist_radio_button;
	g_per_task_hist_radio_button =  per_task_hist_radio_button;
	g_counter_hist_radio_button = counter_hist_radio_button;

	g_matrix_widget = gtk_matrix_new();
	gtk_container_add(GTK_CONTAINER(matrix_box), g_matrix_widget);

	g_matrix_summary_widget = gtk_matrix_new();
	gtk_container_add(GTK_CONTAINER(matrix_summary_box), g_matrix_summary_widget);

	g_hscroll_bar = hscroll_bar;
	g_vscroll_bar = vscroll_bar;
	g_omp_for_treeview = omp_for_treeview;
	g_omp_task_treeview = omp_task_treeview;
	g_task_treeview = task_treeview;
	g_cpu_treeview = cpu_treeview;
	g_frame_treeview = frame_treeview;
	g_writes_to_numa_nodes_treeview = writes_to_numa_nodes_treeview;
	g_frame_numa_node_treeview = frame_numa_node_treeview;
	g_counter_treeview = counter_treeview;
	g_state_treeview = state_treeview;
	g_code_view = code_view;
	g_main_notebook = main_notebook;
	g_statusbar = statusbar;
	g_task_selected_event_label = task_selected_event_label;
	g_active_task_label = active_task_label;

	g_active_for_label = active_for_label;
	g_active_for_instance_label = active_for_instance_label;
	g_active_for_chunk_label = active_for_chunk_label;
	g_active_for_chunk_part_label = active_for_chunk_part_label;

	g_openmp_task_selected_event_label = openmp_task_selected_event_label;
	g_active_omp_task_label = active_omp_task_label;
	g_toggle_tool_button_draw_steals = toggle_tool_button_draw_steals;
	g_toggle_tool_button_draw_pushes = toggle_tool_button_draw_pushes;
	g_toggle_tool_button_draw_data_reads = toggle_tool_button_draw_data_reads;
	g_toggle_tool_button_draw_data_writes = toggle_tool_button_draw_data_writes;
	g_toggle_tool_button_draw_size = toggle_tool_button_draw_size;
	g_writes_to_numa_nodes_min_size_entry = writes_to_numa_nodes_min_size_entry;
	g_toggle_omp_for = toggle_omp_for;
	g_omp_map_mode = GTK_TRACE_MAP_MODE_OMP_FOR_LOOPS;

	g_use_global_values_check = use_global_values_check;
	g_global_values_min_entry = global_values_min_entry;
	g_global_values_max_entry = global_values_max_entry;
	g_use_global_slopes_check = use_global_slopes_check;
	g_global_slopes_min_entry = global_slopes_min_entry;
	g_global_slopes_max_entry = global_slopes_max_entry;

	g_label_hist_selection_length = label_hist_selection_length;
	g_label_hist_selection_length_omp = label_hist_selection_length_omp;
	g_label_hist_avg_task_length = label_hist_avg_task_length;
	g_label_hist_avg_chunk_part_length = label_hist_avg_chunk_part_length;
	g_label_hist_num_tasks = label_hist_num_tasks;
	g_label_hist_num_chunk_parts = label_hist_num_chunk_parts;
	g_label_hist_num_tcreate = label_hist_num_tcreate;
	g_label_hist_min_cycles = label_hist_min_cycles;
	g_label_hist_max_cycles = label_hist_max_cycles;
	g_label_hist_min_perc = label_hist_min_perc;
	g_label_hist_max_perc = label_hist_max_perc;
	g_label_hist_min_cycles_omp = label_hist_min_cycles_omp;
	g_label_hist_max_cycles_omp = label_hist_max_cycles_omp;
	g_label_hist_min_perc_omp = label_hist_min_perc_omp;
	g_label_hist_max_perc_omp = label_hist_max_perc_omp;

	g_use_task_length_check = use_task_length_check;
	g_task_length_min_entry = task_length_min_entry;
	g_task_length_max_entry = task_length_max_entry;

	g_use_comm_size_check = use_comm_size_check;
	g_comm_size_min_entry = comm_size_min_entry;
	g_comm_size_max_entry = comm_size_max_entry;
	g_comm_numa_node_treeview = comm_numa_node_treeview;

	g_button_clear_range = button_clear_range;
	g_button_clear_range_omp = button_clear_range_omp;
	g_label_range_selection = label_range_selection;
	g_label_range_selection_omp = label_range_selection_omp;

	g_heatmap_min_cycles = heatmap_min_cycles;
	g_heatmap_max_cycles = heatmap_max_cycles;
	g_heatmap_num_shades = heatmap_num_shades;

	g_check_matrix_reads = check_matrix_reads;
	g_check_matrix_writes = check_matrix_writes;
	g_check_matrix_steals = check_matrix_steals;
	g_check_matrix_pushes = check_matrix_pushes;
	g_check_matrix_reflexive = check_matrix_reflexive;
	g_check_matrix_direction = check_matrix_direction;
	g_check_matrix_numonly = check_matrix_numonly;
	g_label_matrix_local_bytes = label_matrix_local_bytes;
	g_label_matrix_remote_bytes = label_matrix_remote_bytes;
	g_label_matrix_local_perc = label_matrix_local_perc;
	g_label_comm_matrix = label_comm_matrix;

	g_check_single_c = check_single_c;
	g_check_single_es = check_single_es;
	g_check_single_ee = check_single_ee;
	g_check_single_d = check_single_d;

	g_radio_matrix_mode_node = radio_matrix_mode_node;
	g_radio_matrix_mode_cpu = radio_matrix_mode_cpu;

	g_draw_predecessors = 0;
	g_predecessor_max_depth = 3;
	g_predecessors = NULL;
	g_num_predecessors = NULL;

	g_last_filter_expr = NULL;

	g_vbox_stats_all = vbox_stats_all;
	g_vbox_comm = vbox_comm;
	g_vbox_comm_pos = vbox_comm_pos;

	snprintf(buffer, sizeof(buffer), "%"PRId64, multi_event_get_min_counter_value(&g_mes));
	gtk_entry_set_text(GTK_ENTRY(g_global_values_min_entry), buffer);
	snprintf(buffer, sizeof(buffer), "%"PRId64, multi_event_get_max_counter_value(&g_mes));
	gtk_entry_set_text(GTK_ENTRY(g_global_values_max_entry), buffer);

	snprintf(buffer, sizeof(buffer), "%Lf", multi_event_get_min_counter_slope(&g_mes));
	gtk_entry_set_text(GTK_ENTRY(g_global_slopes_min_entry), buffer);
	snprintf(buffer, sizeof(buffer), "%Lf", multi_event_get_max_counter_slope(&g_mes));
	gtk_entry_set_text(GTK_ENTRY(g_global_slopes_max_entry), buffer);

	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(omp_select_level), omp_select_level_menu);

	g_signal_connect(G_OBJECT(g_trace_widget), "bounds-changed", G_CALLBACK(trace_bounds_changed), g_trace_widget);
	g_signal_connect(G_OBJECT(g_trace_widget), "ybounds-changed", G_CALLBACK(trace_ybounds_changed), g_trace_widget);
	g_signal_connect(G_OBJECT(g_trace_widget), "state-event-under-pointer-changed", G_CALLBACK(trace_state_event_under_pointer_changed), g_trace_widget);
	g_signal_connect(G_OBJECT(g_trace_widget), "state-event-selection-changed", G_CALLBACK(trace_state_event_selection_changed), g_trace_widget);
	g_signal_connect(G_OBJECT(g_trace_widget), "omp-chunk-set-part-selection-changed", G_CALLBACK(trace_omp_chunk_set_part_selection_changed), g_trace_widget);
	g_signal_connect(G_OBJECT(g_trace_widget), "omp-task-part-selection-changed", G_CALLBACK(trace_omp_task_part_selection_changed), g_trace_widget);
	g_signal_connect(G_OBJECT(g_trace_widget), "range-selection-changed", G_CALLBACK(trace_range_selection_changed), g_trace_widget);
	g_signal_connect(G_OBJECT(g_trace_widget), "create-annotation", G_CALLBACK(trace_create_annotation), g_trace_widget);
	g_signal_connect(G_OBJECT(g_trace_widget), "edit-annotation", G_CALLBACK(trace_edit_annotation), g_trace_widget);
	g_signal_connect(G_OBJECT(g_histogram_widget), "range-selection-changed", G_CALLBACK(histogram_range_selection_changed), g_histogram_widget);
	g_signal_connect(G_OBJECT(g_multi_histogram_widget), "range-selection-changed", G_CALLBACK(multi_histogram_range_selection_changed), g_multi_histogram_widget);
	g_signal_connect(G_OBJECT(toplevel_window), "delete-event", G_CALLBACK(check_quit), NULL);

	g_signal_connect(G_OBJECT(g_matrix_widget), "pair-under-pointer-changed", G_CALLBACK(comm_matrix_pair_under_pointer_changed), g_matrix_widget);
	g_signal_connect(G_OBJECT(g_matrix_summary_widget), "pair-under-pointer-changed", G_CALLBACK(comm_summary_matrix_pair_under_pointer_changed), g_matrix_summary_widget);

	g_omp_for_treeview_type = omp_for_treeview_init(GTK_TREE_VIEW(g_omp_for_treeview));
	g_signal_connect(G_OBJECT(g_omp_for_treeview_type), "omp-update-highlighted-part", G_CALLBACK(omp_update_highlighted_part), g_trace_widget);
	omp_for_treeview_fill(GTK_TREE_VIEW(g_omp_for_treeview), g_mes.omp_fors, g_mes.num_omp_fors);

	g_omp_task_treeview_type = omp_task_treeview_init(GTK_TREE_VIEW(g_omp_task_treeview));
	g_signal_connect(G_OBJECT(g_omp_task_treeview_type), "omp-task-update-highlighted-part", G_CALLBACK(omp_task_update_highlighted_part), g_trace_widget);
	omp_task_treeview_fill(GTK_TREE_VIEW(g_omp_task_treeview), g_mes.omp_tasks, g_mes.num_omp_tasks);

	/* Set the same handler for links for all labels */
	gtk_extra_for_each_descendant(GTK_WIDGET(toplevel_window), set_activate_link_handler_if_label, link_activated);

	task_list_init(GTK_TREE_VIEW(g_task_treeview));
	task_list_fill(GTK_TREE_VIEW(g_task_treeview), g_mes.tasks, g_mes.num_tasks);

	cpu_list_init(GTK_TREE_VIEW(g_cpu_treeview));
	cpu_list_fill(GTK_TREE_VIEW(g_cpu_treeview), multi_event_get_max_cpu(&g_mes));
	cpu_list_check_all(GTK_TREE_VIEW(g_cpu_treeview));

	frame_list_init(GTK_TREE_VIEW(g_frame_treeview));
	frame_list_fill(GTK_TREE_VIEW(g_frame_treeview), g_mes.frames, g_mes.num_frames);

	numa_node_list_init(GTK_TREE_VIEW(g_frame_numa_node_treeview));
	numa_node_list_fill(GTK_TREE_VIEW(g_frame_numa_node_treeview), g_mes.max_numa_node_id);

	numa_node_list_init(GTK_TREE_VIEW(g_writes_to_numa_nodes_treeview));
	numa_node_list_fill(GTK_TREE_VIEW(g_writes_to_numa_nodes_treeview), g_mes.max_numa_node_id);

	counter_list_init(GTK_TREE_VIEW(g_counter_treeview));
	counter_list_fill(GTK_TREE_VIEW(g_counter_treeview), g_mes.counters, g_mes.num_counters);

	state_list_init(GTK_TREE_VIEW(g_state_treeview));
	state_list_fill_name(GTK_TREE_VIEW(g_state_treeview), g_mes.states, g_mes.num_states);

	numa_node_list_init(GTK_TREE_VIEW(g_comm_numa_node_treeview));
	numa_node_list_fill(GTK_TREE_VIEW(g_comm_numa_node_treeview), g_mes.max_numa_node_id);

	filter_init(&g_filter,
		    multi_event_get_min_counter_value(&g_mes),
		    multi_event_get_max_counter_value(&g_mes),
		    multi_event_get_min_counter_slope(&g_mes),
		    multi_event_get_max_counter_slope(&g_mes),
		    multi_event_get_max_cpu(&g_mes)
		);

	if(histogram_init(&g_task_histogram, HISTOGRAM_DEFAULT_NUM_BINS, 0, 1)) {
		show_error_message("Cannot initialize task length histogram structure.");
		return 1;
	}

	if(histogram_init(&g_counter_histogram, HISTOGRAM_DEFAULT_NUM_BINS, 0, 1)) {
		show_error_message("Cannot initialize counter histogram structure.");
		return 1;
	}

	if(intensity_matrix_init(&g_comm_matrix, 1, 1)) {
		show_error_message("Cannot initialize communication matrix.");
		return 1;
	}

	if(intensity_matrix_init(&g_comm_summary_matrix, 1, 1)) {
		show_error_message("Cannot initialize communication summary matrix.");
		return 1;
	}

	g_address_range_tree_built = 0;

	color_scheme_set_init(&g_color_scheme_set);

	snprintf(title, sizeof(title), "Aftermath - %s", tracefile);
	gtk_window_set_title(GTK_WINDOW(toplevel_window), title);

	gtk_widget_show_all(toplevel_window);

	gtk_widget_hide(g_multi_histogram_widget);
	gtk_widget_set_sensitive(g_counter_list_widget, 0);

	reset_zoom();

	gtk_main();

	multi_event_set_destroy(&g_mes);
	filter_destroy(&g_filter);

	if(write_user_settings(&g_settings) != 0)
		show_error_message("Could not write settings");

	settings_destroy(&g_settings);
	histogram_destroy(&g_task_histogram);
	histogram_destroy(&g_counter_histogram);
	multi_histogram_destroy(&g_task_multi_histogram);
	intensity_matrix_destroy(&g_comm_matrix);
	intensity_matrix_destroy(&g_comm_summary_matrix);
	free(g_visuals_filename);

	free(g_predecessors);
	free(g_num_predecessors);

	if(g_address_range_tree_built)
		address_range_tree_destroy(&g_address_range_tree);

	free(g_last_filter_expr);

	color_scheme_set_destroy(&g_color_scheme_set);

	return 0;
}
