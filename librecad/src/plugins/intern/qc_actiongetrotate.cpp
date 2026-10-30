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

#include "qc_actiongetrotate.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <cmath>

#include "lc_actioninfomessagebuilder.h"
#include "lc_graphicviewport.h"
#include "rs_arc.h"
#include "rs_commandevent.h"
#include "rs_coordinateevent.h"
#include "rs_debug.h"
#include "rs_document.h"
#include "rs_entity.h"
#include "rs_entitycontainer.h"
#include "rs_graphicview.h"
#include "rs_layer.h"
#include "rs_line.h"
#include "rs_math.h"
#include "rs_modification.h"
#include "rs_preview.h"
#include "rs_selection.h"

double QC_ActionGetRotate::s_lastAngleDeg = 0.0;

QC_ActionGetRotate::QC_ActionGetRotate(LC_ActionContext *actionContext)
    : LC_OverlayBoxAction("Get Rotate", actionContext, RS2::ActionNone)
{
    m_showRefEntitiesOnPreview = true;
}

QC_ActionGetRotate::~QC_ActionGetRotate() = default;

void QC_ActionGetRotate::setInitialEntities(const QList<qulonglong> &entityIds)
{
    m_initialEntityIds = entityIds;
}

void QC_ActionGetRotate::init(int status)
{
    LC_OverlayBoxAction::init(status);
    m_showRefEntitiesOnPreview = true;

    if (status == 0) {
        if (!m_initialEntityIds.isEmpty() && getContainer()) {
            for (RS_Entity *e : *getContainer()) {
                if (e && m_initialEntityIds.contains(e->getId())) {
                    e->setSelected(true);
                }
            }
        }

        collectSelectedEntities();
        if (!m_selectedEntities.empty()) {
            updateSelectionCenter();
            setStatus(SetBasePoint);
            commandMessage(tr("ROTATE: %1 objek terpilih. Tentukan titik poros (Base Point) [Shift+Klik = Pusat Objek]:")
                               .arg(m_selectedEntities.size()));
        } else {
            setStatus(SelectObjects);
            commandMessage(tr("ROTATE: Pilih objek (Klik objek / drag kotak seleksi, lalu tekan Enter atau Klik Kanan untuk lanjut):"));
        }
    }
    updateMouseButtonHints();
}

void QC_ActionGetRotate::collectSelectedEntities()
{
    m_selectedEntities.clear();
    RS_EntityContainer *container = getContainer();
    if (!container) return;

    for (RS_Entity *e : *container) {
        if (e && e->isVisible() && !e->isUndone() && e->isSelected()) {
            if (e->getLayer() && e->getLayer()->isLocked()) continue;
            m_selectedEntities.push_back(e);
        }
    }
}

void QC_ActionGetRotate::updateSelectionCenter()
{
    if (m_selectedEntities.empty()) {
        m_selectionCenter = RS_Vector(false);
        return;
    }
    RS_BoundData bound = RS_Modification::getBoundingRect(m_selectedEntities);
    m_selectionCenter = bound.getCenter();
}

void QC_ActionGetRotate::highlightSelectedEntities()
{
    for (RS_Entity *en : m_selectedEntities) {
        if (en) {
            highlightSelected(en, true);
        }
    }
}

void QC_ActionGetRotate::trigger()
{
    m_completed = true;
    updateMouseButtonHints();
    finish(false);
}

RS2::CursorType QC_ActionGetRotate::doGetMouseCursor(int status)
{
    if (status == SelectObjects) {
        return RS2::SelectCursor;
    }
    return RS2::CadCursor;
}

void QC_ActionGetRotate::mousePressEvent(QMouseEvent *e)
{
    if (getStatus() == SelectObjects && e->button() == Qt::LeftButton) {
        m_boxCorner1 = toGraph(e);
        m_inBoxSelection = false;
    } else {
        LC_OverlayBoxAction::mousePressEvent(e);
    }
}

