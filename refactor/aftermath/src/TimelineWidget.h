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

#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include "CairoWidget.h"

extern "C" {
	#include <aftermath/render/timeline/renderer.h>
}

struct am_timeline_entity;

/**
 * Widget encapsulating the time line renderer showing events over time.
 */
class TimelineWidget : public CairoWidget {
	public:
		typedef CairoWidget super;
		class TimelineWidgetException {};

		TimelineWidget(QWidget* parent = NULL);
		virtual ~TimelineWidget();
		virtual void resizeEvent(QResizeEvent *event);
		void setTrace(struct am_trace* t);
		void setHierarchy(struct am_hierarchy* h);
		void addLayer(struct am_timeline_render_layer* l);

	protected:
		virtual void cairoPaintEvent(cairo_t* cr);

		virtual void mouseMoveEvent(QMouseEvent* event);
		virtual void mousePressEvent(QMouseEvent* event);

		virtual void handleMouseMoveHierarchyLayerItem(
			QMouseEvent* event,
			const struct am_timeline_entity* e);

		virtual void handleMousePressHierarchyLayerItem(
			QMouseEvent* event,
			const struct am_timeline_entity* e);

		struct am_timeline_renderer renderer;
};

#endif
