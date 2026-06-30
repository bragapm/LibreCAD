/*******************************************************************************
 *
 This file is part of the LibreCAD project, a 2D CAD program

 Copyright (C) 2025 LibreCAD.org

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ******************************************************************************/

#include "lc_actiondrawviewport.h"

#include "lc_actioncontext.h"
#include "lc_viewport.h"
#include "rs_graphic.h"
#include "rs_graphicview.h"
#include "rs_debug.h"
#include "rs_block.h"
#include "rs_dialogfactory.h"

LC_ActionDrawViewport::LC_ActionDrawViewport(LC_ActionContext* actionContext)
    : RS_PreviewActionInterface("Draw Viewport", actionContext, RS2::ActionNone) {
}

LC_ActionDrawViewport::~LC_ActionDrawViewport() = default;

RS2::CursorType LC_ActionDrawViewport::doGetMouseCursor(int /*status*/) {
    return RS2::CadCursor;
}

void LC_ActionDrawViewport::updateMouseButtonHints() {
    switch (getStatus()) {
        case SetCorner1:
            updateMouseWidgetTRCancel(tr("Specify first corner of viewport"));
            break;
        case SetCorner2:
            updateMouseWidgetTRCancel(tr("Specify opposite corner of viewport"));
            break;
        default:
            break;
    }
}

void LC_ActionDrawViewport::onMouseMoveEvent(int status, LC_MouseEvent* e) {
    RS_Vector mouse = e->snapPoint;

    switch (status) {
        case SetCorner1:
            trySnapToRelZeroCoordinateEvent(e);
            break;
        case SetCorner2: {
            if (m_corner1.valid) {
                deletePreview();
                // Preview rectangle
                previewToCreateLine(m_corner1, RS_Vector(mouse.x, m_corner1.y));
                previewToCreateLine(RS_Vector(mouse.x, m_corner1.y), mouse);
                previewToCreateLine(mouse, RS_Vector(m_corner1.x, mouse.y));
                previewToCreateLine(RS_Vector(m_corner1.x, mouse.y), m_corner1);
                drawPreview();
            }
            break;
        }
        default:
            break;
    }
}

void LC_ActionDrawViewport::onMouseLeftButtonRelease(int status, LC_MouseEvent* e) {
    fireCoordinateEvent(e->snapPoint);
}

void LC_ActionDrawViewport::onMouseRightButtonRelease(int status, [[maybe_unused]] LC_MouseEvent* e) {
    deletePreview();
    switch (status) {
        default:
        case SetCorner1:
            initPrevious(status);
            break;
        case SetCorner2:
            setStatus(SetCorner1);
            break;
    }
}

void LC_ActionDrawViewport::onCoordinateEvent(int status, bool /*isZero*/, const RS_Vector& pos) {
    switch (status) {
        case SetCorner1:
            m_corner1 = pos;
            moveRelativeZero(pos);
            setStatus(SetCorner2);
            break;
        case SetCorner2:
            m_corner2 = pos;
            trigger();
            break;
        default:
            break;
    }
}

void LC_ActionDrawViewport::doTrigger() {
    if (!m_corner1.valid || !m_corner2.valid) return;
    if ((m_corner2 - m_corner1).magnitude() < RS_TOLERANCE) return;

    // Only allow viewports in Paper Space
    auto* blockContainer = dynamic_cast<RS_Block*>(getContainer());
    if (blockContainer == nullptr || blockContainer->getName() != "*Paper_Space") {
        commandMessage(tr("Viewports can only be created in the Layout tab."));
        setStatus(SetCorner1);
        finish();
        return;
    }

    // Only allow one viewport per layout
    for (RS_Entity* e : *getContainer()) {
        if (e && e->rtti() == RS2::EntityViewport) {
            commandMessage(tr("Only one viewport is allowed per layout."));
            setStatus(SetCorner1);
            finish();
            return;
        }
    }

    // Get the model graphic (model space)
    RS_Graphic* modelGraphic = m_graphic;

    LC_ViewportData data(m_corner1, m_corner2);
    auto* viewport = new LC_Viewport(getContainer(), data);
    viewport->setModelGraphic(modelGraphic);
    setPenAndLayerToActive(viewport);
    undoCycleAdd(viewport);

    RS_DEBUG->print("LC_ActionDrawViewport: viewport added");

    // Reset for next viewport
    m_corner1 = RS_Vector(false);
    m_corner2 = RS_Vector(false);
    setStatus(SetCorner1);
}
