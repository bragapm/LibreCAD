/*******************************************************************************
*
 This file is part of the LibreCAD project, a 2D CAD program

 Copyright (C) 2025 LibreCAD.org
 Copyright (C) 2025 sand1024

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ******************************************************************************/
#include "lc_plugininvoker.h"

#include <QAction>
#include <QDir>
#include <QMenuBar>
#include <QMessageBox>
#include <QPluginLoader>

#include "doc_plugin_interface.h"
#include "lc_undosection.h"
#include "qc_applicationwindow.h"
#include "qc_mdiwindow.h"
#include "qc_plugininterface.h"
#include "qg_graphicview.h"
#include "rs_debug.h"
#include "rs_system.h"

#include "rs_painter.h"

LC_PluginInvoker::LC_PluginInvoker(QC_ApplicationWindow *appWindow, LC_ActionContext* ctx):
    m_appWindow(appWindow),
    m_actionContext(ctx) {
}

LC_PluginInvoker::~LC_PluginInvoker() = default;

void LC_PluginInvoker::loadPlugins(){
    m_loadedPluginList.clear();
    QStringList lst = RS_SYSTEM->getDirectoryList("plugins");
    // Keep track of plugin filenames loaded to skip duplicate plugins.
    QStringList loadedPluginFileNames;

    for (int i = 0; i < lst.size(); ++i) {
        QDir pluginsDir(lst.at(i));
        for (const QString &fileName: pluginsDir.entryList(QDir::Files)) {
            // Skip loading a plugin if a plugin with the same
            // filename has already been loaded.
#ifdef Q_OS_MAC
            if (!fileName.contains(".dylib"))
                continue;
#endif
#if (defined (_WIN32) || defined (_WIN32) || defined (_WIN64))
            if (!fileName.contains(".dll"))
                continue;
#endif

            if (loadedPluginFileNames.contains(fileName)) {
                continue;
            }
            QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(fileName));
            QObject *plugin = pluginLoader.instance();
            if (plugin) {
                QC_PluginInterface *pluginInterface = qobject_cast<QC_PluginInterface *>(plugin);
                if (pluginInterface) {
                    // Check if a plugin with the same internal name was already loaded
                    QString pluginName = pluginInterface->name();
                    bool alreadyLoaded = false;
                    for (QC_PluginInterface* loadedPlugin : m_loadedPluginList) {
                        if (loadedPlugin->name() == pluginName) {
                            alreadyLoaded = true;
                            break;
                        }
                    }
                    if (alreadyLoaded) {
                        pluginLoader.unload();
                        continue;
                    }

                    m_loadedPluginList.push_back(pluginInterface);
                    loadedPluginFileNames.push_back(fileName);

                    PluginCapabilities pluginCapabilities = pluginInterface->getCapabilities();
                    QList<QAction*> plugActions;
                    for (const PluginMenuLocation& loc : pluginCapabilities.menuEntryPoints) {
                        auto menuBar = m_appWindow->menuBar();
                        if (loc.menuEntryActionName.isEmpty()) {
                            QMenu* menu = m_appWindow->findMenu(loc.menuEntryPoint, menuBar->children(), "");
                            if (menu) {
                                menu->addSeparator();
                            }
                            continue;
                        }

                        QString iconLoc = loc.menuIcon;
                        QAction* actpl = nullptr;
                        if (!iconLoc.isEmpty()) {
                            actpl = new QAction(QIcon(loc.menuIcon), loc.menuEntryActionName, plugin);
                        } else {
                            actpl = new QAction(loc.menuEntryActionName, plugin);
                        }
                        actpl->setVisible(loc.menuEntryVisible);
                        actpl->setData(loc.menuEntryActionName);
                        connect(actpl, &QAction::triggered, this, &LC_PluginInvoker::execPlug);
                        actpl->setEnabled(true);
                        if(actpl) plugActions.append(actpl);
                        //connect(m_appWindow, &QC_ApplicationWindow::windowsChanged, actpl, &QAction::setEnabled);

                        QStringList treemenu = loc.menuEntryPoint.split('/', Qt::SkipEmptyParts);
                        QMenu* parentMenu = nullptr;
                        QMenu* currentMenu = nullptr;
                        QString currentLevel;
                        for (const QString& menuName : treemenu) {
                            currentLevel = currentLevel.isEmpty() ? menuName : currentLevel + "/" + menuName;
                            currentMenu = m_appWindow->findMenu(currentLevel, menuBar->children(), "");
                            if (!currentMenu) {
                                if (!parentMenu) {
                                    currentMenu = menuBar->addMenu(menuName);
                                } else {
                                    currentMenu = parentMenu->addMenu(menuName);
                                }
                                currentMenu->setObjectName(menuName);
                            }
                            parentMenu = currentMenu;
                        }
                        if (currentMenu) currentMenu->addAction(actpl);
                    }
                    pluginInterface->plugInitialized(std::move(plugActions));
                }
            } else {
                QMessageBox::information(m_appWindow, "Info", pluginLoader.errorString());
                RS_DEBUG->print("QC_ApplicationWindow::loadPlugin: %s", pluginLoader.errorString().toLatin1().data());
            }
        }
    }
}

/**
 * Execute the plugin.
 */
void LC_PluginInvoker::execPlug(){
    auto *action = qobject_cast<QAction *>(sender());
    QC_PluginInterface *plugin = qobject_cast<QC_PluginInterface *>(action->parent());
    //get actual drawing
    QC_MDIWindow *w = m_appWindow->getCurrentMDIWindow();
    if(!w) {
        m_appWindow->slotFileNewFromDefaultTemplate();
        w = m_appWindow->getCurrentMDIWindow();
    }
    RS_Document *currdoc = w->getDocument();
    //create document interface instance
    QG_GraphicView *graphicView = w->getGraphicView();
    Doc_plugin_interface* pligundoc = new Doc_plugin_interface(m_actionContext, m_appWindow);
    //execute plugin
    LC_UndoSection undo(currdoc, graphicView->getViewPort());
    plugin->execComm(pligundoc, m_appWindow, action->data().toString());
    //TODO call update view
    graphicView->redraw();

}

void LC_PluginInvoker::drawPlugs(RS_Painter* painter, int flags){
    const LC_Rect& boundingBox = painter->getWcsBoundingRect();
    const int width = painter->getWidth();
    const int height = painter->getHeight();
    const double b = boundingBox.lowerLeftCorner().y;
    const double l = boundingBox.lowerLeftCorner().x;
    const double t = boundingBox.upperRightCorner().y;
    const double r = boundingBox.upperRightCorner().x;

    Doc_plugin_interface pligundoc(m_actionContext, m_appWindow);

    for(auto plug : m_loadedPluginList){
        if(plug){
            QImage* img = plug->render(&pligundoc, b, l, t, r, width, height, flags);
            if(img) painter->drawImage(0, 0, *img);
        }
    }
}
