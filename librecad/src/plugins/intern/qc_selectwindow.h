#ifndef QC_SELECTWINDOW_H
#define QC_SELECTWINDOW_H

#include "rs_previewactioninterface.h"
#include "document_interface.h"
#include "doc_plugin_interface.h"

class QC_SelectWindow  : public RS_PreviewActionInterface
{
    Q_OBJECT

public:
    /**
     * Action States.
     */
    enum Status {
        SetCorner1,     /**< Setting the 1st corner of the window.  */
        SetCorner2      /**< Setting the 2nd corner of the window. */
    };

public:
    QC_SelectWindow(RS_EntityContainer& container,
                          RS_GraphicView& graphicView,
                          bool select);
    ~QC_SelectWindow() override;

    void init(int status=0) override;

    void trigger() override;

    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

    void updateMouseButtonHints() override;
    void updateMouseCursor() override;
    void setMessage(QString msg);
    bool isCompleted() const{return completed;}


private:
    bool select = false;
    struct Points;
    std::unique_ptr<Points> pPoints;


    bool completed = false;
    std::unique_ptr<QString> message;

};

#endif // QC_SELECTWINDOW_H
