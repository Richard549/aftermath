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
#include <gtk/gtk.h>
#include "globals.h"
#include "trace_widget.h"
#include <inttypes.h>
#include "dialogs.h"
#include "task_list.h"

void reset_zoom(void)
{
	uint64_t start = multi_event_set_first_event_start(&g_mes);
	uint64_t end = multi_event_set_last_event_end(&g_mes);

	printf("ZOOM TO %"PRIu64" %"PRIu64"\n", start, end);

	gtk_trace_set_bounds(g_trace_widget, start, end);
	trace_bounds_changed(GTK_TRACE(g_trace_widget), (double)start, (double)end, NULL);
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

G_MODULE_EXPORT void toolbar_draw_states_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_states(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void toolbar_draw_comm_toggled(GtkToggleToolButton *button, gpointer data)
{
	gboolean new_state = gtk_toggle_tool_button_get_active(button);

	gtk_trace_set_draw_comm(g_trace_widget, new_state);

	gtk_widget_set_sensitive(g_toggle_tool_button_draw_steals, new_state);
	gtk_widget_set_sensitive(g_toggle_tool_button_draw_pushes, new_state);
	gtk_widget_set_sensitive(g_toggle_tool_button_draw_data_reads, new_state);
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

G_MODULE_EXPORT void toolbar_draw_single_events_toggled(GtkToggleToolButton *button, gpointer data)
{
	gtk_trace_set_draw_single_events(g_trace_widget, gtk_toggle_tool_button_get_active(button));
}

G_MODULE_EXPORT void menubar_double_buffering_toggled(GtkCheckMenuItem *item, gpointer data)
{
	gtk_trace_set_double_buffering(g_trace_widget, gtk_check_menu_item_get_active(item));
}

G_MODULE_EXPORT void menubar_about(GtkMenuItem *item, gpointer data)
{
	show_about_dialog();
}

G_MODULE_EXPORT void menubar_goto_time(GtkMenuItem *item, gpointer data)
{
	double start = multi_event_set_first_event_start(&g_mes);
	double end = multi_event_set_last_event_end(&g_mes);
	long double left, right, new_left, new_right, range;
	double time;

	gtk_trace_get_bounds(g_trace_widget, &left, &right);
	range = right - left;

	if(show_goto_dialog(start, end, (double)((left+right)/2.0), &time)) {
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
}

static int react_to_scrollbar_change = 1;

G_MODULE_EXPORT void trace_bounds_changed(GtkTrace *item, gdouble left, gdouble right, gpointer data)
{
	react_to_scrollbar_change = 0;
	double start = multi_event_set_first_event_start(&g_mes);
	double end = multi_event_set_last_event_end(&g_mes);
	double page_size = right-left;

	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(g_scroll_bar));

	gtk_adjustment_set_lower(adj, start + page_size / 2.0);
	gtk_adjustment_set_upper(adj, end + page_size / 2.0);
	gtk_adjustment_set_page_size(adj, page_size);
	gtk_adjustment_set_page_increment(adj, page_size);
	gtk_adjustment_set_step_increment(adj, page_size / 10.0);
	gtk_adjustment_set_value(adj, (left+right)/2.0);

	react_to_scrollbar_change = 1;
}

G_MODULE_EXPORT void trace_state_event_under_pointer_changed(GtkTrace* item, gpointer pstate_event, int cpu, int worker, gpointer data)
{
	static int message_id = -1;
	guint context_id = 0;
	char buffer[256];
	struct task* task;
	struct state_event* se = pstate_event;

	if(message_id != -1)
		gtk_statusbar_remove(GTK_STATUSBAR(g_statusbar), context_id, message_id);

	if(se) {
		task = multi_event_set_find_task_by_work_fn(&g_mes, se->active_task);
		snprintf(buffer, sizeof(buffer), "CPU %d: state %d (%s) from %"PRIu64" to %"PRIu64", duration: %"PRIu64" cycles, active task: 0x%"PRIx64" %s",
			 cpu, se->state, worker_state_names[se->state], se->start, se->end, se->end - se->start, se->active_task, task->symbol_name);

		message_id = gtk_statusbar_push(GTK_STATUSBAR(g_statusbar), context_id, buffer);
	}
}

G_MODULE_EXPORT gint task_link_activated(GtkLabel *label, gchar *uri, gpointer user_data)
{
	uint64_t work_fn;
	struct task* t;

	sscanf(uri, "task://0x%"PRIx64"", &work_fn);
	t = multi_event_set_find_task_by_work_fn(&g_mes, work_fn);

	if(t && t->source_filename)
		show_task_code(t);

	return 1;
}

G_MODULE_EXPORT void trace_state_event_selection_changed(GtkTrace* item, gpointer pstate_event, int cpu, int worker, gpointer data)
{
	struct state_event* se = pstate_event;
	char buffer[256];
	struct task* task;

	if(se) {
		task = multi_event_set_find_task_by_work_fn(&g_mes, se->active_task);
		snprintf(buffer, sizeof(buffer),
			 "CPU:\t\t%d\n"
			 "State\t\t%d (%s)\n"
			 "From\t\t%"PRIu64" to %"PRIu64"\n"
			 "Duration:\t%"PRIu64" cycles\n"
			 "Active task:\t0x%"PRIx64" <a href=\"task://0x%"PRIx64"\">%s</a>",
			 cpu,
			 se->state,
			 worker_state_names[se->state],
			 se->start,
			 se->end,
			 se->end - se->start,
			 se->active_task,
			 se->active_task,
			 task->symbol_name);

		gtk_label_set_markup(GTK_LABEL(g_selected_event_label), buffer);
	} else {
		gtk_label_set_markup(GTK_LABEL(g_selected_event_label), "");
	}
}

G_MODULE_EXPORT void scrollbar_value_changed(GtkHScrollbar *item, gdouble value, gpointer data)
{
	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(item));
	double page_size = gtk_adjustment_get_page_size(adj);
	double curr_value = gtk_adjustment_get_value(adj);

	if(react_to_scrollbar_change)
		gtk_trace_set_bounds(g_trace_widget, curr_value - page_size / 2.0, curr_value + page_size / 2.0);
}

G_MODULE_EXPORT void task_filter_button_clicked(GtkMenuItem *item, gpointer data)
{
	filter_clear_tasks(&g_filter);
	task_list_build_filter(GTK_TREE_VIEW(g_task_treeview), &g_filter);

	gtk_trace_set_filter(g_trace_widget, &g_filter);
	gtk_trace_paint(g_trace_widget);
}

G_MODULE_EXPORT void task_check_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	task_list_check_all(GTK_TREE_VIEW(g_task_treeview));
}

G_MODULE_EXPORT void task_uncheck_all_button_clicked(GtkMenuItem *item, gpointer data)
{
	task_list_uncheck_all(GTK_TREE_VIEW(g_task_treeview));
}

void show_task_code(struct task* t)
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
		goto out;
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

G_MODULE_EXPORT void task_treeview_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data)
{
	GtkTreeView* task_treeview = user_data;
	GtkTreeModel* model = gtk_tree_view_get_model(task_treeview);
	GtkTreeIter tree_iter;
	struct task* t;

	gtk_tree_model_get_iter(model, &tree_iter, path);
	gtk_tree_model_get(model, &tree_iter, TASK_LIST_COL_TASK_POINTER, &t, -1);

	if(!t->source_filename)
		return;

	show_task_code(t);
}
