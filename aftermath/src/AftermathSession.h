/**
 * Author: Andi Drebes <andi@drebesium.org>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef AM_GUI_AFTERMATHSESSION_H
#define AM_GUI_AFTERMATHSESSION_H

#include <map>
#include <cstdint>
#include "Exception.h"
#include "gui/AftermathGUI.h"
#include "dfg/DFGQTProcessor.h"

extern "C" {
	#include <aftermath/core/trace.h>
	#include <aftermath/core/dfg_node_type_registry.h>
	#include <aftermath/core/dfg_type_registry.h>
	#include <aftermath/core/dfg_graph.h>
	#include <aftermath/render/dfg/dfg_coordinate_mapping.h>
	#include <aftermath/render/timeline/layer.h>
}

/* Arbitrary limit for the number of frame types; Might have to be changed in
 * the future */
#define AM_MAX_FRAME_TYPES 128

/* The AftermathSession class contains all the run-time data of an Aftermath
 * instance.
 */
class AftermathSession {
	public:
		class Exception : public AftermathException {
			public:
				Exception(const std::string& msg) :
					AftermathException(msg) { };
		};

		class RegisterDFGTypesException : public Exception {
			public:
				RegisterDFGTypesException() :
					Exception("Could not register builtin "
						  "DFG types.")
				{ }
		};

		class RegisterDFGNodeTypesException : public Exception {
			public:
				RegisterDFGNodeTypesException() :
					Exception("Could not register builtin "
						  "DFG node types.")
				{ }
		};

		class NoDFGException : public Exception {
			public:
				NoDFGException() :
					Exception("No DFG associated to this "
						  "session.")
				{ }
		};

		class DFGSchedulingException : public Exception {
			public:
				DFGSchedulingException() :
					Exception("Failed to schedule DFG.")
				{ }
		};

		AftermathSession();
		~AftermathSession();

		struct am_dfg_type_registry* getDFGTypeRegistry() noexcept;
		struct am_dfg_node_type_registry* getDFGNodeTypeRegistry() noexcept;
		struct am_trace* getTrace(unsigned id) noexcept;
		struct am_dfg_graph* getDFG() noexcept;
		struct am_dfg_coordinate_mapping* getDFGCoordinateMapping() noexcept;

		void setTrace(struct am_trace* t, unsigned id) noexcept;
		void setDFG(struct am_dfg_graph* g) noexcept;
		void setDFGCoordinateMapping(struct am_dfg_coordinate_mapping* m) noexcept;
		void scheduleDFG();

		struct am_timeline_render_layer_type_registry*
		getRenderLayerTypeRegistry() noexcept;

		AftermathGUI& getGUI();
		DFGQTProcessor& getDFGProcessor();
		DFGQTProcessor* getDFGProcessorp();

		void loadTrace(const char* filename, unsigned id);
		void loadDFG(const char* filename);

	protected:
		void cleanup();

		struct {
			struct am_dfg_graph* graph;
			struct am_dfg_type_registry type_registry;
			struct am_dfg_node_type_registry node_type_registry;
			struct am_dfg_coordinate_mapping* coordinate_mapping;
		} dfg;

		struct am_trace* trace[2];
		struct am_timeline_render_layer_type_registry rltr;

		AftermathGUI gui;
		DFGQTProcessor dfgProcessor;

		static int DFGNodeInstantiationCallback(
			struct am_dfg_node_type_registry* reg,
			struct am_dfg_node* n,
			void* data);
};

#endif
