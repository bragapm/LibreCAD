/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software 
** Foundation and appearing in the file gpl-2.0.txt included in the
** packaging of this file.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
** This copyright notice MUST APPEAR in all copies of the script!  
**
**********************************************************************/
#ifndef QG_WIDGETPEN_H
#define QG_WIDGETPEN_H

#include "ui_qg_widgetpen.h"
#include "rs_pen.h"
#include "rs_layer.h"
#include "rs_entity.h"

class QG_WidgetPen : public QWidget, public Ui::QG_WidgetPen
{
    Q_OBJECT

public:
    QG_WidgetPen(QWidget* parent = nullptr, Qt::WindowFlags fl = {});
    ~QG_WidgetPen();

    virtual bool isColorUnchanged();
    virtual bool isLineTypeUnchanged();
    virtual bool isWidthUnchanged();

public slots:
    virtual void setPen( RS_Pen pen, bool showByLayer, bool showUnchanged, const QString & title );
    void setPen(RS_Pen pen, RS_Layer* layer, const QString &title);
    void setPen(RS_Pen pen, RS_Layer* layer, bool showUnchanged, const QString &title);
    void setPen(RS_Entity *entity, RS_Layer *layer, const QString &title);
    virtual RS_Pen getPen();

protected slots:
    virtual void languageChange();
protected:
    bool initialized = false;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
};

#endif // QG_WIDGETPEN_H
