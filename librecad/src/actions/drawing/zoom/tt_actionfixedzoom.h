#ifndef TT_ACTIONFIXEDZOOM_H
#define TT_ACTIONFIXEDZOOM_H

#include "rs_previewactioninterface.h"

class tt_fixedzoom : public RS_PreviewActionInterface{
    Q_OBJECT

public:
    tt_fixedzoom(LC_ActionContext *actionContext, bool keepAspectRatio=true);
    ~tt_fixedzoom() override;
    void init (int status) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

protected:
    enum Status {
        SetFirstCorner,
        SetSecondCorner
    };

    struct ActionData;
    std::unique_ptr<ActionData> m_actionData;
    RS2::CursorType doGetMouseCursor(int status) override;
    bool m_keepAspectRatio = false;
    void onMouseLeftButtonRelease(int status, LC_MouseEvent *e) override;
    void onMouseRightButtonRelease(int status, LC_MouseEvent *e) override;
    void updateMouseButtonHints() override;
    void doTrigger() override;
};
#endif // TT_ACTIONFIXEDZOOM_H
