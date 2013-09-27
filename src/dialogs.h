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

#ifndef DIALOGS_H
#define DIALOGS_H

#include <gtk/gtk.h>
#include "settings.h"
#include "multi_event_set.h"
#include "derived_counters.h"
#include "annotation.h"

struct progress_window_widgets {
	GtkWindow* window;
	GtkProgressBar* progressbar;
	GtkLabel* label_trace_bytes;
	GtkLabel* label_bytes_loaded;
	GtkLabel* label_seconds_remaining;
	GtkLabel* label_throughput;
};

void show_error_message(char* format, ...);

enum yes_no_cancel_dialog_response {
	DIALOG_RESPONSE_YES,
	DIALOG_RESPONSE_NO,
	DIALOG_RESPONSE_CANCEL
};

enum yes_no_cancel_dialog_response show_yes_no_cancel_dialog(char* format, ...);
char* load_save_file_dialog(const char* title, GtkFileChooserAction mode, const char* filter_name, const char* filter_extension, const char* default_dir);
int show_goto_dialog(double start, double end, double curr_value, double* time);
void show_about_dialog(void);
int show_settings_dialog(struct settings* s);
void show_progress_window_persistent(struct progress_window_widgets* widgets);
int show_color_dialog(GdkColor* color);

enum annotation_dialog_response {
	ANNOTATION_DIALOG_RESPONSE_OK,
	ANNOTATION_DIALOG_RESPONSE_CANCEL,
	ANNOTATION_DIALOG_RESPONSE_DELETE
};

enum annotation_dialog_response show_annotation_dialog(struct annotation* a, int edit);

enum derived_counter_type {
	DERIVED_COUNTER_PARALLELISM = 0,
	DERIVED_COUNTER_AGGREGATE,
	DERIVED_COUNTER_NUMA_CONTENTION
};

struct derived_counter_options {
	enum derived_counter_type type;
	unsigned int cpu;
	unsigned int counter_idx;
	unsigned int num_samples;
	unsigned int numa_node;
	char* name;
	enum worker_state state;
	enum access_type contention_type;
	enum access_model contention_model;
	enum source_type source_type;
};

int show_derived_counter_dialog(struct multi_event_set* mes, struct derived_counter_options* opt);

#endif
