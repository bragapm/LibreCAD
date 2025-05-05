/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2011 Rallaz (rallazz@gmail.com)
**
**
** This file is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
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
#ifndef DOC_PLUGIN_INTERFACE_H
#define DOC_PLUGIN_INTERFACE_H

#include <QObject>

#include <optional>

#include "document_interface.h"
#include "rs_graphic.h"

class Doc_plugin_interface;

class convLTW
{
public:
    convLTW();
    QString lt2str(enum RS2::LineType lt);
    QString lw2str(enum RS2::LineWidth lw);
    QString intColor2str(int col);
    enum RS2::LineType str2lt(QString s);
    enum RS2::LineWidth str2lw(QString w);
private:
    QHash<RS2::LineType, QString> lType;
    QHash<RS2::LineWidth, QString> lWidth;
};

class Plugin_Entity
{
public:
    Plugin_Entity(RS_Entity* ent, Doc_plugin_interface* d);
    Plugin_Entity(RS_EntityContainer* parent, enum DPI::ETYPE type);
    virtual ~Plugin_Entity();
    bool isValid(){if (entity) return true; else return false;}
    RS_Entity* getEnt() {return entity;}
    virtual void getData(QHash<int, QVariant> *data);
    virtual void updateData(QHash<int, QVariant> *data);
    virtual void getPolylineData(QList<Plug_VertexData> *data);
    virtual void updatePolylineData(QList<Plug_VertexData> *data);

    virtual void move(QPointF offset, DPI::Disposition disp = DPI::DELETE_ORIGINAL);
    virtual void moveRotate(QPointF const& offset, QPointF const& center, double angle, DPI::Disposition disp = DPI::DELETE_ORIGINAL);
    virtual void rotate(QPointF center, double angle, DPI::Disposition disp = DPI::DELETE_ORIGINAL);
    virtual void scale(QPointF center, QPointF factor, DPI::Disposition disp = DPI::DELETE_ORIGINAL);
    virtual QString intColor2str(int color);
    virtual bool isSelected();
private:
    RS_Entity* entity;
    bool hasContainer;
    Doc_plugin_interface* dpi;
};

class Doc_plugin_interface : public Document_Interface
{
public:
    Doc_plugin_interface(RS_Document *d, RS_GraphicView* gv, QWidget* parent);
    void updateView() override;
    void addPoint(QPointF *start) override;
    std::optional<qulonglong> addPointReturn(QPointF *start) override;
    void addLine(QPointF *start, QPointF *end) override;
    std::optional<qulonglong> addLineReturn(QPointF *start, QPointF *end) override;
    void addMText(QString txt, QString sty, QPointF *start,
            double height, double angle, DPI::HAlign ha,  DPI::VAlign va);
    std::optional<qulonglong> addMTextReturn(QString txt, QString sty, QPointF *start,
            double height, double angle, DPI::HAlign ha,  DPI::VAlign va);
    void addText(QString txt, QString sty, QPointF *start,
            double height, double angle, DPI::HAlign ha,  DPI::VAlign va) override;
    std::optional<qulonglong> addTextReturn(QString txt, QString sty, QPointF *start,
            double height, double angle, DPI::HAlign ha,  DPI::VAlign va) override;

