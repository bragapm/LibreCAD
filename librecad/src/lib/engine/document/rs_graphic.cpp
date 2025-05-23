/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
** Copyright (C) 2016 ravas (github.com/r-a-v-a-s)
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

#include <cmath>
#include <iostream>

#include <QObject>
#include <QDir>

#include "dxf_format.h"
#include "lc_defaults.h"
#include "lc_graphicviewport.h"
#include "rs_actioninterface.h"
#include "rs_block.h"
#include "rs_debug.h"
#include "rs_dialogfactory.h"
#include "rs_dialogfactoryinterface.h"
#include "rs_fileio.h"
#include "rs_graphic.h"
#include "rs_graphicview.h"
#include "rs_layer.h"
#include "rs_math.h"
#include "rs_settings.h"
#include "rs_units.h"

namespace {
// default paper size A4: 210x297 mm
const RS_Vector g_paperSizeA4{210., 297.};

// validate coordinates
bool validCoordinate(double x)
{
    return x >= RS_MINDOUBLE && x <= RS_MAXDOUBLE;
}

// validate vector
bool validRange(const RS_Vector& vp)
{
    return vp.valid && validCoordinate(vp.x) && validCoordinate(vp.y);
}

// validate vpMin and vpMax forms a valid bounding box
bool validRange(const RS_Vector& vpMin, const RS_Vector& vpMax)
{
    return validRange(vpMin)
           && validRange(vpMax)
           && vpMin.x < vpMax.x
           && vpMin.y < vpMax.y;
}
}

/**
 * Default constructor.
 */
RS_Graphic::RS_Graphic(RS_EntityContainer* parent)
        : RS_Document(parent)
{
    LC_GROUP_GUARD("Defaults");
    {
        setUnit(RS_Units::stringToUnit(LC_GET_ONE_STR("Defaults", "Unit", "None")));
        addVariable("$SNAPSTYLE", static_cast<int>(LC_GET_INT("IsometricGrid", 0)), 70);
        addVariable("$SNAPISOPAIR", static_cast<int>(LC_GET_INT("IsoGridView", 1)), 70);
        setGridOn(!LC_GET_BOOL("GridOffForNewDrawing", false));

        const QString &defaultAnglesBase = LC_GET_STR("AnglesBaseAngle", "0.0");
        bool anglesCounterClockwise = LC_GET_BOOL("AnglesCounterClockwise", true);

        double angleBaseDegrees = RS_Math::eval(defaultAnglesBase, 0.0);
        double angleBaseRadians = RS_Math::deg2rad(angleBaseDegrees);
        setAnglesCounterClockwise(anglesCounterClockwise);
        setAnglesBase(angleBaseRadians);
    }
    RS2::Unit unit = getUnit();

    if (unit == RS2::Inch) {
        addVariable("$DIMASZ", 0.1, DXF_FORMAT_GC_DimASz);
        addVariable("$DIMEXE", 0.05, DXF_FORMAT_GC_DimEXE);
        addVariable("$DIMEXO", 0.025, DXF_FORMAT_GC_DimExO);
        addVariable("$DIMGAP", 0.025, DXF_FORMAT_GC_DimGap);
        addVariable("$DIMTXT", 0.1, DXF_FORMAT_GC_DimTxt);
    } else {
        addVariable("$DIMASZ",
                    RS_Units::convert(2.5, RS2::Millimeter, unit), DXF_FORMAT_GC_DimASz);
        addVariable("$DIMEXE",
                    RS_Units::convert(1.25, RS2::Millimeter, unit), DXF_FORMAT_GC_DimEXE);
        addVariable("$DIMEXO",
                    RS_Units::convert(0.625, RS2::Millimeter, unit), DXF_FORMAT_GC_DimExO);
        addVariable("$DIMGAP",
                    RS_Units::convert(0.625, RS2::Millimeter, unit), DXF_FORMAT_GC_DimGap);
        addVariable("$DIMTXT",
                    RS_Units::convert(2.5, RS2::Millimeter, unit), DXF_FORMAT_GC_DimTxt);
    }
    addVariable("$DIMTIH", 0, DXF_FORMAT_GC_DimTIH);
    //initialize printer vars bug #3602444
    setPaperScale(getPaperScale());
    setPaperInsertionBase(getPaperInsertionBase());

    //set default values for point style
    addVariable("$PDMODE", LC_DEFAULTS_PDMode, DXF_FORMAT_GC_PDMode);
    addVariable("$PDSIZE", LC_DEFAULTS_PDSize, DXF_FORMAT_GC_PDSize);

    addVariable("$JOINSTYLE", 1, DXF_FORMAT_GC_JoinStyle);
    addVariable("$ENDCAPS", 1, DXF_FORMAT_GC_Endcaps);
    setModified(false);
}

