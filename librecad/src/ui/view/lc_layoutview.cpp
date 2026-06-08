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
#include "rs_debug.h"
#include "rs_actionselectsingle.h"
#include "rs_eventhandler.h"
#include <cmath>

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


// ──────────────────────────────────────────────────────────────────────────
// Activate / deactivate viewport on double-click
// ──────────────────────────────────────────────────────────────────────────

// Forward declaration (defined below)
static bool isInsideViewport(const RS_Vector& pt, LC_Viewport* vp);

void LC_LayoutView::executeWithModelSpaceTransform(QMouseEvent* e, const std::function<void()>& func) {
    if (!m_activeViewport) {
        func();
        return;
    }

    LC_GraphicViewport* vport = getViewPort();
    RS_Vector mousePaper = vport->toUCSFromGui(e->position().x(), e->position().y());

    if (!isInsideViewport(mousePaper, m_activeViewport)) {
        func();
        return;
    }

    // Apply the active viewport projection so snapping and drawing map directly to the model coordinates
    RS_Vector oldFactor = vport->getFactor();
    int oldOffsetX = vport->getOffsetX();
    int oldOffsetY = vport->getOffsetY();

    RS_Vector c1 = m_activeViewport->getCorner1();
    RS_Vector c2 = m_activeViewport->getCorner2();
    RS_Vector vpCenter = (c1 + c2) * 0.5;

    double vpScale = m_activeViewport->getModelScale();
    RS_Vector modelCenter = m_activeViewport->getModelCenter();

    double effFactor = vpScale * oldFactor.x;
    int effOffsetX = std::round((vpCenter.x - vpScale * modelCenter.x) * oldFactor.x + oldOffsetX);
    int effOffsetY = std::round((vpCenter.y - vpScale * modelCenter.y) * oldFactor.y + oldOffsetY);

    vport->justSetOffsetAndFactor(effOffsetX, effOffsetY, effFactor);

    func();

    vport->justSetOffsetAndFactor(oldOffsetX, oldOffsetY, oldFactor.x);
}

void LC_LayoutView::mousePressEvent(QMouseEvent* e) {
    if (m_activeViewport) {
        RS_Vector mousePaper = getViewPort()->toUCSFromGui(e->position().x(), e->position().y());

        if (isInsideViewport(mousePaper, m_activeViewport)) {
            if (e->button() == Qt::MiddleButton) {
                m_isPanning = true;
                m_panLastPos = e->pos();
                e->accept();
                return;
            }
            
            // Forward event to base class but use Model Space coordinate mapping
            executeWithModelSpaceTransform(e, [&]() {
                QG_GraphicView::mousePressEvent(e);
            });
            return;
        } else {
            // Clicked outside active viewport -> deactivate it
            m_activeViewport->setActive(false);
            m_activeViewport = nullptr;
            m_layoutActionContext->setEntityContainer(m_paperSpace);
            redraw();
        }
    }

    QG_GraphicView::mousePressEvent(e);
}

void LC_LayoutView::mouseMoveEvent(QMouseEvent* e) {
    if (m_isPanning && m_activeViewport) {
        QPoint delta = e->pos() - m_panLastPos;
        panActiveViewport(delta.x(), delta.y());
        m_panLastPos = e->pos();
        redraw(RS2::RedrawDrawing);
        e->accept();
        return;
    }
    
    executeWithModelSpaceTransform(e, [&]() {
        QG_GraphicView::mouseMoveEvent(e);
    });
}

void LC_LayoutView::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::MiddleButton && m_isPanning) {
        m_isPanning = false;
        e->accept();
        return;
    }
    
    executeWithModelSpaceTransform(e, [&]() {
        QG_GraphicView::mouseReleaseEvent(e);
    });
}

