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

#ifndef LC_ACTIONDRAWVIEWPORT_H
#define LC_ACTIONDRAWVIEWPORT_H

#include "rs_previewactioninterface.h"

/**
 * Action to create a viewport in paper space.
 * User picks two corners to define a rectangle viewport.
 * The viewport will display model space content.
 */
class LC_ActionDrawViewport : public RS_PreviewActionInterface {
    Q_OBJECT
public:
    LC_ActionDrawViewport(LC_ActionContext* actionContext);
    ~LC_ActionDrawViewport() override;

protected:
    enum Status {
        SetCorner1,  ///< Picking first corner
        SetCorner2   ///< Picking second corner (opposite corner)
    };

    RS2::CursorType doGetMouseCursor(int status) override;
    void onMouseLeftButtonRelease(int status, LC_MouseEvent* e) override;
    void onMouseRightButtonRelease(int status, LC_MouseEvent* e) override;
    void onMouseMoveEvent(int status, LC_MouseEvent* event) override;
    void onCoordinateEvent(int status, bool isZero, const RS_Vector& pos) override;
    void updateMouseButtonHints() override;
    void doTrigger() override;

private:
    RS_Vector m_corner1;
    RS_Vector m_corner2;
};

#endif // LC_ACTIONDRAWVIEWPORT_H
