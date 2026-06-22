#ifndef LC_LAYOUTVIEW_H
#define LC_LAYOUTVIEW_H

#include "qg_graphicview.h"
#include "rs.h"

class RS_Document;
class LC_Viewport;
class QWheelEvent;
class QMouseEvent;
class QKeyEvent;
#include <QPoint>
#include <QKeyEvent>
#include <functional>
#include "rs_painter.h"
#include "lc_graphicviewportlistener.h"

class LC_LayoutView : public QG_GraphicView, public LC_ViewportZoomDelegate {
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
    void keyReleaseEvent(QKeyEvent* e) override;


    LC_Viewport* getActiveViewport() const { return m_activeViewport; }
    RS_Document* getDocument() const { return m_document; }

protected:
    void createViewRenderer() override;

private:
    LC_ActionContext* m_layoutActionContext;
    RS_Document* m_document;
    LC_Viewport* m_activeViewport {nullptr};
    RS_Block* m_paperSpace {nullptr};

    // Pan state when viewport is active
    bool   m_isPanning  {false};
    QPoint m_panLastPos {};

    void panActiveViewport(int dx, int dy);
    void zoomActiveViewport(double zoomFactor, const QPointF& screenPt);
    void executeWithModelSpaceTransform(QMouseEvent* e, const std::function<void()>& func);

public:
    void zoomAuto(bool axis=true) override;

    // LC_ViewportZoomDelegate overrides
    bool handleZoomIn(double f, const RS_Vector &center) override;
    bool handleZoomOut(double f, const RS_Vector &center) override;
    bool handleZoomPan(int dx, int dy) override;
    bool handleZoomWindow(const RS_Vector& v1, const RS_Vector& v2, bool keepAspectRatio) override;
    bool handleZoomAuto(bool axis, bool keepAspectRatio) override;
    bool handleZoomPrevious() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // LC_LAYOUTVIEW_H
