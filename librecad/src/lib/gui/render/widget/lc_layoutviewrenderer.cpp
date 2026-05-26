#include "lc_layoutviewrenderer.h"

#include "lc_graphicviewport.h"
#include "rs_graphic.h"
#include "rs_math.h"
#include "rs_painter.h"
#include "lc_overlayentitiescontainer.h"

namespace {
    static const RS_Color layoutBackgroundColor = RS_Color(85, 85, 85); // AutoCAD-like dark grey background
    static const RS_Color layoutPaperColor = RS_Color(255, 255, 255);   // White paper
    static const RS_Color layoutBorderColor = RS_Color(0, 0, 0);        // Black border
    static const RS_Color layoutShadowColor = RS_Color(30, 30, 30);     // Dark shadow
}

LC_LayoutViewRenderer::LC_LayoutViewRenderer(LC_GraphicViewport *viewport, QPaintDevice* paintDevice)
   : LC_WidgetViewPortRenderer(viewport, paintDevice) {
}

void LC_LayoutViewRenderer::doRender() {
    if (graphic != nullptr) {
        m_paperScale = graphic->getPaperScale();
    } else {
        m_paperScale = 1.0;
    }
    LC_WidgetViewPortRenderer::doRender();
}

void LC_LayoutViewRenderer::doDrawLayerBackground(RS_Painter *painter) {
    drawPaper(painter);
}

void LC_LayoutViewRenderer::doDrawLayerOverlays(RS_Painter *painter) {
    LC_OverlaysManager *overlaysManager = viewport->getOverlaysManager();
    if (!overlaysManager) {
        return;
    }

    RS_Pen blackPen(RS_Color(0, 0, 0), RS2::Width00, RS2::SolidLine);

    // Forcefully draw all overlay entities (like rubber bands and snappers) in Black
    auto forceDrawEntitiesBlack = [&](RS2::OverlayGraphics type) {
        RS_EntityContainer* overlayContainer = overlaysManager->entitiesAt(type);
        if (overlayContainer != nullptr) {
            for (auto e : overlayContainer->getEntityList()) {
                painter->setPen(blackPen);
                bool selected = e->isSelected();
                e->setSelected(false);
                e->draw(painter);
                if (selected) {
                    e->setSelected(true);
                }
            }
        }
    };

    forceDrawEntitiesBlack(RS2::OverlayGraphics::OverlayEffects);
    forceDrawEntitiesBlack(RS2::OverlayGraphics::ActionPreviewEntity);
    forceDrawEntitiesBlack(RS2::OverlayGraphics::Snapper);
    forceDrawEntitiesBlack(RS2::OverlayGraphics::InfoCursor);

    // Draw drawables (like cursor info)
    auto drawDrawables = [&](RS2::OverlayGraphics type) {
        LC_OverlayDrawablesContainer* overlayContainer = overlaysManager->drawablesAt(type);
        if (overlayContainer != nullptr) {
            overlayContainer->draw(painter);
        }
    };

    drawDrawables(RS2::OverlayGraphics::OverlayEffects);
    drawDrawables(RS2::OverlayGraphics::ActionPreviewEntity);
    drawDrawables(RS2::OverlayGraphics::Snapper);
    drawDrawables(RS2::OverlayGraphics::InfoCursor);
}

void LC_LayoutViewRenderer::drawPaper(RS_Painter *painter) {
    if (m_paperScale < 1.0e-6) {
        return;
    }

    RS_Vector pinsbase = graphic->getPaperInsertionBase();
    RS_Vector printAreaSize = graphic->getPrintAreaSize();

    double paperFactorX = painter->toGuiDX(1.0) / m_paperScale;
    double paperFactorY = painter->toGuiDX(1.0) / m_paperScale;

    int marginLeft = (int) (graphic->getMarginLeftInUnits() * paperFactorX);
    int marginTop = (int) (graphic->getMarginTopInUnits() * paperFactorY);
    int marginRight = (int) (graphic->getMarginRightInUnits() * paperFactorX);
    int marginBottom = (int) (graphic->getMarginBottomInUnits() * paperFactorY);

    const RS_Vector &wcsLeftBottomCorner = (RS_Vector(0, 0) - pinsbase) / m_paperScale;
    const RS_Vector &wcsTopBottomCorner = (printAreaSize - pinsbase) / m_paperScale;

    double v1x, v1y, v2x, v2y;
    painter->toGui(wcsLeftBottomCorner, v1x, v1y);
    painter->toGui(wcsTopBottomCorner, v2x, v2y);

    int viewWidth = viewport->getWidth();
    int viewHeight = viewport->getHeight();

    int printAreaW = (int) (v2x - v1x);
    int printAreaH = (int) (v2y - v1y);

    int paperX1 = (int) v1x;
    int paperY1 = (int) v1y;
    int paperW = printAreaW + marginLeft + marginRight;
    int paperH = printAreaH - marginTop - marginBottom;

    painter->setPen(QColor(Qt::black));

    // Dark background:
    painter->fillRect(0, 0, viewWidth, viewHeight, layoutBackgroundColor);

    // Shadow:
    painter->fillRect(paperX1 + 4, paperY1 + 4, paperW, paperH, layoutShadowColor);

    // Border:
    painter->fillRect(paperX1, paperY1, paperW, paperH, layoutBorderColor);

    // Paper:
    painter->fillRect(paperX1 + 1, paperY1 - 1, paperW - 2, paperH + 2, layoutPaperColor);

    // Printable area dashed outline could be added here similar to AutoCAD,
    // but for now we mimic the basic paper view.
}

