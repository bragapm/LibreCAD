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

#include "lc_viewport.h"
#include "rs_painter.h"
#include "rs_graphic.h"
#include "rs_math.h"

LC_Viewport::LC_Viewport(RS_EntityContainer* parent, const LC_ViewportData& d)
    : RS_AtomicEntity(parent)
    , m_data(d) {
    calculateBorders();
}

RS_Entity* LC_Viewport::clone() const {
    auto* v = new LC_Viewport(parent, m_data);
    v->m_modelGraphic = m_modelGraphic;
    v->initId();
    return v;
}

void LC_Viewport::calculateBorders() {
    minV = RS_Vector(std::min(m_data.corner1.x, m_data.corner2.x),
                     std::min(m_data.corner1.y, m_data.corner2.y));
    maxV = RS_Vector(std::max(m_data.corner1.x, m_data.corner2.x),
                     std::max(m_data.corner1.y, m_data.corner2.y));
}

void LC_Viewport::draw(RS_Painter* painter) {
    if (painter == nullptr) return;

    // Draw viewport border as a rectangle
    double x1, y1, x2, y2;
    painter->toGui(m_data.corner1, x1, y1);
    painter->toGui(m_data.corner2, x2, y2);

    // Ensure correct order
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    // Draw dashed rectangle border. Make it thicker if active.
    RS2::LineWidth width = m_isActive ? RS2::Width05 : RS2::Width00;
    RS_Pen borderPen(RS_Color(0, 100, 200), width, RS2::SolidLine);
    painter->setPen(borderPen);
    painter->drawLineUISimple(x1, y1, x2, y1);
    painter->drawLineUISimple(x2, y1, x2, y2);
    painter->drawLineUISimple(x2, y2, x1, y2);
    painter->drawLineUISimple(x1, y2, x1, y1);

    // Label "VIEWPORT" in top-left corner
    QRect labelRect(x1 + 3, y1 + 3, 100, 20);
    painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignTop, "VIEWPORT", nullptr);

    // Render the model space contents
    if (m_modelGraphic != nullptr) {
        m_modelGraphic->calculateBorders();
        RS_Vector modelMin = m_modelGraphic->getMin();
        RS_Vector modelMax = m_modelGraphic->getMax();

        if (modelMin.valid && modelMax.valid) {
            double modelWidth = modelMax.x - modelMin.x;
            double modelHeight = modelMax.y - modelMin.y;

            if (modelWidth > 1e-6 && modelHeight > 1e-6) {
                double vpWidth = std::abs(m_data.corner2.x - m_data.corner1.x);
                double vpHeight = std::abs(m_data.corner2.y - m_data.corner1.y);

                double scale = 1.0;
                RS_Vector modelCenter;

                if (!m_data.isCustomView) {
                    double scaleX = vpWidth / modelWidth;
                    double scaleY = vpHeight / modelHeight;
                    scale = std::min(scaleX, scaleY) * 0.95; // 5% margin
                    modelCenter = (modelMin + modelMax) * 0.5;
                    
                    // Save for future pan/zoom operations
                    m_data.modelScale = scale;
                    m_data.modelCenter = modelCenter;
                } else {
                    scale = m_data.modelScale;
                    modelCenter = m_data.modelCenter;
                }

                RS_Vector vpCenter = (m_data.corner1 + m_data.corner2) * 0.5;

                painter->save();

                // 1. Set clipping to viewport bounds
                painter->setClipRect(x1, y1, x2 - x1, y2 - y1);

                // 2. Calculate QPainter transform mapping model center to viewport center
                double guiModelCenterX, guiModelCenterY;
                painter->toGui(modelCenter, guiModelCenterX, guiModelCenterY);

                double guiVpCenterX, guiVpCenterY;
                painter->toGui(vpCenter, guiVpCenterX, guiVpCenterY);

                double tx = guiVpCenterX - scale * guiModelCenterX;
                double ty = guiVpCenterY - scale * guiModelCenterY;

                // 3. Apply transformation
                painter->translate(tx, ty);
                painter->scale(scale, scale);

                // 4. Draw all visible entities from model space
                for (auto* e : *m_modelGraphic) {
                    if (e && e->isVisible()) {
                        e->draw(painter);
                    }
                }

                painter->restore();
            }
        }
    }
}

RS_Vector LC_Viewport::getNearestEndpoint(const RS_Vector& coord, double* dist) const {
    // Return nearest of the 4 corners
    RS_Vector corners[4] = {
        m_data.corner1,
        RS_Vector(m_data.corner2.x, m_data.corner1.y),
        m_data.corner2,
        RS_Vector(m_data.corner1.x, m_data.corner2.y)
    };

    double minDist = RS_MAXDOUBLE;
    RS_Vector nearest(false);

    for (const auto& corner : corners) {
        double d = (coord - corner).magnitude();
        if (d < minDist) {
            minDist = d;
            nearest = corner;
        }
    }

    if (dist) *dist = minDist;
    return nearest;
}

RS_Vector LC_Viewport::getNearestPointOnEntity(const RS_Vector& coord, bool /*onEntity*/,
                                               double* dist, RS_Entity** /*entity*/) const {
    return getNearestEndpoint(coord, dist);
}

RS_Vector LC_Viewport::getNearestCenter(const RS_Vector& coord, double* dist) const {
    RS_Vector center = (m_data.corner1 + m_data.corner2) * 0.5;
    if (dist) *dist = (coord - center).magnitude();
    return center;
}

RS_Vector LC_Viewport::getNearestMiddle(const RS_Vector& coord, double* dist, int /*middlePoints*/) const {
    return getNearestCenter(coord, dist);
}

RS_Vector LC_Viewport::getNearestDist(double /*distance*/, const RS_Vector& coord, double* dist) const {
    return getNearestEndpoint(coord, dist);
}

void LC_Viewport::move(const RS_Vector& offset) {
    m_data.corner1 += offset;
    m_data.corner2 += offset;
    calculateBorders();
}

void LC_Viewport::rotate(const RS_Vector& center, double angle) {
    m_data.corner1.rotate(center, angle);
    m_data.corner2.rotate(center, angle);
    calculateBorders();
}

void LC_Viewport::rotate(const RS_Vector& center, const RS_Vector& angleVector) {
    m_data.corner1.rotate(center, angleVector);
    m_data.corner2.rotate(center, angleVector);
    calculateBorders();
}

void LC_Viewport::scale(const RS_Vector& center, const RS_Vector& factor) {
    m_data.corner1.scale(center, factor);
    m_data.corner2.scale(center, factor);
    m_data.scale *= factor.x;
    calculateBorders();
}

void LC_Viewport::mirror(const RS_Vector& axisPoint1, const RS_Vector& axisPoint2) {
    m_data.corner1.mirror(axisPoint1, axisPoint2);
    m_data.corner2.mirror(axisPoint1, axisPoint2);
    calculateBorders();
}

RS_Entity& LC_Viewport::shear(double k) {
    // Simple shear for corner1 and corner2
    m_data.corner1.y += k * m_data.corner1.x;
    m_data.corner2.y += k * m_data.corner2.x;
    calculateBorders();
    return *this;
}
