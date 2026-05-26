#ifndef LC_LAYOUTVIEWRENDERER_H
#define LC_LAYOUTVIEWRENDERER_H

#include "lc_widgetviewportrenderer.h"

class RS_Painter;

class LC_LayoutViewRenderer : public LC_WidgetViewPortRenderer {
public:
    LC_LayoutViewRenderer(LC_GraphicViewport *viewport, QPaintDevice* paintDevice);
    void renderEntity(RS_Painter *painter, RS_Entity *e) override;

    RS2::DrawingMode getDrawingMode() const {
        return m_drawingMode;
    }

    void setDrawingMode(RS2::DrawingMode mode);
    bool isTextLineNotRenderable([[maybe_unused]]double uiLineHeight) const override { return false; }
protected:
    void drawPaper(RS_Painter *painter);
    void setPenForPrintingEntity(RS_Painter *painter, RS_Entity *e);
    void doDrawLayerBackground(RS_Painter *painter) override;
    void doDrawLayerOverlays(RS_Painter *painter) override;
    void setupPainter(RS_Painter *painter) override;
    void doRender() override;
private:
    RS2::DrawingMode m_drawingMode = RS2::DrawingMode::ModeAuto;
    double m_paperScale = 1.0;
};

#endif // LC_LAYOUTVIEWRENDERER_H