void LC_LayoutViewRenderer::renderEntity(RS_Painter *painter, RS_Entity *e) {
    // BLOCKING: The user requested to block drawing model space entities
    // in the paper space layout until viewports are implemented.
    // return;
    
    if ((e->getFlag(RS2::FlagSelected) != painter->shouldDrawSelected())) {
        return;
    }

    if (!e->isVisible() || e->isConstruction()) {
        return;
    }

    if (!e->isPrint()) {
        return;
    }

    if (isOutsideOfBoundingClipRect(e, false)) {
        return;
    }

    setPenForPrintingEntity(painter, e);
    justDrawEntity(painter, e);
}

void LC_LayoutViewRenderer::setPenForPrintingEntity(RS_Painter *painter, RS_Entity *e) {
    RS_Pen pen = e->getPenResolved();
    RS_Pen originalPen = pen;

    double patternOffset = painter->currentDashOffset();
    if (lastPaintEntityPen.isSameAs(pen, patternOffset)) {
        return;
    }

    double width = pen.getWidth();

    if (pen.getAlpha() == 1.0) {
        if (width > 0) {
            double wf = 1.0;
            if (m_paperScale > RS_TOLERANCE) {
                if (m_scaleLineWidth) {
                    wf = defaultWidthFactor;
                } else {
                    wf = 1.0 / m_paperScale;
                }
            }
            double screenWidth = painter->toGuiDX(width * unitFactor100 * wf);
            if (screenWidth < 1) {
                screenWidth = 0.0;
            }
            pen.setScreenWidth(screenWidth);
        } else {
            pen.setScreenWidth(0.0);
        }
    } else {
        if (RS_Math::round(pen.getScreenWidth()) == 1) {
            pen.setScreenWidth(0.0);
        }
    }

    // Adapt line color against white paper background
    RS_Color penColor = pen.getColor();
    // If the color is white or too close to white, make it black so it's visible on paper.
    if (penColor.isEqualIgnoringFlags(layoutPaperColor) ||
        (penColor.colorDistance(layoutPaperColor) < RS_Color::MinColorDistance)) {
        pen.setColor(RS_Color(0, 0, 0));
    }

    if (pen.getLineType() != RS2::SolidLine) {
        pen.setDashOffset(patternOffset * defaultWidthFactor);
    }

    if (e->getFlag(RS2::FlagTransparent)) {
        pen.setColor(m_colorBackground);
    }

    lastPaintEntityPen.updateBy(originalPen);
    painter->setPen(pen);
}

void LC_LayoutViewRenderer::setupPainter(RS_Painter *painter) {
    LC_WidgetViewPortRenderer::setupPainter(painter);
    painter->setDrawingMode(m_drawingMode);

    painter->setMinCircleDrawingRadius(0);
    painter->setMinArcDrawingRadius(0);
    painter->setMinLineDrawingLen(0);
    painter->setMinEllipseMajorRadius(0);
    painter->setMinEllipseMinorRadius(0);
    painter->setPenCapStyle(Qt::RoundCap);
    painter->setPenJoinStyle(Qt::RoundJoin);
    painter->setMinRenderableTextHeightInPx(0);

    painter->setRenderArcsInterpolate(true);
    painter->setRenderArcsInterpolationAngleFixed(true);
    painter->setRenderArcsInterpolationAngleValue(M_PI / 36); 
    painter->disableUCS();
}

void LC_LayoutViewRenderer::setDrawingMode(RS2::DrawingMode mode) {
    m_drawingMode = mode;
    viewport->notifyChanged();
}
