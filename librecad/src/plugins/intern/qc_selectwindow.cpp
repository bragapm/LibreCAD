// #include "qc_selectwindow.h"

// #include <QAction>
// #include <QMouseEvent>
// #include "rs_dialogfactory.h"
// #include "rs_graphicview.h"
// #include "rs_selection.h"
// #include "rs_overlaybox.h"
// #include "rs_preview.h"
// #include "rs_debug.h"


// struct QC_SelectWindow::Box {
//     RS_Vector v1;
//     RS_Vector v2;
// };


// QC_SelectWindow::QC_SelectWindow(LC_ActionContext *actionContext)
//     : RS_PreviewActionInterface("Select Window", actionContext)
//     , m_actionContext(actionContext)
//     , m_box(std::make_unique<Box>())
//     , m_message(std::make_unique<QString>())
//     , m_overlayBoxOptions(std::make_unique<LC_OverlayBoxOptions>())
// {
//     m_actionType=RS2::ActionSelectWindow;
//     m_overlayBoxOptions->loadSettings();
// }

// QC_SelectWindow::~QC_SelectWindow() = default;

// void QC_SelectWindow::init(int status) {
//     RS_PreviewActionInterface::init(status);
//     //m_graphicView->setCurrentAction(std::make_shared<RS_ActionSelectSingle>(m_entityTypeToSelect,  m_actionContext, this));
// }

// void QC_SelectWindow::trigger() {
//     RS_PreviewActionInterface::trigger();

//     if (m_box->v1.valid && m_box->v2.valid) {
//         if (m_graphic->toGuiDX(m_box->v1.distanceTo(m_box->v2))>10) {

//             bool cross = (m_box->v1.x>m_box->v2.x);

//             RS_Selection s(*getContainer(), m_graphic);
//             s.selectWindow(RS2::EntityType::EntityUnknown, m_box->v1, m_box->v2, m_select, cross);

//             updateSelectionWidget(getContainer()->countSelected(),getContainer()->totalSelectedLength());
//             init();
//             m_completed = true;
//         }
//     }
// }

// void QC_SelectWindow::mouseMoveEvent(QMouseEvent* e) {
//     snapFree(e);
//     drawSnapper();
//     if (getStatus()==SetCorner2 && m_box->v1.valid) {
//         m_box->v2 = snapFree(e);
//         deletePreview();
//         //RS_OverlayBox(const RS_Vector &corner1, const RS_Vector &corner2, LC_OverlayBoxOptions *options);
//         LC_OverlayBoxOptions options;
//         options.loadSettings();
//         RS_OverlayBox* ob = new RS_OverlayBox(m_box->v1, m_box->v2, m_overlayBoxOptions.get());
//         addOverlay(ob, RS2::OverlayGraphics::OverlayEffects);
//         drawPreview();
//     }
// }

// void QC_SelectWindow::mousePressEvent(QMouseEvent* e) {
//     if (e->button()==Qt::LeftButton) {
//         switch (getStatus()) {
//         case SetCorner1:
//             m_box->v1 = snapFree(e);
//             setStatus(SetCorner2);
//             break;

//         default:
//             break;
//         }
//     }

//     RS_DEBUG->print("RS_ActionSelectWindow::mousePressEvent(): %f %f",
//                     m_box->v1.x, m_box->v1.y);
// }

// void QC_SelectWindow::mouseReleaseEvent(QMouseEvent* e) {
//     RS_DEBUG->print("RS_ActionSelectWindow::mouseReleaseEvent()");

//     if (e->button()==Qt::LeftButton) {
//         if (getStatus()==SetCorner2) {
//             m_box->v2 = snapFree(e);
//             trigger();
//         }
//     } else if (e->button()==Qt::RightButton) {
//         if (getStatus()==SetCorner2) {
//             deletePreview();
//         }
//         init(getStatus()-1);
//     }
// }

// void QC_SelectWindow::updateMouseButtonHints() {
//     switch (getStatus()) {
//     case SetCorner1:
//         updateMouseWidget(tr("Click and drag for the selection window"), tr("Cancel"));
//         break;
//     case SetCorner2:
//         updateMouseWidget(tr("Choose second edge"), tr("Back"));
//         break;
//     default:
//         updateMouseWidget();
//         break;
//     }
// }

// // void QC_SelectWindow::updateMouseCursor() {
// //     m_graphicView->setMouseCursor(RS2::SelectCursor);
// // }



// void QC_SelectWindow::setMessage(QString msg){
//     *m_message = std::move(msg);
// }
