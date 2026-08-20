/*******************************************************************************
 *
 This file is part of the LibreCAD project, a 2D CAD program
 Copyright (C) 2025 LibreCAD.org
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 ******************************************************************************/

#include "lc_overlayarrow.h"
#include "rs_painter.h"
#include "rs_pen.h"

#include <cmath>
#include <QPainter>

void LC_OverlayArrowOptions::loadSettings(){
    // default
}

LC_OverlayArrow::LC_OverlayArrow(LC_OverlayArrowOptions *options)
    : options(options) {}

void LC_OverlayArrow::update(int viewportWidth, int viewportHeight)
{
    vpWidth = viewportWidth;
    vpHeight = viewportHeight;
}

void LC_OverlayArrow::draw(RS_Painter *painter)
{
    if (!options || !options->m_showArrow) return;

    const int axisLen = options->m_axisLength;
    const int margin  = options->m_margin;
    const int headLen = 8;
    const int boxHalf = 5;

    // Pusat origin (0,0) di pojok kiri bawah viewport (UI space)
    const double cx = margin;
    const double cy = vpHeight - margin;

    painter->save();

    // 1. Setup Font (Bold)
    QFont font("Arial", options->m_fontSize, QFont::Bold);
    painter->setFont(font);

    // 2. Gambar Kotak Origin (Abu-abu terang)
    QPen originQPen(QColor(180, 180, 180), 2, Qt::SolidLine);
    painter->QPainter::setPen(originQPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRectUI(cx - boxHalf, cy - boxHalf, cx + boxHalf, cy + boxHalf);

    // 3. GAMBAR SUMBU X (MERAH)
    QColor redColor(230, 40, 40);
    QPen xQPen(redColor, 2, Qt::SolidLine);
    painter->QPainter::setPen(xQPen);

    // Garis sumbu X (ke kanan)
    double xEnd = cx + axisLen;
    painter->drawLineUISimple(cx + boxHalf, cy, xEnd, cy);

    // Kepala panah sumbu X (segitiga merah terisi)
    QPolygonF xArrowHead;
    xArrowHead << QPointF(xEnd + headLen, cy)
               << QPointF(xEnd, cy - 4)
               << QPointF(xEnd, cy + 4);
    painter->setBrush(QBrush(redColor));
    painter->drawPolygon(xArrowHead);

    // Label teks "X" (Merah)
    QRect xTextRect((int)(xEnd + headLen + 6), (int)(cy - 8), 16, 16);
    painter->drawText(xTextRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, "X", nullptr);

    // 4. GAMBAR SUMBU Y (HIJAU)
    QColor greenColor(40, 210, 40);
    QPen yQPen(greenColor, 2, Qt::SolidLine);
    painter->QPainter::setPen(yQPen);

    // Garis sumbu Y (ke atas)
    double yEnd = cy - axisLen;
    painter->drawLineUISimple(cx, cy - boxHalf, cx, yEnd);

    // Kepala panah sumbu Y (segitiga hijau terisi)
    QPolygonF yArrowHead;
    yArrowHead << QPointF(cx, yEnd - headLen)
               << QPointF(cx - 4, yEnd)
               << QPointF(cx + 4, yEnd);
    painter->setBrush(QBrush(greenColor));
    painter->drawPolygon(yArrowHead);

    // Label teks "Y" (Hijau)
    QRect yTextRect((int)(cx - 8), (int)(yEnd - headLen - 18), 16, 16);
    painter->drawText(yTextRect, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip, "Y", nullptr);

    painter->restore();
}
