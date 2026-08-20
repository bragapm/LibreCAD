#ifndef LC_OVERLAYARROW_H
#define LC_OVERLAYARROW_H

#include <QFont>
#include "lc_overlayentity.h"
#include "rs_color.h"

struct LC_OverlayArrowOptions {
    bool        m_showArrow         = true;
    int         m_axisLength        = 40;
    int         m_margin            = 25;
    int         m_fontSize          = 10;
    RS_Color    m_colorX            = QColor(230, 40, 40);
    RS_Color    m_colorY            = QColor(40, 210, 40);
    RS_Color    m_colorOrigin       = QColor(180, 180, 180);
    void        loadSettings();
};

class LC_OverlayArrow : public LC_OverlayDrawable {
public :
    explicit LC_OverlayArrow(LC_OverlayArrowOptions *options);
    ~LC_OverlayArrow() override = default;
    void draw(RS_Painter *painter) override;
    void update(int viewportWidth, int viewportHeight);
private:
    int vpWidth = 400;
    int vpHeight = 300;
    LC_OverlayArrowOptions *options;
};

#endif // LC_OVERLAYARROW_H
