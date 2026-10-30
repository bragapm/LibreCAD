/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2026 LibreCAD.org
**
** This file is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
**********************************************************************/

#ifndef QC_ACTIONGETROTATE_H
#define QC_ACTIONGETROTATE_H

#include <QList>
#include <QPointF>
#include <vector>
#include "lc_overlayboxaction.h"

/**
 * AutoCAD-style interactive Rotate action for plugins.
 * Supports:
 *  - Object selection (single click, Ctrl+click quick select, Window/Crossing box)
 *  - Base point selection (with Shift/Ctrl snap to selection center/centroid)
 *  - Real-time rotated entity preview following the mouse cursor
 *  - Direct numeric angle input on the command line (e.g. 45, -30, 90)
 *  - [C]opy option to keep original entities
 *  - [R]eference option to align an existing direction to a target direction
 *  - Remembering the last rotation angle (<default> on Enter)
 */
class QC_ActionGetRotate : public LC_OverlayBoxAction {
    Q_OBJECT
public:
    enum Status {
        SelectObjects = 0,
        SetBasePoint = 1,
        SetRotationAngle = 2,
        SetReferencePoint1 = 3,
        SetReferencePoint2 = 4
    };

    explicit QC_ActionGetRotate(LC_ActionContext *actionContext);
    ~QC_ActionGetRotate() override;

    void init(int status = 0) override;
    void trigger() override;
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

    void setInitialEntities(const QList<qulonglong> &entityIds);
    void getResult(QList<qulonglong> *entityIds,
                   QPointF *basePoint,
                   double *angleRad,
                   bool *copyMode) const;

    bool isCompleted() const { return m_completed; }
    bool wasCanceled() const { return m_canceled; }

protected:
    RS2::CursorType doGetMouseCursor(int status) override;
    void onMouseMoveEvent(int status, LC_MouseEvent *e) override;
    void onMouseLeftButtonRelease(int status, LC_MouseEvent *e) override;
    void onMouseRightButtonRelease(int status, LC_MouseEvent *e) override;
    void onCoordinateEvent(int status, bool isZero, const RS_Vector &pos) override;
    bool doProcessCommand(int status, const QString &command) override;
    QStringList doGetAvailableCommands(int status) override;
    void updateMouseButtonHints() override;

private:
    void collectSelectedEntities();
    void updateSelectionCenter();
    void highlightSelectedEntities();

    bool m_completed = false;
    bool m_canceled = false;
    bool m_copyMode = false;
    bool m_hasReference = false;
    bool m_inBoxSelection = false;

    RS_Vector m_boxCorner1{false};
    RS_Vector m_selectionCenter{false};
    RS_Vector m_basePoint{false};
    RS_Vector m_refStartPoint{false};

    double m_referenceAngle = 0.0;
    double m_rotationAngle = 0.0;

    std::vector<RS_Entity *> m_selectedEntities;
    QList<qulonglong> m_initialEntityIds;

    static double s_lastAngleDeg;
};

#endif // QC_ACTIONGETROTATE_H
