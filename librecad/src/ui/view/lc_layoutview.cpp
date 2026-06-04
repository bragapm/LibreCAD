#include "lc_layoutview.h"
#include "lc_layoutviewrenderer.h"
#include "rs_graphic.h"
#include "rs_blocklist.h"
#include "rs_block.h"
#include "lc_viewport.h"
#include "lc_actiondrawviewport.h"
#include "lc_actioncontext.h"
#include "lc_graphicviewport.h"
#include <QMenu>
#include <QAction>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include "rs_painter.h"
#include "rs_actionselectsingle.h"
#include "lc_graphicviewport.h"
#include "rs_graphic.h"
#include "rs_block.h"
#include "rs_blocklist.h"

class ScopedViewportTransform {
public:
    ScopedViewportTransform(LC_LayoutView* view, LC_Viewport* activeVp) : m_viewport(view->getViewPort()) {
        if (activeVp && m_viewport) {
            oldFactor = m_viewport->getFactor();
            oldOffsetX = m_viewport->getOffsetX();
            oldOffsetY = m_viewport->getOffsetY();

            double vpScale = activeVp->getData().modelScale;
            RS_Vector modelCenter = activeVp->getData().modelCenter;
            RS_Vector c1 = activeVp->getCorner1();
            RS_Vector c2 = activeVp->getCorner2();
            RS_Vector vpCenter = (c1 + c2) * 0.5;

            double effFactor = vpScale * oldFactor.x;
            int effOffsetX = std::round((vpCenter.x - vpScale * modelCenter.x) * oldFactor.x + oldOffsetX);
            int effOffsetY = std::round((vpCenter.y - vpScale * modelCenter.y) * oldFactor.y + oldOffsetY);

            m_viewport->justSetOffsetAndFactor(effOffsetX, effOffsetY, effFactor);
            applied = true;
        }
    }
    ~ScopedViewportTransform() {
        if (applied) {
            m_viewport->justSetOffsetAndFactor(oldOffsetX, oldOffsetY, oldFactor.x);
        }
    }
private:
    LC_GraphicViewport* m_viewport;
    bool applied = false;
    RS_Vector oldFactor;
    int oldOffsetX, oldOffsetY;
};

LC_LayoutView::LC_LayoutView(QWidget* parent, RS_Document* doc, LC_ActionContext* actionContext)
    : QG_GraphicView(parent, doc, actionContext)
    , m_layoutActionContext(actionContext)
    , m_document(doc) {
    RS_Graphic* graphic = dynamic_cast<RS_Graphic*>(doc);
    if (graphic) {
        RS_BlockList* blockList = graphic->getBlockList();
        RS_Block* paperSpace = nullptr;
        for(auto block : *blockList) {
            if(block && block->getName() == "*Paper_Space") {
                paperSpace = block;
                break;
            }
        }
        if (!paperSpace) {
            paperSpace = new RS_Block(graphic, RS_BlockData("*Paper_Space", RS_Vector(0,0), false));
            graphic->addBlock(paperSpace);
        }
        m_paperSpace = paperSpace;
        setContainer(m_paperSpace);
        m_layoutActionContext->setEntityContainer(m_paperSpace);
    }
}

LC_LayoutView::~LC_LayoutView() = default;

void LC_LayoutView::initView() {
    // Call parent initView first
    QG_GraphicView::initView();

    // Set up the right-click context menu for layout paper space
    auto* rightClickMenu = new QMenu(this);
    auto* addViewportAction = new QAction(tr("Add Viewport"), this);

    connect(addViewportAction, &QAction::triggered, this, [this]() {
        // Start the viewport drawing action
        auto action = std::make_shared<LC_ActionDrawViewport>(m_layoutActionContext);
        setCurrentAction(action);
    });

    rightClickMenu->addAction(addViewportAction);
    setMenu("Right-Click", rightClickMenu);
}

void LC_LayoutView::createViewRenderer() {
    setRenderer(std::make_unique<LC_LayoutViewRenderer>(getViewPort(), this, this));
}

void LC_LayoutView::setDrawingMode(RS2::DrawingMode m) const {
    auto layoutRenderer = dynamic_cast<LC_LayoutViewRenderer *>(getRenderer());
    if (layoutRenderer != nullptr) {
        layoutRenderer->setDrawingMode(m);
    }
}

RS2::DrawingMode LC_LayoutView::getDrawingMode() const {
    auto layoutRenderer = dynamic_cast<LC_LayoutViewRenderer *>(getRenderer());
    if (layoutRenderer != nullptr) {
        return layoutRenderer->getDrawingMode();
    }
    return RS2::DrawingMode::ModeFull;
}

void LC_LayoutView::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        // Convert GUI pixel to paper space coordinates
        RS_Vector mousePaper = getViewPort()->toUCSFromGui(e->position().x(), e->position().y());
        
        RS_Block* paperSpace = dynamic_cast<RS_Block*>(getContainer());
        if (paperSpace) {
            LC_Viewport* clickedViewport = nullptr;
            for (auto* entity : *paperSpace) {
                if (entity && entity->rtti() == RS2::EntityOverlayBox) { 
                    LC_Viewport* vp = dynamic_cast<LC_Viewport*>(entity);
                    if (vp) {
                        RS_Vector c1 = vp->getCorner1();
                        RS_Vector c2 = vp->getCorner2();
                        double minX = std::min(c1.x, c2.x);
                        double maxX = std::max(c1.x, c2.x);
                        double minY = std::min(c1.y, c2.y);
                        double maxY = std::max(c1.y, c2.y);
                        
                        if (mousePaper.x >= minX && mousePaper.x <= maxX &&
                            mousePaper.y >= minY && mousePaper.y <= maxY) {
                            clickedViewport = vp;
                            break;
                        }
                    }
                }
            }
            
            if (m_activeViewport && m_activeViewport != clickedViewport) {
                m_activeViewport->setActive(false);
            }
            
            m_activeViewport = clickedViewport;
            
            if (m_activeViewport) {
                m_activeViewport->setActive(true);
                m_layoutActionContext->setEntityContainer(m_document);
            } else {
                m_layoutActionContext->setEntityContainer(m_paperSpace);
            }
            
            redraw();
            e->accept();
            return;
        }
    }
    
    QG_GraphicView::mouseDoubleClickEvent(e);
}

