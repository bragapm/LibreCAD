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

#ifndef LC_VIEWPORT_H
#define LC_VIEWPORT_H

#include "rs_atomicentity.h"
#include "rs_vector.h"

class RS_Graphic;

/**
 * Data that defines a viewport in paper space.
 */
struct LC_ViewportData {
    LC_ViewportData() = default;
    LC_ViewportData(const RS_Vector& corner1, const RS_Vector& corner2)
        : corner1(corner1), corner2(corner2) {}

    RS_Vector corner1;        ///< Bottom-left corner in paper space
    RS_Vector corner2;        ///< Top-right corner in paper space
    double scale {1.0};       ///< Display scale factor (deprecated, use modelScale)
    
    RS_Vector modelCenter;    ///< Center of the model view
    double modelScale {1.0};  ///< Scale factor of the model view
    bool isCustomView {false};///< True if user has panned or zoomed manually
};

/**
 * A viewport entity representing a rectangular window in paper space
 * that displays model space content.
 *
 * Viewports are stored in the *Paper_Space block and reference
 * the model space (RS_Graphic) for rendering.
 */
class LC_Viewport : public RS_AtomicEntity {
public:
    LC_Viewport(RS_EntityContainer* parent, const LC_ViewportData& d);

    RS_Entity* clone() const override;

    RS2::EntityType rtti() const override {
        return RS2::EntityOverlayBox; // Reuse available type for now
    }

    bool isViewport() const { return true; }

    const LC_ViewportData& getData() const { return m_data; }

    RS_Vector getCorner1() const { return m_data.corner1; }
    RS_Vector getCorner2() const { return m_data.corner2; }
    
    void setActive(bool active) { m_isActive = active; }
    bool isActive() const { return m_isActive; }
    
    void setCustomView(bool custom) { m_data.isCustomView = custom; }
    void setModelCenter(const RS_Vector& center) { m_data.modelCenter = center; }
    void setModelScale(double scale) { m_data.modelScale = scale; }
    
    RS_Vector getModelCenter() const { return m_data.modelCenter; }
    double getModelScale() const { return m_data.modelScale; }

    RS_Vector getNearestEndpoint(const RS_Vector& coord, double* dist = nullptr) const override;
    RS_Vector getNearestPointOnEntity(const RS_Vector& coord, bool onEntity = true,
                                      double* dist = nullptr, RS_Entity** entity = nullptr) const override;
    RS_Vector getNearestCenter(const RS_Vector& coord, double* dist = nullptr) const override;
    RS_Vector getNearestMiddle(const RS_Vector& coord, double* dist = nullptr,
                               int middlePoints = 1) const override;
    RS_Vector getNearestDist(double distance, const RS_Vector& coord,
                             double* dist = nullptr) const override;

    void draw(RS_Painter* painter) override;
    void calculateBorders() override;

    //double getDistanceToPoint(const RS_Vector& coord, RS_Entity** entity = nullptr, RS2::ResolveLevel level = RS2::ResolveNone, double solidDist = RS_MAXDOUBLE) const override;
    void move(const RS_Vector& offset) override;
    void rotate(const RS_Vector& center, double angle) override;
    void rotate(const RS_Vector& center, const RS_Vector& angleVector) override;
    void scale(const RS_Vector& center, const RS_Vector& factor) override;
    void mirror(const RS_Vector& axisPoint1, const RS_Vector& axisPoint2) override;
    RS_Entity& shear(double k) override;

    unsigned int count() const override { return 1; }
    unsigned int countDeep() const override { return 1; }

    // Set reference to graphic (model space) for rendering
    void setModelGraphic(RS_Graphic* graphic) { m_modelGraphic = graphic; }
    RS_Graphic* getModelGraphic() const { return m_modelGraphic; }

private:
    LC_ViewportData m_data;
    RS_Graphic* m_modelGraphic {nullptr};
    bool m_isActive {false};
};

#endif // LC_VIEWPORT_H