    void addCircle(QPointF *start, qreal radius) override;
    void addArc(QPointF *start, qreal radius, qreal a1, qreal a2) override;
    void addEllipse(QPointF *start, QPointF *end, qreal ratio, qreal a1, qreal a2) override;
    virtual void addLines(std::vector<QPointF> const& points, bool closed=false) override;
    virtual std::vector<qulonglong> addLinesReturn(std::vector<QPointF> const& points, bool closed=false) override;
    virtual void addPolyline(std::vector<Plug_VertexData> const& points, bool closed=false) override;
    virtual std::optional<qulonglong> addPolylineReturn(std::vector<Plug_VertexData> const& points, bool closed=false) override;
    virtual void addSplinePoints(std::vector<QPointF> const& points, bool closed=false) override;
    void addImage(int handle, QPointF *start, QPointF *uvr, QPointF *vvr,
                  int w, int h, QString name, int br, int con, int fade) override;
    void addDimAligned(QPointF defPt, QPointF textPt, QString text, QString textStyle, double textAngle,
                       QPointF d1, QPointF d2) override;
    void addDimAngular(QPointF defPt, QPointF textPt, QString text, QString textStyle, double textAngle,
                       QPointF d1, QPointF d2, QPointF d3, QPointF d4) override;
    void addInsert(QString name, QPointF ins, QPointF scale, qreal rot) override;
    QString addBlockfromFromdisk(QString fullName) override;
    void addEntity(Plug_Entity *handle) override;
    Plug_Entity *newEntity( enum DPI::ETYPE type) override;
    void removeEntity(Plug_Entity *ent) override;
    void updateEntity(RS_Entity *org, RS_Entity *newe);

    void setLayer(QString name) override;
    QString getCurrentLayer() override;
    QStringList getAllLayer() override;
    QStringList getAllBlocks() override;
    bool deleteLayer(QString name) override;

    void getCurrentLayerProperties(int *c, DPI::LineWidth *w, DPI::LineType *t) override;
    void getCurrentLayerProperties(int *c, QString *w, QString *t) override;
    void setCurrentLayerProperties(int c, DPI::LineWidth w, DPI::LineType t) override;
    void setCurrentLayerProperties(int c, QString const& w, QString const& t) override;

    bool getPoint(QPointF *point, const QString& message, QPointF *base) override;
    Plug_Entity *getEnt(const QString& message) override;
    bool getSelect(QList<Plug_Entity *> *sel, const QString& message) override;
    bool getAllEntities(QList<Plug_Entity *> *sel, bool visible = false) override;

    bool getVariableInt(const QString& key, int *num) override;
    bool getVariableDouble(const QString& key, double *num) override;
    bool addVariable(const QString& key, int value, int code=70) override;
    bool addVariable(const QString& key, double value, int code=40) override;

    bool getInt(int *num, const QString& message, const QString& title) override;
    bool getReal(qreal *num, const QString& message, const QString& title) override;
    bool getString(QString *txt, const QString& message, const QString& title) override;
    QString realToStr(const qreal num, const int units = 0, const int prec = 0) override;

    QVariantList getExtent() override;

    //method to handle undo in Plugin_Entity
    bool addToUndo(RS_Entity* current, RS_Entity* modified, DPI::Disposition how);

    /*~~[GeoKKP]~~*/
    bool selectEntity(const qulonglong &id) override;
    bool selectByWindow(QList<Plug_Entity *> *sel, const QString& message) override;
    void selectEntities(const QList<qulonglong>* idList) override;
    void deselectEntity(const qulonglong &id) override;
    void deselectEntities(const QList<qulonglong>* idList) override;    
    bool getSelectedEntities(QList<Plug_Entity *> *sel, bool visible = false) override;
    Plug_Entity * getEntity(const qulonglong eid) override;
    QImage getRaster(const QPointF bottomLeft, const QPointF topRight, int imageX, int imageY, int borderX=0, int borderY=0, bool bgWhite = true, bool monochrome = true) override;
    void toggleLayer(QString name) override;
    void lockLayer(QString name) override;
    void printLayer(QString name) override;
    void lockAllLayer() override;
    void unlockAllLayer() override;
    void freezeAllLayer() override;
    void unfreezeAllLayer() override;

private:
    RS_Document *doc;
    RS_Graphic *docGr;
    RS_GraphicView *gView;
    QWidget* main_window;
};

/*void addArc(QPointF *start);			->Without start
void addCircle(QPointF *start);			->Without start
more...
*/
#endif // DOC_PLUGIN_INTERFACE_H
