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

#include "rs_actionmodifyexplodetext.h"
#include "rs_modification.h"

/**
 * Constructor.
 */
RS_ActionModifyExplodeText::RS_ActionModifyExplodeText(RS_EntityContainer& container,
        RS_GraphicView& graphicView)
        :LC_ActionPreSelectionAwareBase("Explode Text",
                           container, graphicView, {RS2::EntityMText, RS2::EntityText}) {
	actionType=RS2::ActionModifyExplodeText;
}

RS_ActionModifyExplodeText::~RS_ActionModifyExplodeText() = default;


void RS_ActionModifyExplodeText::doTrigger(bool keepSelected) {
    RS_Modification m(*container, viewport);
    m.explodeTextIntoLetters(selectedEntities, keepSelected);
}

void RS_ActionModifyExplodeText::updateMouseButtonHintsForSelection() {
    updateMouseWidgetTRCancel(tr("Select to explode text (Enter to complete)"), MOD_CTRL(tr("Explode immediately after selection")));
}
