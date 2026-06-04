/*******************************************************************************
 *
 This file is part of the LibreCAD project, a 2D CAD program

 Copyright (C) 2024 LibreCAD.org
 Copyright (C) 2024 sand1024

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

#include<cmath>


#include "rs_actionLayoutView.h"

#include <QMouseEvent>

#include "lc_graphicviewport.h"
#include "LC_LayoutView.h"
#include "qg_layoutoptions.h"
#include "rs_coordinateevent.h"
#include "rs_dialogfactory.h"
#include "rs_dialogfactoryinterface.h"
#include "rs_graphic.h"
#include "rs_graphicview.h"
#include "rs_settings.h"
#include "rs_units.h"
#include "rs_vector.h"

class LC_LayoutView;

struct RS_ActionLayoutView::ActionData {
    RS_Vector v1{};
    RS_Vector v2{};
};

/**
 * Constructor.
 */
RS_ActionLayoutView::RS_ActionLayoutView(LC_ActionContext *actionContext)
    :RS_ActionInterface("Layout View", actionContext, RS2::ActionFileLayoutView)
    , m_actionData(std::make_unique<ActionData>()){

    bool fixed = LC_GET_ONE_BOOL("LayoutView", "PrintScaleFixed");

    if (!fixed) {
        fit();
        updateOptions();
    }
    setPaperScaleFixed(fixed);
}

RS_ActionLayoutView::~RS_ActionLayoutView()=default;

void RS_ActionLayoutView::init(int status) {
    RS_ActionInterface::init(status);
}

void RS_ActionLayoutView::invokeSettingsDialog(){
    if (m_graphic) {
        RS_DIALOGFACTORY->requestOptionsDrawingDialog(*m_graphic);
        updateCoordinateWidgetFormat();
        updateOptionsUI(QG_LayoutOptions::MODE_UPDATE_ORIENTATION);
        zoomToPage();
    }
}

bool RS_ActionLayoutView::isPortrait(){
    bool landscape;
    m_graphic->getPaperFormat(&landscape);
    return !landscape;
}

void RS_ActionLayoutView::setPaperOrientation(bool portrait) {
    bool landscape;
    RS2::PaperFormat format = m_graphic->getPaperFormat(&landscape);
    if (landscape != !portrait) {
        m_graphic->setPaperFormat(format, !portrait);
        zoomToPage();
    }
}

void RS_ActionLayoutView::mouseMoveEvent(QMouseEvent* e) {
    switch (getStatus()) {
        case Moving: {
            m_actionData->v2 = toGraph(e);
            // if Shift is pressed the paper moves only horizontally
            if (isShift(e)) {
                m_actionData->v2.y = m_actionData->v1.y;
            }
            // if Ctrl is pressed the paper moves only vertically
            if (isControl(e)) {
                m_actionData->v2.x = m_actionData->v1.x;
            }
            if (m_graphic) {
                RS_Vector pinsbase = m_graphic->getPaperInsertionBase();
                double scale = m_graphic->getPaperScale();
                m_graphic->setPaperInsertionBase(pinsbase - m_actionData->v2 * scale + m_actionData->v1 * scale);

#ifdef DEBUG_PAPER_INSERTION_BASE
                const RS_Vector &pib = graphic->getPaperInsertionBase();
                LC_ERR << "PIB:" <<  pib.x << " , " << pib.y;
#endif
            }
            m_actionData->v1 = m_actionData->v2;
            m_graphicView->redraw(RS2::RedrawGrid); // DRAW Grid also draws paper, background items
            break;
        }
        default:
            break;
    }
}

void RS_ActionLayoutView::mousePressEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton) {
        switch (getStatus()) {
        case Neutral: {
            m_actionData->v1 = toGraph(e);
            setStatus(Moving);
            break;
        }
        default:
            break;
        }
    }
}

void RS_ActionLayoutView::mouseReleaseEvent(QMouseEvent* e) {
    switch (getStatus()) {
        case Moving:
            setStatus(Neutral);
            break;

        default:
            e->accept();
            break;
    }
}

void RS_ActionLayoutView::onCoordinateEvent( [[maybe_unused]]int status,  [[maybe_unused]]bool isZero, const RS_Vector &pos) {
    RS_Vector pinsbase = m_graphic->getPaperInsertionBase();
    RS_Vector mouse = pos;
    //    qDebug()<<"coordinateEvent= ("<<mouse.x<<", "<<mouse.y<<")";

    if(m_bPaperOffset) {
        commandMessage(tr("Printout offset in paper coordinates by (%1, %2)").arg(mouse.x).arg(mouse.y));
        mouse *= m_graphic->getPaperScale();
    }else
        commandMessage(tr("Printout offset in graph coordinates by (%1, %2)").arg(mouse.x).arg(mouse.y));

    //    RS_DIALOGFACTORY->commandMessage(tr("old insertion base (%1, %2)").arg(pinsbase.x).arg(pinsbase.y));
    //    RS_DIALOGFACTORY->commandMessage(tr("new insertion base (%1, %2)").arg((pinsbase-mouse).x).arg((pinsbase-mouse).y));

    m_graphic->setPaperInsertionBase(pinsbase-mouse);
    m_graphicView->redraw(RS2::RedrawGrid); // DRAW Grid also draws paper, background items
}

