// #ifndef QC_SELECTWINDOW_H
// #define QC_SELECTWINDOW_H

// #include "rs_previewactioninterface.h"
// struct LC_OverlayBoxOptions;

// class QC_SelectWindow  : public RS_PreviewActionInterface
// {
//     Q_OBJECT

// public:
//     /**
//      * Action States.
//      */
//     enum Status {
//         SetCorner1,     /**< Setting the 1st corner of the window.  */
//         SetCorner2      /**< Setting the 2nd corner of the window. */
//     };

// public:
//     QC_SelectWindow(LC_ActionContext *actionContext);
//     ~QC_SelectWindow() override;

//     void init(int status=0) override;
//     void trigger() override;
//     void mouseMoveEvent(QMouseEvent* e) override;
//     void mousePressEvent(QMouseEvent* e) override;
//     void mouseReleaseEvent(QMouseEvent* e) override;
//     void updateMouseButtonHints() override;

//     void setMessage(QString msg);
//     bool isCompleted() const{return m_completed;}


// private:
//     struct Box;

//     bool m_select = false;
//     bool m_completed = false;

//     LC_ActionContext* m_actionContext;
//     std::unique_ptr<Box> m_box;
//     std::unique_ptr<QString> m_message;
//     std::unique_ptr<LC_OverlayBoxOptions> m_overlayBoxOptions;
// };

// #endif // QC_SELECTWINDOW_H