void LC_LayoutView::wheelEvent(QWheelEvent* e) {
    if (m_activeViewport) {
        if (e->modifiers() == Qt::NoModifier) {
            double factor = e->angleDelta().y() > 0 ? 1.25 : 0.8;
            double currentScale = m_activeViewport->getModelScale();
            
            // Get mouse position in Model Space
            RS_Vector mouseModelPos;
            {
                ScopedViewportTransform svt(this, m_activeViewport);
                mouseModelPos = getViewPort()->toUCSFromGui(e->position().x(), e->position().y());
            }

            // Calculate new model center to zoom centered on mouse
            RS_Vector currentModelCenter = m_activeViewport->getModelCenter();
            RS_Vector newModelCenter = mouseModelPos - (mouseModelPos - currentModelCenter) / factor;

            m_activeViewport->setModelScale(currentScale * factor);
            m_activeViewport->setModelCenter(newModelCenter);
            m_activeViewport->setCustomView(true);
            redraw();
            e->accept();
            return;
        }
        // Fall through to parent for Ctrl/Shift+wheel on viewport
    }
    
    // Handle Ctrl+wheel / Shift+wheel panning without scrollbars
    if (e->modifiers() == Qt::ControlModifier || e->modifiers() == Qt::ShiftModifier) {
        QPoint numPixels = e->angleDelta() / 4;
        int hDelta = 0, vDelta = 0;
        if (e->modifiers() == Qt::ControlModifier) {
            vDelta = numPixels.y();
        } else {
            hDelta = numPixels.x() != 0 ? numPixels.x() : numPixels.y();
        }
        getViewPort()->zoomPan(hDelta, vDelta);
        redraw();
        e->accept();
        return;
    }
    
    QG_GraphicView::wheelEvent(e);
}

void LC_LayoutView::mousePressEvent(QMouseEvent* e) {
    if (m_activeViewport) {
        if (e->button() == Qt::MiddleButton) {
            m_isViewportPanning = true;
            m_lastPanPos = e->position().toPoint();
            e->accept();
            return;
        } else if (e->button() == Qt::LeftButton) {
            // Check if left click is outside the viewport
            RS_Vector mousePaper = getViewPort()->toUCSFromGui(e->position().x(), e->position().y());
            RS_Vector c1 = m_activeViewport->getCorner1();
            RS_Vector c2 = m_activeViewport->getCorner2();
            double minX = std::min(c1.x, c2.x);
            double maxX = std::max(c1.x, c2.x);
            double minY = std::min(c1.y, c2.y);
            double maxY = std::max(c1.y, c2.y);
            
            if (mousePaper.x < minX || mousePaper.x > maxX ||
                mousePaper.y < minY || mousePaper.y > maxY) {
                // Clicked outside, deactivate
                m_activeViewport->setActive(false);
                m_activeViewport = nullptr;
                m_layoutActionContext->setEntityContainer(m_paperSpace);
                redraw();
                // We let the event pass through in case it needs to trigger standard selection etc.
            }
        }
        
        ScopedViewportTransform svt(this, m_activeViewport);
        QG_GraphicView::mousePressEvent(e);
        return;
    }
    QG_GraphicView::mousePressEvent(e);
}

void LC_LayoutView::mouseMoveEvent(QMouseEvent* e) {
    if (m_activeViewport && m_isViewportPanning) {
        QPoint currentPos = e->position().toPoint();
        int dx = currentPos.x() - m_lastPanPos.x();
        int dy = currentPos.y() - m_lastPanPos.y();
        m_lastPanPos = currentPos;
        
        // Convert screen delta to model delta
        // Distance in paper space = delta / paper_scale
        // Distance in model space = paper_distance / viewport_scale
        
        double paperScaleX = getViewPort()->getFactor().x;
        double paperScaleY = getViewPort()->getFactor().y;
        double vpScale = m_activeViewport->getModelScale();
        
        // Y-axis is inverted in GUI coordinates
        double modelDx = dx / (paperScaleX * vpScale);
        double modelDy = -dy / (paperScaleY * vpScale);
        
        RS_Vector center = m_activeViewport->getModelCenter();
        center.x -= modelDx; // Pan moves the camera, so center goes opposite
        center.y -= modelDy;
        
        m_activeViewport->setModelCenter(center);
        m_activeViewport->setCustomView(true);
        
        redraw();
        e->accept();
        return;
    }
    
    ScopedViewportTransform svt(this, m_activeViewport);
    QG_GraphicView::mouseMoveEvent(e);
}

void LC_LayoutView::mouseReleaseEvent(QMouseEvent* e) {
    if (m_activeViewport && e->button() == Qt::MiddleButton && m_isViewportPanning) {
        m_isViewportPanning = false;
        e->accept();
        return;
    }
    
    ScopedViewportTransform svt(this, m_activeViewport);
    QG_GraphicView::mouseReleaseEvent(e);
}

void LC_LayoutView::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape && m_activeViewport) {
        m_activeViewport->setActive(false);
        m_activeViewport = nullptr;
        m_layoutActionContext->setEntityContainer(m_paperSpace);
        redraw();
        e->accept();
        return;
    }
    QG_GraphicView::keyPressEvent(e);
}