bool RS_ActionLayoutView::doProcessCommand( [[maybe_unused]]int status, const QString &c) {
    bool accept = true;
    //    qDebug()<<"cmd="<<c;
    if (checkCommand("blackwhite", c)) {
        setBlackWhite(true);
        commandMessage(tr("Printout in Black/White"));
        updateOptions();
    } else if (checkCommand("color", c)) {
        setBlackWhite(false);
        commandMessage(tr("Printout in color"));
        updateOptions();
    } else if (checkCommand("graphoffset", c)) {
        m_bPaperOffset=false;
        commandMessage(tr("Printout offset in graph coordinates"));
        updateOptions();
    } else if (checkCommand("paperoffset", c)) {
        m_bPaperOffset=true;
        commandMessage(tr("Printout offset in paper coordinates"));
        updateOptions();
    }
    else{
        //coordinate event
        if (c.contains(',')){
            QString coord = c;
            if(c.startsWith('@')) {
                commandMessage(tr("Printout offset ignores relative zero. Ignoring '@'"));
                coord.remove(0, 1);
            }
            //        qDebug()<<"offset by absolute coordinate: ";

            const int commaPos = coord.indexOf(',');
            bool ok1 = false, ok2 = false;
            double x = RS_Math::eval(coord.left(commaPos), &ok1);
            double y = RS_Math::eval(coord.mid(commaPos+1), &ok2);
            if (ok1 && ok2) {
                RS_CoordinateEvent ce(RS_Vector(x,y));
                coordinateEvent(&ce);
            }
            else{
                accept = false;
            }
        }
        else{
            accept = false;
        }
    }
    return accept;
}

QString RS_ActionLayoutView::getAdditionalHelpMessage() {
    return tr(": select printout offset coordinates")+
           "\n"+tr("type in offset from command line to offset printout");
}

QStringList RS_ActionLayoutView::getAvailableCommands() {
    QStringList cmd;
    cmd +=command("blackwhite");
    cmd +=command("color");
    cmd +=command("graphoffset");
    cmd +=command("paperoffset");
    cmd +=command("help");
    return cmd;
}

void RS_ActionLayoutView::resume() {
    RS_ActionInterface::resume();
}

//printout warning in command widget
void RS_ActionLayoutView::printWarning(const QString& s) {
    commandMessage(s);
}

RS2::CursorType RS_ActionLayoutView::doGetMouseCursor([[maybe_unused]] int status){
    switch (status) {
        case Moving:
            return RS2::ClosedHandCursor;
        default:
            return RS2::OpenHandCursor;
    }
}

void RS_ActionLayoutView::center() {
    if (m_graphic) {
        // In Layout View, we don't shift the paper print offsets.
        // We just zoom the screen to see the whole page.
        m_viewport->zoomPage();
    }
}

void RS_ActionLayoutView::zoomToPage(){
    if (m_graphic) {
        m_viewport->zoomPageEx();
    }
}

void RS_ActionLayoutView::fit() {
    if (m_graphic) {
        // In Layout View, paper is always 1:1. We just zoom to page.
        m_viewport->zoomPage();
        redraw();
    }
}

bool RS_ActionLayoutView::setScale(double newScale, bool autoZoom) {
    if (m_graphic != nullptr) {
        if(std::abs(newScale - m_graphic->getPaperScale()) < RS_TOLERANCE )
            return false;

        auto pinBase = m_graphic->getPaperInsertionBase();
        double oldScale = m_graphic->getPaperScale();

        m_graphic->setPaperScale(newScale);

        // changing scale around the drawing center
        // insertion base = center - 0.5 * size * scale
        // To keep the center position, the difference in insertion base is
        //   0.5 * size * (oldScale - newScale)
        pinBase += m_graphic->getSize()*(oldScale - newScale)*0.5;
        m_graphic->setPaperInsertionBase(pinBase);

        if(autoZoom) {
            zoomPageExWithBorder(100);
        }
        redraw();
        return true;
    }
    return false;
}

void RS_ActionLayoutView::zoomPageExWithBorder(int borderSize) {
    int bBottom = m_viewport->getBorderBottom();
    int bTop = m_viewport->getBorderTop();
    int bLeft = m_viewport->getBorderLeft();
    int bRight = m_viewport->getBorderRight();
    // just a small usability improvement - we set additional borders on zoom to let the user
   // see that there might be drawing elements around paper
    m_viewport->setBorders(borderSize, borderSize, borderSize, borderSize);
    m_viewport->zoomPageEx();
    m_viewport->setBorders(bLeft, bTop, bRight, bBottom);
}