void QC_ActionGetRotate::onMouseMoveEvent(int status, LC_MouseEvent *e)
{
    highlightSelectedEntities();

    switch (status) {
        case SelectObjects: {
            RS_Vector mouse = e->graphPoint;
            if (m_boxCorner1.valid && (m_viewport->toGuiDX(m_boxCorner1.distanceTo(mouse)) > 10.0)) {
                m_inBoxSelection = true;
            }
            if (m_inBoxSelection) {
                drawOverlayBox(m_boxCorner1, mouse);
            } else {
                RS_Entity *hoverEnt = catchAndDescribe(e, RS2::ResolveNone);
                if (hoverEnt && hoverEnt->isVisible() && (!hoverEnt->getLayer() || !hoverEnt->getLayer()->isLocked())) {
                    highlightHover(hoverEnt);
                }
            }
            break;
        }
        case SetBasePoint: {
            RS_Vector mouse = e->snapPoint;
            if ((e->isShift || e->isControl) && m_selectionCenter.valid) {
                mouse = m_selectionCenter;
            }
            if (m_selectionCenter.valid) {
                previewRefLine(m_selectionCenter, mouse);
                previewRefPoint(m_selectionCenter);
            }
            previewRefSelectablePoint(mouse);
            break;
        }
        case SetRotationAngle: {
            RS_Vector mouse = getSnapAngleAwarePoint(e, m_basePoint, e->snapPoint, true);
            double cursorAngle = m_basePoint.angleTo(mouse);
            double previewAngle = m_hasReference ? (cursorAngle - m_referenceAngle) : cursorAngle;

            for (RS_Entity *en : m_selectedEntities) {
                if (!en) continue;
                RS_Entity *clone = en->clone();
                clone->rotate(m_basePoint, previewAngle);
                m_preview->addEntity(clone);
            }

            previewRefPoint(m_basePoint);
            previewRefLine(m_basePoint, mouse);
            previewRefSelectablePoint(mouse);

            double radius = m_basePoint.distanceTo(mouse);
            if (radius > 1e-4) {
                double startAng = m_hasReference ? m_referenceAngle : 0.0;
                RS_Vector refEnd = m_basePoint.relative(radius, startAng);
                previewRefLine(m_basePoint, refEnd);
                previewRefPoint(refEnd);
                previewRefArc(RS_ArcData(m_basePoint, radius * 0.35, startAng, cursorAngle, false));
            }

            if (isInfoCursorForModificationEnabled()) {
                double deg = RS_Math::rad2deg(RS_Math::correctAnglePlusMinusPi(previewAngle));
                msg(m_copyMode ? tr("Rotate (Copy)") : tr("Rotate"))
                    .string(tr("Sudut: %1°").arg(deg, 0, 'f', 2))
                    .vector(tr("Base Point:"), m_basePoint)
                    .toInfoCursorZone2(false);
            }
            break;
        }
        case SetReferencePoint1: {
            RS_Vector mouse = getSnapAngleAwarePoint(e, m_basePoint, e->snapPoint, true);
            previewRefPoint(m_basePoint);
            previewRefLine(m_basePoint, mouse);
            previewRefSelectablePoint(mouse);
            break;
        }
        case SetReferencePoint2: {
            RS_Vector mouse = getSnapAngleAwarePoint(e, m_refStartPoint, e->snapPoint, true);
            previewRefPoint(m_refStartPoint);
            previewRefLine(m_refStartPoint, mouse);
            previewRefSelectablePoint(mouse);
            break;
        }
        default:
            break;
    }
}

void QC_ActionGetRotate::onMouseLeftButtonRelease(int status, LC_MouseEvent *e)
{
    switch (status) {
        case SelectObjects: {
            if (m_inBoxSelection) {
                RS_Vector mouse = e->graphPoint;
                deletePreview();

                RS_Vector ucsP1 = toUCS(m_boxCorner1);
                RS_Vector ucsP2 = toUCS(mouse);
                bool selectIntersecting = (ucsP1.x > ucsP2.x);
                if (e->isControl) {
                    selectIntersecting = !selectIntersecting;
                }
                bool performSelection = !e->isShift;

                RS_Vector wcsP1, wcsP2;
                m_viewport->worldBoundingBox(ucsP1, ucsP2, wcsP1, wcsP2);

                RS_Selection s(*getContainer(), m_viewport);
                s.selectWindow(RS2::EntityUnknown, wcsP1, wcsP2, performSelection, selectIntersecting);
                updateSelectionWidget();
            } else {
                RS_Entity *en = catchEntityByEvent(e, RS2::ResolveNone);
                if (en && en->isVisible() && (!en->getLayer() || !en->getLayer()->isLocked())) {
                    RS_Selection s(*getContainer(), m_viewport);
                    s.selectSingle(en);
                    updateSelectionWidget();
                    if (e->isControl) {
                        collectSelectedEntities();
                        if (!m_selectedEntities.empty()) {
                            updateSelectionCenter();
                            setStatus(SetBasePoint);
                            commandMessage(tr("ROTATE: %1 objek terpilih. Tentukan titik poros (Base Point):").arg(m_selectedEntities.size()));
                            m_inBoxSelection = false;
                            m_boxCorner1.valid = false;
                            invalidateSnapSpot();
                            updateMouseButtonHints();
                            return;
                        }
                    }
                }
            }
            m_inBoxSelection = false;
            m_boxCorner1.valid = false;
            invalidateSnapSpot();
            collectSelectedEntities();
            updateMouseButtonHints();
            break;
        }
        case SetBasePoint: {
            RS_Vector pos = e->snapPoint;
            if ((e->isShift || e->isControl) && m_selectionCenter.valid) {
                pos = m_selectionCenter;
            }
            fireCoordinateEvent(pos);
            break;
        }
        case SetRotationAngle:
        case SetReferencePoint1: {
            RS_Vector pos = getSnapAngleAwarePoint(e, m_basePoint, e->snapPoint, false);
            fireCoordinateEvent(pos);
            break;
        }
        case SetReferencePoint2: {
            RS_Vector pos = getSnapAngleAwarePoint(e, m_refStartPoint, e->snapPoint, false);
            fireCoordinateEvent(pos);
            break;
        }
        default:
            break;
    }
}