/**
 * Destructor.
 */
RS_Graphic::~RS_Graphic() = default;


/**
 * Counts the entities on the given layer.
 */
unsigned RS_Graphic::countLayerEntities(RS_Layer *layer) const
{
    unsigned c = 0;
    if (layer) {
        for (RS_Entity *t: entities) {
            if (t->getLayer() &&
                t->getLayer()->getName() == layer->getName()) {
                c += t->countDeep();
            }
        }
    }
    return c;
}

/**
 * Removes the given layer and undoes all entities on it.
 */
void RS_Graphic::removeLayer(RS_Layer* layer) {
    if (layer != nullptr) {
        const QString &layerName = layer->getName();
        if (layerName != "0") {
            std::vector<RS_Entity *> toRemove;
//find entities on layer
            for (RS_Entity *e: entities) {
                if (e->getLayer() &&
                    e->getLayer()->getName() == layerName) {
                    toRemove.push_back(e);
                }
            }
// remove all entities on that layer:
            if (!toRemove.empty()) {
                startUndoCycle();
                for (RS_Entity *e: toRemove) {
                    e->setUndoState(true);
                    e->setLayer("0");
                    addUndoable(e);
                }
                endUndoCycle();
            }

            toRemove.clear();
            // remove all entities in blocks that are on that layer:
            for (RS_Block *blk: blockList) {
                if (!blk) continue;
                for (auto e: *blk) {
                    if (e->getLayer() &&
                        e->getLayer()->getName() == layerName) {
                        toRemove.push_back(e);
                    }
                }
            }

            for (RS_Entity *e: toRemove) {
                e->setUndoState(true);
                e->setLayer("0");
            }

            layerList.remove(layer);
        }
    }
}

/**
 * Clears all layers, blocks and entities of this graphic.
 * A default layer (0) is created.
 */
void RS_Graphic::newDoc() {

    RS_DEBUG->print("RS_Graphic::newDoc");

    clear();
    clearLayers();
    clearBlocks();
    addLayer(new RS_Layer("0"));
    //addLayer(new RS_Layer("ByBlock"));
    setModified(false);
}

/*
 * Description:	Create/update the drawing backup file, if necessary.
 * Author(s):		Claude Sylvain
 * Created:			13 July 2011
 * Last modified:
 *
 * Parameters:		const QString &filename:
 * 						Name of the drawing file to backup.
 *
 * Returns:			bool:
 * 						false	: Operation failed.
 * 						true	: Operation successful.
 */
bool RS_Graphic::BackupDrawingFile(const QString &filename) {
    static const char *msg_err =
        "RS_Graphic::BackupDrawingFile: Can't create object!";

    bool ret = false;  /*	Operation failed, by default. */


    /*	- Create backup only if drawing file name exist.
     *	- Remark: Not really necessary to check if the drawing file
     *	  name have been defined.
     *	----------------------------------------------------------- */
    if (filename.length() > 0) {
        /*	Built Backup File Name.
         *	*/
        QString *qs_backup_fn = new QString(filename + '~');

        /*	Create "Drawing File" object.
         *	*/
        QFile *qf_df = new QFile(filename);

        /*	If able to create the objects, process...
         *	----------------------------------------- */
        if ((qs_backup_fn != nullptr) && (qf_df != nullptr)) {
            /*	Create backup file only if drawing file already exist.
             *	------------------------------------------------------ */
            if (qf_df->exists() == true) {
                /*	Create "Drawing File Backup" object.
                 *	*/
                QFile *qf_dfb = new QFile(*qs_backup_fn);

                /*	If able to create the object, process...
                 *	---------------------------------------- */
                if (qf_dfb != nullptr) {
                    /*	If a backup file already exist, remove it!
                     *	------------------------------------------ */
                    if (qf_dfb->exists() == true)
                        qf_dfb->remove();

                    qf_df->copy(*qs_backup_fn); /*	Create backup file. */
                    ret = true;      /*	Operation successful. */
                    delete qf_dfb;
                }
                    /*	Can't create object.
                     *	-------------------- */
                else {
                    RS_DEBUG->print("%s", msg_err);
                }
            }

        }
            /*	Can't create object(s).
             *	----------------------- */
        else {
            RS_DEBUG->print("%s", msg_err);
        }

        delete qs_backup_fn;
        delete qf_df;
    }

    return ret;
}

