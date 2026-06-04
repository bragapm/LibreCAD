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
    RS_Block* getPaperSpace() const { return m_paperSpace; }

    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

    LC_Viewport* getActiveViewport() const { return m_activeViewport; }

protected:
    void createViewRenderer() override;

private:
    LC_ActionContext* m_layoutActionContext;
    RS_Document* m_document;
    LC_Viewport* m_activeViewport {nullptr};
    RS_Block* m_paperSpace {nullptr};
    
    // Panning state for active viewport
    bool m_isViewportPanning {false};
    QPoint m_lastPanPos;
};

#endif // LC_LAYOUTVIEW_H