void LC_LayoutView::mouseDoubleClickEvent(QMouseEvent* e) {
    executeWithModelSpaceTransform(e, [&]() {
        if (!m_activeViewport) {
            if (e->button() == Qt::LeftButton) {
                RS_Vector mousePaper = getViewPort()->toUCSFromGui(e->position().x(), e->position().y());
                RS_Block* paperSpace = dynamic_cast<RS_Block*>(getContainer());
                if (paperSpace) {
                    LC_Viewport* clickedViewport = nullptr;
                    for (auto* entity : *paperSpace) {
                        if (entity && entity->rtti() == RS2::EntityOverlayBox) {
                            LC_Viewport* vp = dynamic_cast<LC_Viewport*>(entity);
                            if (vp && isInsideViewport(mousePaper, vp)) {
                                clickedViewport = vp;
                                break;
                            }
                        }
                    }

                    if (m_activeViewport && m_activeViewport != clickedViewport) {
                        m_activeViewport->setActive(false);
                        m_isPanning = false;
                    }

                    if (clickedViewport) {
                        m_activeViewport = clickedViewport;
                        m_activeViewport->setActive(true);
                        m_layoutActionContext->setEntityContainer(m_document);
                        
                        if (!m_activeViewport->getData().isCustomView) {
                            zoomAuto();
                            m_activeViewport->setCustomView(true);
                        }
                        
                        qDebug() << "[LayoutView] Viewport activated.";
                        redraw(RS2::RedrawDrawing);
                        e->accept();
                        return;
                    } else {
                        m_activeViewport = nullptr;
                        m_layoutActionContext->setEntityContainer(m_paperSpace);
                        redraw();
                    }
                    e->accept();
                    return;
                }
            }
        }
        QG_GraphicView::mouseDoubleClickEvent(e);
    });
}

// ──────────────────────────────────────────────────────────────────────────
// Helpers: check if a paper-space point is inside a viewport
// ──────────────────────────────────────────────────────────────────────────

static bool isInsideViewport(const RS_Vector& pt, LC_Viewport* vp) {
    if (!vp) return false;
    RS_Vector c1 = vp->getCorner1();
    RS_Vector c2 = vp->getCorner2();
    double minX = std::min(c1.x, c2.x), maxX = std::max(c1.x, c2.x);
    double minY = std::min(c1.y, c2.y), maxY = std::max(c1.y, c2.y);
    return (pt.x >= minX && pt.x <= maxX && pt.y >= minY && pt.y <= maxY);
}

// ──────────────────────────────────────────────────────────────────────────
// Pan / zoom helpers that work directly on the LC_Viewport data
// ──────────────────────────────────────────────────────────────────────────

/**
 * Pan the model inside an active viewport by pixel deltas (dx, dy in screen pixels).
 *
 * The model-center shift in model units is:  -dx / effFactor, +dy / effFactor
 * (negative dx because right-drag should move the model left, i.e. view pans right)
 */
void LC_LayoutView::panActiveViewport(int dx, int dy) {
    if (!m_activeViewport) return;

    double paperFactorX = getViewPort()->getFactor().x;
    double paperFactorY = getViewPort()->getFactor().y; // positive, but Y is inverted in toUcsY formula
    double vpScale      = m_activeViewport->getModelScale();
    if (vpScale <= 1e-10) vpScale = 1.0;

    // effFactor in pixels per model unit (same for x and y since square pixels)
    double effFactorX = vpScale * paperFactorX;
    double effFactorY = vpScale * paperFactorY;
    if (effFactorX <= 1e-10 || effFactorY <= 1e-10) return;

    RS_Vector mc = m_activeViewport->getModelCenter();

    // In screen space: dx>0 = mouse moved right
    // In renderer:  effOffsetX = (vpCenter.x - vpScale*modelCenter.x)*paperFactor.x + paperOffsetX
    // When offsetX increases by dx, modelCenter.x decreases by dx/effFactorX
    mc.x -= dx / effFactorX;

    // In screen space: dy>0 = mouse moved down (screen Y increases downward)
    // To move the model down (following the mouse), modelCenter.y must increase.
    mc.y += dy / effFactorY;

    m_activeViewport->setModelCenter(mc);
    m_activeViewport->setCustomView(true);
}

/**
 * Zoom the model inside the active viewport, centred on the given screen pixel.
 * zoomFactor > 1 → zoom in, < 1 → zoom out.
 */
void LC_LayoutView::zoomActiveViewport(double zoomFactor, const QPointF& screenPt) {
    if (!m_activeViewport) return;

    LC_GraphicViewport* vport = getViewPort();
    double paperFactorX = vport->getFactor().x;
    double paperFactorY = vport->getFactor().y;
    double vpScale      = m_activeViewport->getModelScale();
    if (vpScale <= 1e-10) vpScale = 1.0;

    // Paper-space coordinate under the cursor using the same formula as toUCSFromGui
    double paperX = (screenPt.x() - vport->getOffsetX()) / paperFactorX;
    // toUcsY: -(uiY - height + offsetY) / factor.y
    double paperY = -(screenPt.y() - vport->getHeight() + vport->getOffsetY()) / paperFactorY;

    RS_Vector c1 = m_activeViewport->getCorner1();
    RS_Vector c2 = m_activeViewport->getCorner2();
    RS_Vector vpCenter = (c1 + c2) * 0.5;

    // cursor position relative to viewport centre in paper space, converted to model space
    double cursorModelX = m_activeViewport->getModelCenter().x + (paperX - vpCenter.x) / vpScale;
    double cursorModelY = m_activeViewport->getModelCenter().y + (paperY - vpCenter.y) / vpScale;

    double newScale = vpScale * zoomFactor;
    if (newScale < 1e-8) newScale = 1e-8;
    if (newScale > 1e8)  newScale = 1e8;

    // Adjust model center so cursor point stays at same screen position
    double newMcX = cursorModelX - (paperX - vpCenter.x) / newScale;
    double newMcY = cursorModelY - (paperY - vpCenter.y) / newScale;

    m_activeViewport->setModelScale(newScale);
    m_activeViewport->setModelCenter(RS_Vector(newMcX, newMcY));
    m_activeViewport->setCustomView(true);
}