double RS_ActionLayoutView::getScale() const{
    double ret = 1.0;
    if (m_graphic) {
        ret = m_graphic->getPaperScale();
    }
    return ret;
}

bool RS_ActionLayoutView::isLineWidthScaling(){
    return m_graphicView->getLineWidthScaling();
}

void RS_ActionLayoutView::setLineWidthScaling(bool state) {
    m_graphicView->setLineWidthScaling(state);
    redraw();
}

bool RS_ActionLayoutView::isBlackWhite() {
    LC_LayoutView* LayoutView = dynamic_cast<LC_LayoutView *>(m_graphicView);
    if (LayoutView != nullptr) {
        return LayoutView->getDrawingMode() == RS2::ModeBW;
    }
    return false;
}

void RS_ActionLayoutView::setBlackWhite(bool bw) {
    auto* LayoutView = dynamic_cast<LC_LayoutView *>(m_graphicView);
    if (LayoutView != nullptr) {
        if (bw) {
            LayoutView->setDrawingMode(RS2::ModeBW);
        } else {
            LayoutView->setDrawingMode(RS2::ModeFull);
        }
    }
}

RS2::Unit RS_ActionLayoutView::getUnit() {
    if (m_graphic) {
        return m_graphic->getUnit();
    }
    else {
        return RS2::None;
    }
}

/** set paperscale fixed */
void RS_ActionLayoutView::setPaperScaleFixed(bool fixed){
    m_graphic->setPaperScaleFixed(fixed);
}

/** get paperscale fixed */
bool RS_ActionLayoutView::isPaperScaleFixed(){
    return m_graphic->getPaperScaleFixed();
}

/** calculate number of pages needed to contain a drawing */
void RS_ActionLayoutView::calcPagesNum(bool multiplePages) {
    if (m_graphic) {
        if (multiplePages) {
            RS_Vector printArea = m_graphic->getPrintAreaSize(false);
            RS_Vector graphicSize = m_graphic->getSize() * m_graphic->getPaperScale();
            int pX = ceil(graphicSize.x / printArea.x);
            int pY = ceil(graphicSize.y / printArea.y);

            if (pX > 99 || pY > 99) { // fixme - why such limit? Why hardcoded?
                commandMessage(tr("Limit of pages has been exceeded."));
                return;
            }

            m_graphic->setPagesNum(pX, pY);
            m_graphic->centerToPage();
            m_viewport->zoomPage();
        }
        else {
            m_graphic->setPagesNum(1, 1);
        }
        updateOptionsUI(QG_LayoutOptions::MODE_UPDATE_PAGE_NUMBERS);
        updateOptions();
    }
}
// fixme - sand -  review and check why subtle rounding issues occur on some pages values
void RS_ActionLayoutView::setPagesNumHorizontal(int pagesCount) {
    RS_Vector printArea = m_graphic->getPrintAreaSize(false);
    RS_Vector graphicSize = m_graphic->getSize();
    double paperScale = pagesCount * printArea.x / (graphicSize.x + 5);
    int vertPagesCount = ceil(graphicSize.y * paperScale / printArea.y);
    m_graphic->setPagesNum(pagesCount, vertPagesCount);
    m_graphic->setPaperScale(paperScale);

//    zoomPageExWithBorder(100);
    m_graphic->centerToPage();
    m_viewport->zoomPage();
    updateOptionsUI(QG_LayoutOptions::MODE_UPDATE_PAGE_NUMBERS);
    updateOptions();
}

void RS_ActionLayoutView::setPagesNumVertical(int pagesCount) {
    RS_Vector printArea = m_graphic->getPrintAreaSize(false);
    RS_Vector graphicSize = m_graphic->getSize();
    double paperScale = pagesCount * printArea.y / (graphicSize.y + 5);

    int horPagesCount = ceil(graphicSize.x * paperScale / printArea.x);

    double paperScaleHor = horPagesCount * printArea.x / graphicSize.x;

    paperScale = std::min(paperScaleHor, paperScale);

    m_graphic->setPagesNum(horPagesCount, pagesCount);
    m_graphic->setPaperScale(paperScale);

//    zoomPageExWithBorder(100);
    m_graphic->centerToPage();
    m_viewport->zoomPage();
    updateOptionsUI(QG_LayoutOptions::MODE_UPDATE_PAGE_NUMBERS);
    updateOptions();
}

int RS_ActionLayoutView::getPagesNumHorizontal() {
    return m_graphic->getPagesNumHoriz();
}

int RS_ActionLayoutView::getPagesNumVertical() {
    return m_graphic->getPagesNumVert();
}
    void RS_ActionLayoutView::updateMouseButtonHints() {
        updateMouseWidget(tr("Drag with Left Button to Position Paper or with Middle Button to Pan" ), "", MOD_SHIFT_AND_CTRL(tr("Move Horizontally"), tr("Move Vertically")));
    }

LC_ActionOptionsWidget* RS_ActionLayoutView::createOptionsWidget() {
    return new QG_LayoutOptions();
}




