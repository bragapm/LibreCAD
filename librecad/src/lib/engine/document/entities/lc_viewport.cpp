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
#include "rs_line.h"
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

    RS_Vector p1 = m_data.corner1;
    RS_Vector p3 = m_data.corner2;
    RS_Vector p2(p3.x, p1.y);
    RS_Vector p4(p1.x, p3.y);

    // When selected or highlighted, force the pen to be dashed.
    // The renderer has already applied the correct color for selection/highlight.
    if (isSelected() || getFlag(RS2::FlagHighlighted)) {
        RS_Pen pen = painter->getPen();
        pen.setLineType(RS2::DashLineTiny);
        pen.setWidth(RS2::Width00);
        painter->setPen(pen);
        painter->updateDashOffset(this);
    } else {
        // Draw rectangle border. Make it red and thicker if active, else use layer/renderer pen.
        RS_Pen originalPen = painter->getPen();
        RS2::LineWidth width = m_isActive ? RS2::Width07 : RS2::Width00;
        RS_Color color = m_isActive ? RS_Color(255, 0, 0) : originalPen.getColor();
        RS_Pen borderPen(color, width, RS2::SolidLine);
        painter->setPen(borderPen);
    }

    painter->drawLineWCS(p1, p2);
    painter->drawLineWCS(p2, p3);
    painter->drawLineWCS(p3, p4);
    painter->drawLineWCS(p4, p1);

    // Get GUI coordinates for text label
    double x1, y1, x2, y2;
    painter->toGui(m_data.corner1, x1, y1);
    painter->toGui(m_data.corner2, x2, y2);
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    // Only draw label when not selected (keep selection visual clean)
    if (!isSelected()) {
        QRect labelRect(x1 + 3, y1 + 3, 300, 20);
        QString coordStr = QString("(%1, %2)").arg(m_data.modelCenter.x, 0, 'f', 2).arg(m_data.modelCenter.y, 0, 'f', 2);
        QString labelText = m_isActive ? "VIEWPORT (ACTIVE) Center: " + coordStr : "VIEWPORT Center: " + coordStr;
        painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignTop, labelText, nullptr);
    }

    // Auto-fit calculations (rendering is now handled by LC_LayoutViewRenderer)
    if (m_modelGraphic != nullptr && !m_data.isCustomView) {
        m_modelGraphic->calculateBorders();
        RS_Vector modelMin = m_modelGraphic->getMin();
        RS_Vector modelMax = m_modelGraphic->getMax();

        if (modelMin.valid && modelMax.valid) {
            double modelWidth = modelMax.x - modelMin.x;
            double modelHeight = modelMax.y - modelMin.y;

            if (modelWidth > 1e-6 && modelHeight > 1e-6) {
                double vpWidth = std::abs(m_data.corner2.x - m_data.corner1.x);
                double vpHeight = std::abs(m_data.corner2.y - m_data.corner1.y);

                double scaleX = vpWidth / modelWidth;
                double scaleY = vpHeight / modelHeight;
                double scale = std::min(scaleX, scaleY) * 0.95; // 5% margin
                RS_Vector modelCenter = (modelMin + modelMax) * 0.5;
                
                // Save for future pan/zoom operations
                m_data.modelScale = scale;
                m_data.modelCenter = modelCenter;
            }
        }
    }
}

// double LC_Viewport::getDistanceToPoint(const RS_Vector& coord, RS_Entity** entity, RS2::ResolveLevel level, double solidDist) const {
//     RS_Vector p1(m_data.corner1.x, m_data.corner1.y);
//     RS_Vector p2(m_data.corner2.x, m_data.corner1.y);
//     RS_Vector p3(m_data.corner2.x, m_data.corner2.y);
//     RS_Vector p4(m_data.corner1.x, m_data.corner2.y);

//     RS_Line l1(nullptr, RS_LineData(p1, p2));
//     RS_Line l2(nullptr, RS_LineData(p2, p3));
//     RS_Line l3(nullptr, RS_LineData(p3, p4));
//     RS_Line l4(nullptr, RS_LineData(p4, p1));

//     double d1 = l1.getDistanceToPoint(coord);
//     double d2 = l2.getDistanceToPoint(coord);
//     double d3 = l3.getDistanceToPoint(coord);
//     double d4 = l4.getDistanceToPoint(coord);

//     double minDist = std::min({d1, d2, d3, d4});

//     if (entity) {
//         *entity = const_cast<LC_Viewport*>(this);
//     }
//     return minDist;
// }

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
