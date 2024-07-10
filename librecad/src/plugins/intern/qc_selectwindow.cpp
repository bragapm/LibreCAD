#include "qc_selectwindow.h"

#include <QAction>
#include <QMouseEvent>
#include "rs_dialogfactory.h"
#include "rs_graphicview.h"
#include "rs_selection.h"
#include "rs_overlaybox.h"
#include "rs_preview.h"
#include "rs_debug.h"


struct QC_SelectWindow::Points {
    RS_Vector v1;
    RS_Vector v2;
};


QC_SelectWindow::QC_SelectWindow(
    RS_EntityContainer& container,
    RS_GraphicView& graphicView,
    bool select)
    : RS_PreviewActionInterface("Select Window", container, graphicView)
    , select(select)
    , pPoints(std::make_unique<Points>())
{
    actionType=RS2::ActionSelectWindow;
}

QC_SelectWindow::~QC_SelectWindow() = default;

void QC_SelectWindow::init(int status) {
    RS_PreviewActionInterface::init(status);
    pPoints = std::make_unique<Points>();
    //snapMode.clear();
    //snapMode.restriction = RS2::RestrictNothing;
}

void QC_SelectWindow::trigger() {
    RS_PreviewActionInterface::trigger();

    if (pPoints->v1.valid && pPoints->v2.valid) {
        if (graphicView->toGuiDX(pPoints->v1.distanceTo(pPoints->v2))>10) {

            bool cross = (pPoints->v1.x>pPoints->v2.x);

            RS_Selection s(*container, graphicView);
            s.selectWindow(pPoints->v1, pPoints->v2, select, cross);

            RS_DIALOGFACTORY->updateSelectionWidget(container->countSelected(),container->totalSelectedLength());
            init();
            completed = true;
        }
    }
}

void QC_SelectWindow::mouseMoveEvent(QMouseEvent* e) {
    snapFree(e);
    drawSnapper();
    if (getStatus()==SetCorner2 && pPoints->v1.valid) {
        pPoints->v2 = snapFree(e);
        deletePreview();
        RS_OverlayBox* ob=new RS_OverlayBox(preview.get(), RS_OverlayBoxData(pPoints->v1, pPoints->v2));
        preview->addEntity(ob);

        //RLZ: not needed overlay have contour
        /*                RS_Pen pen(RS_Color(218,105,24), RS2::Width00, RS2::SolidLine);

                // TODO change to a rs_box sort of entity
                RS_Line* e=new RS_Line(preview, RS_LineData(RS_Vector(v1->x, v1->y),  RS_Vector(v2->x, v1->y)));
                e->setPen(pen);
        preview->addEntity(e);

                e=new RS_Line(preview, RS_LineData(RS_Vector(v2->x, v1->y),  RS_Vector(v2->x, v2->y)));
                e->setPen(pen);
        preview->addEntity(e);

                e=new RS_Line(preview, RS_LineData(RS_Vector(v2->x, v2->y),  RS_Vector(v1->x, v2->y)));
                e->setPen(pen);
        preview->addEntity(e);

                e=new RS_Line(preview, RS_LineData(RS_Vector(v1->x, v2->y),  RS_Vector(v1->x, v1->y)));
                e->setPen(pen);
        preview->addEntity(e);*/

        drawPreview();
    }
}

void QC_SelectWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton) {
        switch (getStatus()) {
        case SetCorner1:
            pPoints->v1 = snapFree(e);
            setStatus(SetCorner2);
            break;

        default:
            break;
        }
    }

    RS_DEBUG->print("RS_ActionSelectWindow::mousePressEvent(): %f %f",
                    pPoints->v1.x, pPoints->v1.y);
}

void QC_SelectWindow::mouseReleaseEvent(QMouseEvent* e) {
    RS_DEBUG->print("RS_ActionSelectWindow::mouseReleaseEvent()");

    if (e->button()==Qt::LeftButton) {
        if (getStatus()==SetCorner2) {
            pPoints->v2 = snapFree(e);
            trigger();
        }
    } else if (e->button()==Qt::RightButton) {
        if (getStatus()==SetCorner2) {
            deletePreview();
        }
        init(getStatus()-1);
    }
}

void QC_SelectWindow::updateMouseButtonHints() {
    switch (getStatus()) {
    case SetCorner1:
        RS_DIALOGFACTORY->updateMouseWidget(tr("Click and drag for the selection window"), tr("Cancel"));
        break;
    case SetCorner2:
        RS_DIALOGFACTORY->updateMouseWidget(tr("Choose second edge"), tr("Back"));
        break;
    default:
        RS_DIALOGFACTORY->updateMouseWidget();
        break;
    }
}

void QC_SelectWindow::updateMouseCursor() {
    graphicView->setMouseCursor(RS2::SelectCursor);
}



void QC_SelectWindow::setMessage(QString msg){
    *message = std::move(msg);
}