/*
 *	Description:	Saves this graphic with the current filename and settings.
 *	Author(s):		..., Claude Sylvain
 * Last modified:	13 July 2011
 *	Parameters:
 *
 *	Returns:			bool:
 *							false:	Operation failed.
 *							true:		Operation successful.
 *
 * Notes:			- If this is not an AutoSave, backup the drawing file
 * 					  (if necessary).
 * 					- Drawing is saved only when it has been modified.
 * 					  This prevent lost of backup file when file
 * 					  is saved more than one time without being modified.
 */

bool RS_Graphic::save(bool isAutoSave) {
    bool ret = false;

    RS_DEBUG->print("RS_Graphic::save: Entering...");

    /*	- Save drawing file only if it has been modified.
         *	- Notes: Potentially dangerous in case of an internal
         *	  coding error that make LibreCAD not aware of modification
         *	  when some kind of drawing modification is done.
         *	----------------------------------------------------------- */
    if (isModified()) {
        QString actualName;
        RS2::FormatType actualType;

        actualType = formatType;

        if (isAutoSave) {
            actualName = autosaveFilename;

            if (formatType == RS2::FormatUnknown)
                actualType = RS2::FormatDXFRW;
        } else {
//	- This is not an AutoSave operation.  This is a manual
//	  save operation.  So, ...
//		- Set working file name to the drawing file name.
//		- Backup drawing file (if necessary).
//	------------------------------------------------------
            QFileInfo finfo(filename);
            QDateTime m = finfo.lastModified();
            //bug#3414993
            //modifiedTime should only be used for the same filename
//            DEBUG_HEADER
//            qDebug()<<"currentFileName= "<<currentFileName;
//            qDebug()<<"Checking file: filename= "<<filename;
//            qDebug()<<"Checking file: "<<filename;
//            qDebug()<<"modifiedTime.isValid()="<<modifiedTime.isValid();
//            qDebug()<<"Previous timestamp: "<<modifiedTime;
//            qDebug()<<"Current timestamp: "<<m;
            if (currentFileName == QString(filename)
                && modifiedTime.isValid() && m != modifiedTime) {
                //file modified by others
//            qDebug()<<"detected on disk change";
                RS_DIALOGFACTORY->commandMessage(QObject::tr(
                    "File on disk modified. Please save to another file to avoid data loss! File modified: %1").arg(
                    filename));
                return false;
            }

            actualName = filename;
            if (LC_GET_ONE_BOOL("Defaults","AutoBackupDocument", true)) {
                BackupDrawingFile(filename);
            }
        }

        /*	Save drawing file if able to created associated object.
                 *	------------------------------------------------------- */
        if (!actualName.isEmpty()) {
            RS_DEBUG->print("RS_Graphic::save: File: %s", actualName.toLatin1().data());
            RS_DEBUG->print("RS_Graphic::save: Format: %d", (int) actualType);
            RS_DEBUG->print("RS_Graphic::save: Export...");

            ret = RS_FileIO::instance()->fileExport(*this, actualName, actualType);
            QFileInfo finfo(actualName);
            modifiedTime = finfo.lastModified();
            currentFileName = actualName;
        } else {
            RS_DEBUG->print("RS_Graphic::save: Can't create object!");
            RS_DEBUG->print("RS_Graphic::save: File not saved!");
        }

        /*	Remove AutoSave file after user has successfully saved file.
                 *	------------------------------------------------------------ */
        if (ret && !isAutoSave) {
            /*	Autosave file object.
                         *	*/
            QFile qf_file(autosaveFilename);

/*	Tell that drawing file is no more modified.
    *	------------------------------------------- */
            setModified(false);
            layerList.setModified(false);
            blockList.setModified(false);

/*	- Remove autosave file, if able to create associated object,
    *	  and if autosave file exist.
    *	------------------------------------------------------------ */
            if (qf_file.exists()) {
                RS_DEBUG->print("RS_Graphic::save: Removing old autosave file %s",
                                autosaveFilename.toLatin1().data());
                qf_file.remove();
            }

        }

        RS_DEBUG->print("RS_Graphic::save: Done!");
    } else {
        RS_DEBUG->print("RS_Graphic::save: File not modified, not saved");
        ret = true;
    }

    RS_DEBUG->print("RS_Graphic::save: Exiting...");

    return ret;
}

/*
 *	Description:	- Saves this graphic with the given filename and current
 *						  settings.
 *
 *	Author(s):		..., Claude Sylvain
 *	Created:			?
 *	Last modified:	13 July 2011
 *	Parameters:         QString: name to save
 *                      RS2::FormatType: format to save
 *                      bool:
 *                          false: do not save if not needed
 *                          true: force to save (when called for save as...
 *
 *	Returns:			bool:
 *							false:	Operation failed.
 *							true:		Operation successful.
 *
 * Notes:			Backup the drawing file (if necessary).
 */