void QC_ActionGetRotate::onMouseRightButtonRelease(int status, [[maybe_unused]] LC_MouseEvent *e)
{
    if (status == SelectObjects) {
        collectSelectedEntities();
        if (!m_selectedEntities.empty()) {
            updateSelectionCenter();
            setStatus(SetBasePoint);
            commandMessage(tr("ROTATE: %1 objek terpilih. Tentukan titik poros (Base Point) [Shift+Klik = Pusat Objek]:")
                               .arg(m_selectedEntities.size()));
            updateMouseButtonHints();
            return;
        }
    }
    m_canceled = true;
    m_completed = true;
    updateMouseButtonHints();
    finish(false);
}

void QC_ActionGetRotate::onCoordinateEvent(int status, [[maybe_unused]] bool isZero, const RS_Vector &pos)
{
    if (!pos.valid) return;

    switch (status) {
        case SetBasePoint: {
            m_basePoint = pos;
            moveRelativeZero(m_basePoint);
            setStatus(SetRotationAngle);
            commandMessage(tr("ROTATE: Specify rotation angle or [Copy/Reference] <%1°>:")
                               .arg(s_lastAngleDeg, 0, 'f', 2));
            updateMouseButtonHints();
            break;
        }
        case SetRotationAngle: {
            if (m_basePoint.distanceTo(pos) <= 1e-6) {
                commandMessage(tr("ROTATE: Titik arah tidak boleh sama dengan titik poros."));
                return;
            }
            double cursorAngle = m_basePoint.angleTo(pos);
            m_rotationAngle = m_hasReference ? (cursorAngle - m_referenceAngle) : cursorAngle;
            s_lastAngleDeg = RS_Math::rad2deg(RS_Math::correctAnglePlusMinusPi(m_rotationAngle));
            moveRelativeZero(pos);
            trigger();
            break;
        }
        case SetReferencePoint1: {
            if (m_basePoint.distanceTo(pos) <= 1e-4) {
                m_refStartPoint = m_basePoint;
                setStatus(SetReferencePoint2);
                commandMessage(tr("ROTATE Reference: Specify second point:"));
            } else {
                m_referenceAngle = m_basePoint.angleTo(pos);
                m_hasReference = true;
                setStatus(SetRotationAngle);
                commandMessage(tr("ROTATE Reference: Specify the new angle <%1°>:")
                                   .arg(s_lastAngleDeg, 0, 'f', 2));
            }
            updateMouseButtonHints();
            break;
        }
        case SetReferencePoint2: {
            if (m_refStartPoint.distanceTo(pos) <= 1e-6) {
                commandMessage(tr("ROTATE Reference: Titik kedua tidak boleh sama dengan titik pertama."));
                return;
            }
            m_referenceAngle = m_refStartPoint.angleTo(pos);
            m_hasReference = true;
            setStatus(SetRotationAngle);
            commandMessage(tr("ROTATE Reference: Specify the new angle <%1°>:")
                               .arg(s_lastAngleDeg, 0, 'f', 2));
            updateMouseButtonHints();
            break;
        }
        default:
            break;
    }
}

bool QC_ActionGetRotate::doProcessCommand(int status, const QString &command)
{
    QString cmd = command.trimmed().toLower();
    if (cmd.isEmpty()) return false;

    if (status == SelectObjects) {
        if (cmd == "all") {
            RS_Selection s(*getContainer(), m_viewport);
            s.selectAll(true);
            collectSelectedEntities();
            updateSelectionWidget();
            updateMouseButtonHints();
            return true;
        }
        return false;
    }

    if (status == SetRotationAngle) {
        if (cmd == "c" || cmd == "copy") {
            m_copyMode = !m_copyMode;
            commandMessage(m_copyMode
                               ? tr("ROTATE: Mode Copy AKTIF (objek asli dipertahankan).")
                               : tr("ROTATE: Mode Copy NONAKTIF (objek asli diputar)."));
            updateMouseButtonHints();
            return true;
        }
        if (cmd == "r" || cmd == "ref" || cmd == "reference") {
            m_refStartPoint = m_basePoint;
            setStatus(SetReferencePoint1);
            commandMessage(tr("ROTATE Reference: Specify the reference angle <0°> (klik titik acuan atau ketik sudut):"));
            updateMouseButtonHints();
            return true;
        }

        bool ok = false;
        double degVal = RS_Math::eval(command, &ok);
        if (ok) {
            double radVal = RS_Math::deg2rad(degVal);
            m_rotationAngle = m_hasReference ? (radVal - m_referenceAngle) : radVal;
            s_lastAngleDeg = degVal;
            trigger();
            return true;
        }
    } else if (status == SetReferencePoint1) {
        bool ok = false;
        double degVal = RS_Math::eval(command, &ok);
        if (ok) {
            m_referenceAngle = RS_Math::deg2rad(degVal);
            m_hasReference = true;
            setStatus(SetRotationAngle);
            commandMessage(tr("ROTATE Reference: Specify the new angle <%1°>:")
                               .arg(s_lastAngleDeg, 0, 'f', 2));
            updateMouseButtonHints();
            return true;
        }
    }

    return false;
}