void LC_LayoutView::zoomAuto(bool axis) {
    if (m_activeViewport && m_document) {
        // Calculate the bounding box of the model
        m_document->calculateBorders();
        RS_Vector minPt = m_document->getMin();
        RS_Vector maxPt = m_document->getMax();
        
        if (minPt.valid && maxPt.valid) {
            RS_Vector c1 = m_activeViewport->getCorner1();
            RS_Vector c2 = m_activeViewport->getCorner2();
            
            double vpWidth = std::abs(c2.x - c1.x);
            double vpHeight = std::abs(c2.y - c1.y);
            
            double modWidth = maxPt.x - minPt.x;
            double modHeight = maxPt.y - minPt.y;
            
            // Avoid division by zero if model is empty or a single point
            if (modWidth < 1e-8) modWidth = 100.0;
            if (modHeight < 1e-8) modHeight = 100.0;
            
            double scaleX = vpWidth / modWidth;
            double scaleY = vpHeight / modHeight;
            double newScale = std::min(scaleX, scaleY) * 0.95; // 5% margin
            
            m_activeViewport->setModelScale(newScale);
            m_activeViewport->setModelCenter((minPt + maxPt) * 0.5);
            m_activeViewport->setCustomView(true);
            
            redraw(RS2::RedrawDrawing);
            return;
        }
    }
    
    // Fallback to normal layout auto zoom (Paper Space)
    QG_GraphicView::zoomAuto(axis);
}



void LC_LayoutView::wheelEvent(QWheelEvent* e) {
    if (m_activeViewport) {
        RS_Vector mousePaper = getViewPort()->toUCSFromGui(
            static_cast<int>(e->position().x()),
            static_cast<int>(e->position().y()));

        if (isInsideViewport(mousePaper, m_activeViewport)) {
            double notches = e->angleDelta().y() / 120.0;
            double factor  = std::pow(1.2, notches);
            zoomActiveViewport(factor, e->position());
            redraw(RS2::RedrawDrawing);
            e->accept();
            return;
        }
    }
    QG_GraphicView::wheelEvent(e);
}

void LC_LayoutView::keyPressEvent(QKeyEvent* e) {
    if (m_activeViewport) {
        if (e->key() == Qt::Key_Escape) {
            m_activeViewport->setActive(false);
            m_activeViewport = nullptr;
            m_isPanning = false;
            m_layoutActionContext->setEntityContainer(m_paperSpace);
            redraw(RS2::RedrawAll);
            e->accept();
            return;
        }

        const int PAN_STEP = 20;
        switch (e->key()) {
            case Qt::Key_Left:  panActiveViewport(-PAN_STEP, 0); redraw(RS2::RedrawDrawing); e->accept(); return;
            case Qt::Key_Right: panActiveViewport( PAN_STEP, 0); redraw(RS2::RedrawDrawing); e->accept(); return;
            case Qt::Key_Up:    panActiveViewport(0, -PAN_STEP); redraw(RS2::RedrawDrawing); e->accept(); return;
            case Qt::Key_Down:  panActiveViewport(0,  PAN_STEP); redraw(RS2::RedrawDrawing); e->accept(); return;
            default: break;
        }
        e->accept();
        return;
    }
    QG_GraphicView::keyPressEvent(e);
}

void LC_LayoutView::keyReleaseEvent(QKeyEvent* e) {
    if (m_activeViewport) {
        e->accept();
        return;
    }
    QG_GraphicView::keyReleaseEvent(e);
}

void LC_LayoutView::resizeEvent(QResizeEvent* event) {
    QG_GraphicView::resizeEvent(event);
    redraw();
}