bool RS_Graphic::saveAs(const QString &filename, RS2::FormatType type, bool force) {
    RS_DEBUG->print("RS_Graphic::saveAs: Entering...");

// Set to "failed" by default.
    bool ret = false;

// Check/memorize if file name we want to use as new file
// name is the same as the actual file name.
    bool fn_is_same = filename == this->filename;
    auto const filenameSaved = this->filename;
    auto const autosaveFilenameSaved = this->autosaveFilename;
    auto const formatTypeSaved = this->formatType;

    this->filename = filename;
    this->formatType = type;

// QString	const oldAutosaveName = this->autosaveFilename;
    QFileInfo finfo(filename);

// Construct new autosave filename by prepending # to the filename
// part, using the same directory as the destination file.
    this->autosaveFilename = finfo.path() + "/#" + finfo.fileName();

// When drawing is saved using a different name than the actual
// drawing file name, make LibreCAD think that drawing file
// has been modified, to make sure the drawing file saved.
    if (!fn_is_same || force)
        setModified(true);

    ret = save();  //	Save file.

    if (ret) {
// Save was successful, remove old autosave file.
        QFile qf_file(autosaveFilenameSaved);

        if (qf_file.exists()) {
            RS_DEBUG->print("RS_Graphic::saveAs: Removing old autosave file %s",
                            autosaveFilenameSaved.toLatin1().data());
            qf_file.remove();
        }

    } else {
//do not modify filenames:
        this->filename = filenameSaved;
        this->autosaveFilename = autosaveFilenameSaved;
        this->formatType = formatTypeSaved;
    }

    return ret;
}


/**
 * Loads the given file into this graphic.
 */
bool RS_Graphic::loadTemplate(const QString &filename, RS2::FormatType type) {
    RS_DEBUG->print("RS_Graphic::loadTemplate(%s)", filename.toLatin1().data());

    bool ret;

    // Construct new autosave filename by prepending # to the filename part,
    // using system temporary dir.
    this->autosaveFilename = QDir::tempPath () + "/#" + "Unnamed.dxf";

    // clean all:
    newDoc();

    // import template file:
    ret = RS_FileIO::instance()->fileImport(*this, filename, type);

    setModified(false);
    layerList.setModified(false);
    blockList.setModified(false);
    QFileInfo finfo;
    modifiedTime = finfo.lastModified();

    RS_DEBUG->print("RS_Graphic::loadTemplate(%s): OK", filename.toLatin1().data());

    return ret;
}

/**
 * Loads the given file into this graphic.
 */
bool RS_Graphic::open(const QString &filename, RS2::FormatType type) {
    RS_DEBUG->print("RS_Graphic::open(%s)", filename.toLatin1().data());

    bool ret;

    this->filename = filename;
    QFileInfo finfo(filename);
    // Construct new autosave filename by prepending # to the filename
    // part, using the same directory as the destination file.
    this->autosaveFilename = finfo.path() + "/#" + finfo.fileName();

    // clean all:
    newDoc();

    // import file:
    ret = RS_FileIO::instance()->fileImport(*this, filename, type);

    if (ret) {
        RS_GraphicView *gv = getGraphicView(); // fixme - eliminate this dependency!
        if (gv != nullptr) {
            // fixme - sand - review and probably move initialization of UCS - as normal support of VIEWPORT will be available
            // todo - not sure whether this is right place for setting up current wcs.
            // Actually, it seems that it's better to rely on reading viewport (were setting for the offset and zoom are set.
            // however, must probably with proper support of VIEW, they will be reworked too..
            // So let it have here for now so far
            LC_GraphicViewport* viewport = gv->getViewPort();
            viewport->initAfterDocumentOpen();
        }

        setModified(false);
        layerList.setModified(false);
        blockList.setModified(false);
        namedViewsList.setModified(false);
        ucsList.setModified(false);

        modifiedTime = finfo.lastModified();
        currentFileName = QString(filename);

        //cout << *((RS_Graphic*)graphic);
        //calculateBorders();

        RS_DEBUG->print("RS_Graphic::open(%s): OK", filename.toLatin1().data());
    }
    return ret;
}

void RS_Graphic::clearVariables() {
    variableDict.clear();
}

int RS_Graphic::countVariables() const
{
    return variableDict.count();
}

void RS_Graphic::addVariable(const QString& key, const RS_Vector& value, int code) {
    variableDict.add(key, value, code);
}

void RS_Graphic::addVariable(const QString& key, const QString& value, int code) {
    variableDict.add(key, value, code);
}

