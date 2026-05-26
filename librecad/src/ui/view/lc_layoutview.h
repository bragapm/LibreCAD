#ifndef LC_LAYOUTVIEW_H
#define LC_LAYOUTVIEW_H

#include "qg_graphicview.h"
#include "rs.h"

class RS_Document;
class LC_Viewport;
class QWheelEvent;
class QMouseEvent;

class LC_LayoutView : public QG_GraphicView {
Q_OBJECT
public:
    LC_LayoutView(QWidget* parent, RS_Document* doc, LC_ActionContext* actionContext);
    ~LC_LayoutView() override;

    void setDrawingMode(RS2::DrawingMode m) const;
    RS2::DrawingMode getDrawingMode() const;
    void initView() override;

    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

protected:
    void createViewRenderer() override;

private:
    LC_ActionContext* m_layoutActionContext;
    LC_Viewport* m_activeViewport {nullptr};
    
    // Panning state for active viewport
    bool m_isViewportPanning {false};
    QPoint m_lastPanPos;
};

#endif // LC_LAYOUTVIEW_H