QStringList QC_ActionGetRotate::doGetAvailableCommands(int status)
{
    if (status == SetRotationAngle) {
        return {"copy", "reference"};
    }
    if (status == SelectObjects) {
        return {"all"};
    }
    return {};
}

void QC_ActionGetRotate::keyPressEvent(QKeyEvent *e)
{
    int key = e->key();
    int status = getStatus();

    if (key == Qt::Key_Escape) {
        m_canceled = true;
        m_completed = true;
        updateMouseButtonHints();
        finish(false);
        e->accept();
        return;
    }

    if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
        if (status == SelectObjects) {
            collectSelectedEntities();
            if (!m_selectedEntities.empty()) {
                updateSelectionCenter();
                setStatus(SetBasePoint);
                commandMessage(tr("ROTATE: %1 objek terpilih. Tentukan titik poros (Base Point) [Shift+Klik = Pusat Objek]:")
                                   .arg(m_selectedEntities.size()));
                updateMouseButtonHints();
                e->accept();
                return;
            }
        } else if (status == SetRotationAngle) {
            double radVal = RS_Math::deg2rad(s_lastAngleDeg);
            m_rotationAngle = m_hasReference ? (radVal - m_referenceAngle) : radVal;
            trigger();
            e->accept();
            return;
        } else if (status == SetReferencePoint1) {
            m_referenceAngle = 0.0;
            m_hasReference = true;
            setStatus(SetRotationAngle);
            commandMessage(tr("ROTATE Reference: Specify the new angle <%1°>:")
                               .arg(s_lastAngleDeg, 0, 'f', 2));
            updateMouseButtonHints();
            e->accept();
            return;
        }
    }

    LC_OverlayBoxAction::keyPressEvent(e);
}

void QC_ActionGetRotate::updateMouseButtonHints()
{
    if (m_completed) {
        updateMouseWidget();
        return;
    }

    switch (getStatus()) {
        case SelectObjects:
            updateMouseWidget(
                tr("Select objects (%1 selected) [Ctrl+Click = Quick Select, Enter/Right-Click = Done]")
                    .arg(m_selectedEntities.size()),
                tr("Done / Cancel"));
            break;
        case SetBasePoint:
            updateMouseWidget(
                tr("Specify base point [Shift+Click = Selection Center]"),
                tr("Cancel"));
            break;
        case SetRotationAngle: {
            QString modePrefix = m_copyMode ? tr("[COPY] ") : QString();
            QString prompt = m_hasReference
                                 ? tr("%1Specify new angle or [Copy/Reference] <%2°>:")
                                       .arg(modePrefix)
                                       .arg(s_lastAngleDeg, 0, 'f', 2)
                                 : tr("%1Specify rotation angle or [Copy/Reference] <%2°>:")
                                       .arg(modePrefix)
                                       .arg(s_lastAngleDeg, 0, 'f', 2);
            updateMouseWidget(prompt, tr("Cancel"));
            break;
        }
        case SetReferencePoint1:
            updateMouseWidget(
                tr("Specify the reference angle <0°> (click reference point or type angle):"),
                tr("Cancel"));
            break;
        case SetReferencePoint2:
            updateMouseWidget(
                tr("Specify second reference point:"),
                tr("Cancel"));
            break;
        default:
            updateMouseWidget();
            break;
    }
}

void QC_ActionGetRotate::getResult(QList<qulonglong> *entityIds,
                                   QPointF *basePoint,
                                   double *angleRad,
                                   bool *copyMode) const
{
    if (entityIds) {
        entityIds->clear();
        for (RS_Entity *en : m_selectedEntities) {
            if (en) {
                entityIds->append(en->getId());
            }
        }
    }
    if (basePoint) {
        basePoint->setX(m_basePoint.x);
        basePoint->setY(m_basePoint.y);
    }
    if (angleRad) {
        *angleRad = m_rotationAngle;
    }
    if (copyMode) {
        *copyMode = m_copyMode;
    }
}