void RS_Graphic::addVariable(const QString& key, int value, int code) {
    variableDict.add(key, value, code);
}

void RS_Graphic::addVariable(const QString& key, bool value, int code) {
    variableDict.add(key, value, code);
}

void RS_Graphic::addVariable(const QString& key, double value, int code) {
    variableDict.add(key, value, code);
}

void RS_Graphic::removeVariable(const QString& key) {
    variableDict.remove(key);
}

RS_Vector RS_Graphic::getVariableVector(const QString& key, const RS_Vector& def) const {
    return variableDict.getVector(key, def);
}

QString RS_Graphic::getVariableString(const QString& key, const QString& def) const {
    return variableDict.getString(key, def);
}

int RS_Graphic::getVariableInt(const QString& key, int def) const {
    return variableDict.getInt(key, def);
}

bool RS_Graphic::getVariableBool(const QString& key, bool def) const {
    return variableDict.getInt(key, def ? 1 : 0) != 0;
}

double RS_Graphic::getVariableDouble(const QString& key, double def) const {
    return variableDict.getDouble(key, def);
}

QHash<QString, RS_Variable>& RS_Graphic::getVariableDict() {
    return variableDict.getVariableDict();
}

//
// fixme - sand - actually, some additional caching of variables may be used,
// in order to avoid loading/writing them into hashmap on each access...
// need to measure and provile the cost of direct access to variables map first.
// so it might be something like common initialization below..
//  void RS_Graphic::loadVariables(){
//    gridOn = getVariableInt("$GRIDMODE", 1) != 0;
//     etc...
//  }
//

/**
 * @return true if the grid is switched on (visible).
 */
bool RS_Graphic::isGridOn() const {
    int on = getVariableInt("$GRIDMODE", 1);
    return on != 0;
//    return gridOn;
}

/**
 * Enables / disables the grid.
 */
void RS_Graphic::setGridOn(bool on) {
//    gridOn = on;
    addVariable("$GRIDMODE", (int)on, 70);
}

/**
 * @return true if the isometric grid is switched on (visible).
 */
bool RS_Graphic::isIsometricGrid() const{
    //$ISOMETRICGRID == $SNAPSTYLE
    int on = getVariableInt("$SNAPSTYLE", 0);
    return on!=0;
}

/**
 * Enables / disables isometric grid.
 */
void RS_Graphic::setIsometricGrid(bool on) {
    //$ISOMETRICGRID == $SNAPSTYLE
    addVariable("$SNAPSTYLE", (int)on, 70);
}

double RS_Graphic::getAnglesBase() const
{
    double result = getVariableDouble("$ANGBASE",0.0);
    return result;
}

void RS_Graphic::setAnglesBase(double baseAngle){
    addVariable("$ANGBASE", baseAngle, 50);
}

bool RS_Graphic::areAnglesCounterClockWise() const
{
    int angDir = getVariableInt("$ANGDIR", 0);
    return angDir == 0;
}

void RS_Graphic::setAnglesCounterClockwise(bool on){
    addVariable("$ANGDIR", on ? 0: 1, 70);
}

void RS_Graphic::setCurrentUCS(LC_UCS* ucs){
    QString name = ucs->getName();
    if (!ucs->isUCS()){
        name = "";
    }
    addVariable("$UCSNAME", name, 2);
    addVariable("$UCSORG", ucs->getOrigin(), 10);

    // so far we don't support the following variables
    // http://entercad.ru/acad_dxf.en/ws1a9193826455f5ff18cb41610ec0a2e719-7a6f.htm
    /*
    $UCSORGBACK
    $UCSORGBOTTOM
    $UCSORGFRONT
    $UCSORGLEFT
    $UCSORGRIGHT
    $UCSORGTOP
    */

    // as for these variables... well, so far it' snot clear who they are related to $SNAPSTYLE.... should we rely on them for isometric mode?
    /*
      $UCSORTHOREF
    */

    addVariable("$UCSORTHOVIEW", ucs->getOrthoType(), 70);
    addVariable("$UCSXDIR", ucs->getXAxis(), 10);
    addVariable("$UCSYDIR", ucs->getYAxis(), 10);
}

