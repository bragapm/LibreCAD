#include "tt_actionfixedzoom.h"

#include "rs_debug.h"
#include <QMouseEvent>
#include "lc_graphicviewport.h"
#include "rs_preview.h"
#include "rs_snapper.h"

struct tt_fixedzoom::ActionData{
    RS_Vector ucsV1;
    RS_Vector v1;
    RS_Vector v2;
};

tt_fixedzoom::tt_fixedzoom(LC_ActionContext *actionContext , bool keepAspectRatio)
    :RS_PreviewActionInterface("Fixed Zoom", actionContext, RS2::ActionFixedZoom)
    , m_actionData(std::make_unique<ActionData>()), m_keepAspectRatio(keepAspectRatio){

}

tt_fixedzoom::~tt_fixedzoom() = default;

void tt_fixedzoom::init(int status){
    RS_DEBUG->print("tt_fixedzoom::init");
    RS_PreviewActionInterface::init(status);
    m_actionData.reset(new ActionData{});

    // Biarkan snapper aktif dengan tidak menghapus atau membersihkannya
    // deleteSnapper();
    // m_snapMode.clear();
    // m_snapMode.restriction = RS2::RestrictNothing;
}

void tt_fixedzoom::doTrigger()
{
    RS_DEBUG->print("tt_actionfixedzoom::trigger()");
    if (m_actionData->v1.valid && m_actionData->v2.valid){
        if (m_viewport->toGuiDX(m_actionData->v1.distanceTo(m_actionData->v2)) > 5){
            RS_Vector point1 = toUCS(m_actionData->v1);
            RS_Vector point2 = toUCS(m_actionData->v2);
            m_viewport->zoomWindow(point1, point2, m_keepAspectRatio);
            init(SetFirstCorner);
        }
    }
}

void tt_fixedzoom::updateMouseButtonHints()
{
    RS_DEBUG->print("tt_actionfixedzoom::updateMouseButtonHints()");

    switch(getStatus()){
        case SetFirstCorner:
            updateMouseWidgetTRCancel(tr("Specify first edge"));
            break;
        case SetSecondCorner:
            updateMouseWidgetTRBack(tr("Specify second edge"));
            break;
        default:
            updateMouseWidget();
            break;
        }
}

void tt_fixedzoom::onMouseRightButtonRelease(int status, [[maybe_unused]] LC_MouseEvent *e)
{
    RS_DEBUG->print("TT_ActionFixedZoom::mouseReleaseEvent");
    if(status == SetSecondCorner){
        deletePreview();
    }
    initPrevious(status);
}

void tt_fixedzoom::onMouseLeftButtonRelease(int status, LC_MouseEvent *e)
{
    RS_DEBUG->print("TT_ActionFixedZoom::mouseReleaseEvent()");
    if(status == SetSecondCorner)
    {
        m_actionData->v2 = e->graphPoint;
        if(fabs(m_actionData->v1.x - m_actionData->v2.x) < RS_TOLERANCE || fabs(m_actionData->v1.y - m_actionData->v2.y) < RS_TOLERANCE)
        {
            deletePreview();
            initPrevious(status);
        }
        trigger();
    }
}

RS2::CursorType tt_fixedzoom::doGetMouseCursor([[maybe_unused]] int status){
    return RS2::CrossCursor;
}

void tt_fixedzoom::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        switch (getStatus())
        {
        case SetFirstCorner:
            m_actionData->v1 = snapPoint(e); // snapFree(e);
            drawSnapper();
            setStatus(SetSecondCorner);
            break;
        default :
            break;
        }
    }

    RS_DEBUG->print("TT_ActionFixedZoom::mousePressEvent(); %f %f", m_actionData->v1.x, m_actionData->v1.y);
}

void tt_fixedzoom::mouseMoveEvent(QMouseEvent *e)
{
    deletePreview();
    snapPoint(e);
    drawSnapper();
    if (getStatus() == SetSecondCorner && m_actionData->v1.valid){
        m_actionData->v2 = snapPoint(e);

        RS_Vector worldCorner1 = m_actionData -> v1;
        RS_Vector worldCorner3 = m_actionData -> v2;

        RS_Vector worldCorner2,worldCorner4;
        calcRectCorners(worldCorner1, worldCorner3, worldCorner2, worldCorner4);
        m_preview -> addRectangle(worldCorner1, worldCorner2, worldCorner3, worldCorner4);

    }

    drawPreview();
}