LC_UCS* RS_Graphic::getCurrentUCS() const {
    QString name = getVariableString("$UCSNAME", "");
    RS_Vector origin = getVariableVector("$UCSORG", RS_Vector(0.0, 0.0));
    int orthoType = getVariableInt("$UCSORTHOVIEW", 0);
    RS_Vector xAxis = getVariableVector("$UCSXDIR", RS_Vector(1.0, 0.0));
    RS_Vector yAxis = xAxis;
    yAxis = getVariableVector("$UCSYDIR", yAxis.rotate(M_PI_2));

    LC_UCS* wcs = ucsList.getWCS();

    auto result = new LC_UCS(name);
    result->setOrigin(origin);
    result->setOrthoType(orthoType);
    result->setXAxis(xAxis);
    result->setYAxis(yAxis);

    if (wcs->isSameTo(result)){
        delete result;
        result = new LC_WCS();
    }
    return result;
}

RS2::IsoGridViewType RS_Graphic::getIsoView() const{
    RS2::IsoGridViewType result = (RS2::IsoGridViewType) getVariableInt("$SNAPISOPAIR", RS2::IsoGridViewType::IsoTop);
    return result;
}

void RS_Graphic::setIsoView(RS2::IsoGridViewType viewType){
    addVariable("$SNAPISOPAIR", viewType, 70);
}

/**
 * Sets the unit of this graphic to 'u'
 */
void RS_Graphic::setUnit(RS2::Unit u) {
    // fixme - sand - add caching
    setPaperSize(RS_Units::convert(getPaperSize(), getUnit(), u));
    addVariable("$INSUNITS", (int)u, 70);
}

/**
 * Gets the unit of this graphic
 */
RS2::Unit RS_Graphic::getUnit() const {
    // fixme - sand - add caching
    return static_cast<RS2::Unit>(getVariableInt("$INSUNITS", 0));
}

/**
 * @return The linear format type for this document.
 * This is determined by the variable "$LUNITS".
 */
RS2::LinearFormat RS_Graphic::getLinearFormat() const
{
    // fixme - sand - add caching
    int lunits = getVariableInt("$LUNITS", 2);
    return getLinearFormat(lunits);
}

/**
 * @return The linear format type used by the variable "$LUNITS" & "$DIMLUNIT".
 */
RS2::LinearFormat RS_Graphic::getLinearFormat(int f) const
{
    switch (f) {
        case 1:
            return RS2::Scientific;
        case 2:
            return RS2::Decimal;
        case 3:
            return RS2::Engineering;
        case 4:
            return RS2::Architectural;
        case 5:
            return RS2::Fractional;
        case 6:
            return RS2::ArchitecturalMetric;
        default:
            return RS2::Decimal;
    }
}

/**
 * @return The linear precision for this document.
 * This is determined by the variable "$LUPREC".
 */
int RS_Graphic::getLinearPrecision() const
{
    // fixme - sand - add caching
    return getVariableInt("$LUPREC", 4);
}

/**
 * @return The angle format type for this document.
 * This is determined by the variable "$AUNITS".
 */
RS2::AngleFormat RS_Graphic::getAngleFormat() const
{
    // fixme - sand - add caching
    int aunits = getVariableInt("$AUNITS", 0);

    switch (aunits) {
        case 0:
            return RS2::DegreesDecimal;
        case 1:
            return RS2::DegreesMinutesSeconds;
        case 2:
            return RS2::Gradians;
        case 3:
            return RS2::Radians;
        case 4:
            return RS2::Surveyors;        
        default:
            return RS2::DegreesDecimal;
    }
}

/**
 * @return The linear precision for this document.
 * This is determined by the variable "$LUPREC".
 */
int RS_Graphic::getAnglePrecision() const
{
    // fixme - sand - add caching
    return getVariableInt("$AUPREC", 4);
}

/**
 * @return The insertion point of the drawing into the paper space.
 * This is the distance from the lower left paper edge to the zero
 * point of the drawing. DXF: $PINSBASE.
 */
RS_Vector RS_Graphic::getPaperInsertionBase() {
    return getVariableVector("$PINSBASE", RS_Vector(0.0,0.0));
}

/**
 * Sets the PINSBASE variable.
 */
void RS_Graphic::setPaperInsertionBase(const RS_Vector& p) {
    addVariable("$PINSBASE", p, 10);
}

/**
 * @return Paper size in graphic units.
 */
RS_Vector RS_Graphic::getPaperSize() const
{
    bool okX = false,okY = false;
    double sX = 0., sY = 0.;
    LC_GROUP_GUARD("Print");
    {
        sX = LC_GET_STR("PaperSizeX", "0.0").toDouble(&okX);
        sY = LC_GET_STR("PaperSizeY", "0.0").toDouble(&okY);
    }
    RS_Vector def ;
    if(okX&&okY && sX>RS_TOLERANCE && sY>RS_TOLERANCE) {
        def=RS_Units::convert(RS_Vector(sX,sY),
                              RS2::Millimeter, getUnit());
    }else{
        def= RS_Units::convert(g_paperSizeA4,
                               RS2::Millimeter, getUnit());
    }

    RS_Vector v1 = getVariableVector("$PLIMMIN", RS_Vector(0.0,0.0));
    RS_Vector v2 = getVariableVector("$PLIMMAX", def);

    return v2-v1;
}

/**
 * Sets a new paper size.
 */
void RS_Graphic::setPaperSize(const RS_Vector& s) {
    addVariable("$PLIMMIN", RS_Vector(0.0,0.0), 10);
    addVariable("$PLIMMAX", s, 10);
    //set default paper size
    RS_Vector def = RS_Units::convert(s,
                                     getUnit(), RS2::Millimeter);
    LC_GROUP_GUARD("Print");
    {
        LC_SET("PaperSizeX", def.x);
        LC_SET("PaperSizeY", def.y);
    }
}

/**
 * @return Print Area size in graphic units.
 */
RS_Vector RS_Graphic::getPrintAreaSize(bool total) const {
    RS_Vector printArea = getPaperSize();
    RS2::Unit dest = getUnit();
    printArea.x -= RS_Units::convert(marginLeft + marginRight, RS2::Millimeter, dest);
    printArea.y -= RS_Units::convert(marginTop+marginBottom, RS2::Millimeter, dest);
    if (total) {
        printArea.x *= pagesNumH;
        printArea.y *= pagesNumV;
    }
    return printArea;
}

/**
 * @return Paper format.
 * This is determined by the variables "$PLIMMIN" and "$PLIMMAX".
 *
 * @param landscape will be set to true for landscape and false for portrait if not nullptr.
 */
RS2::PaperFormat RS_Graphic::getPaperFormat(bool* landscape) {
    RS_Vector size = RS_Units::convert(getPaperSize(),
                                       getUnit(), RS2::Millimeter);
    if (landscape) {
        *landscape = (size.x>size.y);
    }
    return RS_Units::paperSizeToFormat(size);
}

/**
 * Sets the paper format to the given format.
 */
void RS_Graphic::setPaperFormat(RS2::PaperFormat f, bool landscape) {
    RS_Vector size = RS_Units::paperFormatToSize(f);

    if (landscape != (size.x > size.y)) {
        std::swap(size.x, size.y);
    }

    setPaperSize(RS_Units::convert(size, RS2::Millimeter, getUnit()));
}

/**
 * @return Paper space scaling (DXF: $PSVPSCALE).
 */
double RS_Graphic::getPaperScale() const {
    double paperScale = getVariableDouble("$PSVPSCALE", 1.0);
    return paperScale;
}

/**
 * Sets a new scale factor for the paper space.
 */
void RS_Graphic::setPaperScale(double s) {
    if(paperScaleFixed==false) {
        addVariable("$PSVPSCALE", s, 40);
    }
}

/**
 * Centers drawing on page. Affects DXF variable $PINSBASE.
 */
void RS_Graphic::centerToPage() {
    RS_Vector paperSize = getPrintAreaSize();
    auto graphicSize=getSize();
    auto graphicMin=getMin();
    /** avoid zero size, bug#3573158 */
    if(std::abs(graphicSize.x)<RS_TOLERANCE) {
        graphicSize.x=10.;
        graphicMin.x=-5.;
    }
    if(std::abs(graphicSize.y)<RS_TOLERANCE) {
        graphicSize.y=10.;
        graphicMin.y=-5.;
    }

    const RS2::Unit unit = getUnit();
    RS_Vector paperMin{
        RS_Units::convert(marginLeft, RS2::Millimeter, unit),
        RS_Units::convert(marginBottom, RS2::Millimeter, unit)
    };
    // paper printable area center
    RS_Vector paperCenter = paperMin + paperSize * 0.5;
    const double scale = getPaperScale();
    // graphic center
    RS_Vector scaledCenter = (graphicMin + graphicSize * 0.5) * scale;

    // align graphic center to the paper center
    RS_Vector pinsbase = paperCenter - scaledCenter;

    setPaperInsertionBase(pinsbase);
}

/**
 * Fits drawing on page. Affects DXF variable $PINSBASE.
 */
  // fixme - check margins support in single and tiled modes - looks like they are shown differently
bool RS_Graphic::fitToPage()
{
    RS_Vector printSize = getPrintAreaSize(false);
    RS_Vector graphicSize = getSize();
    LC_ERR<<"graphic size: "<<graphicSize.x<<", "<<graphicSize.y;
    LC_ERR<<"printSize size: "<<printSize.x<<", "<<printSize.y;
    /** avoid zero size, bug#3573158 */
    if(std::abs(graphicSize.x)<RS_TOLERANCE)
        graphicSize.x=10.;
    if(std::abs(graphicSize.y)<RS_TOLERANCE)
        graphicSize.y=10.;
    double scaleX = RS_MAXDOUBLE;
    double scaleY = RS_MAXDOUBLE;

    // tin-pot 2011-12-30: TODO: can s.x < 0.0 (==> fx < 0.0) happen?
    if (std::abs(graphicSize.x) > RS_TOLERANCE) {
        scaleX = printSize.x / graphicSize.x;
    }
    if (std::abs(graphicSize.y) > RS_TOLERANCE) {
        scaleY = printSize.y / graphicSize.y;
    }

    double scale = std::min(scaleX, scaleY);
    if (scale >= RS_MAXDOUBLE || scale <= 1.0e-10) {
        setPaperSize(RS_Units::convert(g_paperSizeA4, RS2::Millimeter, getUnit()));
        RS_DIALOGFACTORY->commandMessage(QObject::tr("Invalid printing scale %1. Cannot fit print preview to page").arg(scale));
        return false;
    }
    setPaperScale(scale);
    centerToPage();
    return true;
}

bool RS_Graphic::isBiggerThanPaper() {
    RS_Vector ps = getPrintAreaSize();
    RS_Vector s = getSize() * getPaperScale();
    return !s.isInWindow(RS_Vector(0.0, 0.0), ps);
}

void RS_Graphic::addEntity(RS_Entity *entity) {
    RS_EntityContainer::addEntity(entity);
    if (entity->rtti() == RS2::EntityBlock ||
        entity->rtti() == RS2::EntityContainer) {
        auto *e = dynamic_cast<RS_EntityContainer *>(entity);
        for (auto e1: *e) {
            addEntity(e1);
        }
    }
}

/**
 * Dumps the entities to stdout.
 */
std::ostream& operator << (std::ostream& os, RS_Graphic& g) {
    os << "--- Graphic: \n";
    os << "---" << *g.getLayerList() << "\n";
    os << "---" << *g.getBlockList() << "\n";
    os << "---" << (RS_Undo&)g << "\n";
    os << "---" << (RS_EntityContainer&)g << "\n";
    return os;
}

/**
 * Removes invalid objects.
 * @return how many objects were removed
 */
int RS_Graphic::clean() {
    // author: ravas

    int how_many = 0;

    for(RS_Entity *e: std::as_const(entities)) {
        if (e == nullptr || !validRange(e->getMin(), e->getMax())) {
            removeEntity(e);
            how_many += 1;
        }
    }
    return how_many;
}

/**
 * Paper margins in graphic units
 */
void RS_Graphic::setMarginsInUnits(double left, double top, double right, double bottom) {
    RS2::Unit src = getUnit();
    setMargins(RS_Units::convert(left, src, RS2::Millimeter),
               RS_Units::convert(top, src, RS2::Millimeter),
               RS_Units::convert(right, src, RS2::Millimeter),
               RS_Units::convert(bottom, src, RS2::Millimeter));
}

double RS_Graphic::getMarginLeftInUnits() {
    return RS_Units::convert(marginLeft, RS2::Millimeter, getUnit());
}

double RS_Graphic::getMarginTopInUnits() {
    return RS_Units::convert(marginTop, RS2::Millimeter, getUnit());
}

double RS_Graphic::getMarginRightInUnits() {
    return RS_Units::convert(marginRight, RS2::Millimeter, getUnit());
}

double RS_Graphic::getMarginBottomInUnits() {
    return RS_Units::convert(marginBottom, RS2::Millimeter, getUnit());
}

void RS_Graphic::setPagesNum(int horiz, int vert) {
    if (horiz > 0)
        pagesNumH = horiz;
    if (vert > 0)
        pagesNumV = vert;
}

void RS_Graphic::setPagesNum(const QString &horizXvert) {
    if (horizXvert.contains('x')) {
        bool ok1 = false;
        bool ok2 = false;
        int i = horizXvert.indexOf('x');
        int h = (int)RS_Math::eval(horizXvert.left(i), &ok1);
        int v = (int)RS_Math::eval(horizXvert.mid(i+1), &ok2);
        if (ok1 && ok2)
            setPagesNum(h, v);
    }
}

QString RS_Graphic::formatAngle(double angle) const
{
    return RS_Units::formatAngle(angle, getAngleFormat(), getAnglePrecision());
}

QString RS_Graphic::formatLinear(double linear) const
{
    return RS_Units::formatLinear(linear, getUnit(), getLinearFormat(), getLinearPrecision(), false);
}
