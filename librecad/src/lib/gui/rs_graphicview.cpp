/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2015 A. Stebich (librecad@mail.lordofbikes.de)
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


#include "rs_graphicview.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QStringList>
#include <QSurfaceFormat>
#include <QTimer>

#include "lc_cursoroverlayinfo.h"
#include "lc_graphicviewport.h"
#include "lc_shortcuts_manager.h"
#include "lc_widgetviewportrenderer.h"
#if __has_include("lc_tracygpu.h")
#include "lc_tracygpu.h"
#else
class LC_TracyGpuMonitor {
public:
    static LC_TracyGpuMonitor& instance() {
        static LC_TracyGpuMonitor s;
        return s;
    }
    void configurePlots() {}
    void sampleGpuUsage(double = 0.0) {}
    void initOpenGLContext() {}
};
#endif
#include "rs_actioninterface.h"
#include "rs_debug.h"
#include "rs_entitycontainer.h"
#include "rs_eventhandler.h"
#include "rs_graphic.h"
#include "rs_grid.h"
#include "rs_linetypepattern.h"
#include "rs_selection.h"
#include "rs_settings.h"
#include "rs_snapper.h"
#include "tracy/Tracy.hpp"  // Tracy Profiler

#ifdef EMU_C99
#include "emu_c99.h"
#endif

#ifdef OPENGLACTIVE
class LC_OpenGLCanvas : public QOpenGLWidget {
public:
    explicit LC_OpenGLCanvas(RS_GraphicView *parentView)
        : QOpenGLWidget(parentView), m_view(parentView) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setFocusPolicy(Qt::NoFocus);
        setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    }

protected:
    void initializeGL() override {
        if (m_view != nullptr) {
            m_view->initializeGL();
        }
    }

    void resizeGL(int w, int h) override {
        if (m_view != nullptr) {
            m_view->resizeGL(w, h);
        }
    }

    void paintGL() override {
        if (m_view != nullptr) {
            m_view->paintGL();
        }
    }

private:
    RS_GraphicView *m_view = nullptr;
};
#endif

bool RS_GraphicView::detectGpuCapability() {
#ifndef OPENGLACTIVE
    TracyMessageLC("[GPU Check] OPENGLACTIVE nonaktif -> Mode CPU Asli (Checkpoint)", tracy::Color::Orange);
    TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
    return false;
#else
    static bool s_checked = false;
    static bool s_capable = false;

    if (s_checked) {
        return s_capable;
    }
    ZoneScopedNC("RS_GraphicView::detectGpuCapability", tracy::Color::Cyan);
    s_checked = true;

    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    QOpenGLContext ctx;
    ctx.setFormat(fmt);
    if (!ctx.create()) {
        LC_LOG << "RS_GraphicView: OpenGL context gagal dibuat, fallback menggunakan CPU.";
        TracyMessageLC("[GPU Check] OpenGL Context gagal dibuat -> Fallback CPU", tracy::Color::OrangeRed);
        TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
        s_capable = false;
        return false;
    }

    QOffscreenSurface surface;
    surface.setFormat(ctx.format());
    surface.create();
    if (!surface.isValid() || !ctx.makeCurrent(&surface)) {
        LC_LOG << "RS_GraphicView: OpenGL surface tidak valid, fallback menggunakan CPU.";
        TracyMessageLC("[GPU Check] OpenGL Surface tidak valid -> Fallback CPU", tracy::Color::OrangeRed);
        TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
        s_capable = false;
        return false;
    }

    QOpenGLFunctions *funcs = ctx.functions();
    if (funcs == nullptr) {
        ctx.doneCurrent();
        LC_LOG << "RS_GraphicView: QOpenGLFunctions tidak tersedia, fallback menggunakan CPU.";
        TracyMessageLC("[GPU Check] QOpenGLFunctions null -> Fallback CPU", tracy::Color::OrangeRed);
        TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
        s_capable = false;
        return false;
    }

    QSurfaceFormat actualFmt = ctx.format();
    if (actualFmt.majorVersion() < 2) {
        ctx.doneCurrent();
        LC_LOG << "RS_GraphicView: Versi OpenGL (" << actualFmt.majorVersion() << "." << actualFmt.minorVersion()
               << ") tidak memadai (< 2.0), fallback menggunakan CPU.";
        TracyMessageLC("[GPU Check] Versi OpenGL < 2.0 -> Fallback CPU", tracy::Color::OrangeRed);
        TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
        s_capable = false;
        return false;
    }

    if (!funcs->hasOpenGLFeature(QOpenGLFunctions::Framebuffers) ||
        !funcs->hasOpenGLFeature(QOpenGLFunctions::Shaders)) {
        ctx.doneCurrent();
        LC_LOG << "RS_GraphicView: Fitur OpenGL Framebuffer/Shader tidak didukung GPU, fallback menggunakan CPU.";
        TracyMessageLC("[GPU Check] Framebuffer/Shader tidak didukung -> Fallback CPU", tracy::Color::OrangeRed);
        TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
        s_capable = false;
        return false;
    }

    const char *vendorRaw = reinterpret_cast<const char *>(funcs->glGetString(GL_VENDOR));
    const char *rendererRaw = reinterpret_cast<const char *>(funcs->glGetString(GL_RENDERER));
    const char *versionRaw = reinterpret_cast<const char *>(funcs->glGetString(GL_VERSION));

    QString vendor = vendorRaw ? QString::fromLatin1(vendorRaw) : QString();
    QString renderer = rendererRaw ? QString::fromLatin1(rendererRaw) : QString();
    QString version = versionRaw ? QString::fromLatin1(versionRaw) : QString();

    ctx.doneCurrent();

    QByteArray gpuDescUtf8 = QString("Vendor: %1 | Renderer: %2 | GL: %3").arg(vendor, renderer, version).toUtf8();
    ZoneText(gpuDescUtf8.constData(), gpuDescUtf8.size());

    if (vendor.isEmpty() && renderer.isEmpty()) {
        LC_LOG << "RS_GraphicView: GPU tidak terdeteksi, fallback menggunakan CPU.";
        TracyMessageLC("[GPU Check] Vendor/Renderer kosong -> Fallback CPU", tracy::Color::OrangeRed);
        TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
        s_capable = false;
        return false;
    }

    QString combinedInfo = (vendor + " " + renderer).toLower();
    const QStringList unsupportedOrSoftwareRenderers = {
        "gdi generic",
        "microsoft basic render",
        "microsoft basic display",
        "llvmpipe",
        "softpipe",
        "swrast",
        "software rasterizer",
        "mesa offscreen"
    };

    for (const QString &swName : unsupportedOrSoftwareRenderers) {
        if (combinedInfo.contains(swName)) {
            LC_LOG << "RS_GraphicView: GPU hardware tidak mumpuni / software renderer terdeteksi ("
                   << vendor << " - " << renderer << "), fallback menggunakan CPU.";
            QByteArray fallbackMsg = QString("[GPU Check] Software/Basic Renderer (%1 - %2) -> Fallback CPU").arg(vendor, renderer).toUtf8();
            TracyMessageC(fallbackMsg.constData(), fallbackMsg.size(), tracy::Color::OrangeRed);
            TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
            s_capable = false;
            return false;
        }
    }

    LC_LOG << "RS_GraphicView: GPU terdeteksi dan mumpuni (" << vendor << " | " << renderer
           << " | OpenGL " << version << "). Menggunakan akselerasi OpenGL.";
    QByteArray okMsg = QString("[GPU Check] OpenGL GPU AKTIF: %1 | %2 | GL %3").arg(vendor, renderer, version).toUtf8();
    TracyMessageC(okMsg.constData(), okMsg.size(), tracy::Color::Green);
    TracyAppInfo(okMsg.constData(), okMsg.size());
    TracyPlot("RenderBackend (1=GPU, 0=CPU)", 1LL);
    s_capable = true;
    return true;
#endif
}

/**
 * Constructor.
 */
RS_GraphicView::RS_GraphicView(QWidget *parent, Qt::WindowFlags f)
    :QWidget(parent, f)
    , m_eventHandler{std::make_unique<RS_EventHandler>(this)}
    , m_viewport{std::make_unique<LC_GraphicViewport>()}
    , defaultSnapMode{std::make_unique<RS_SnapMode>()}
    , infoCursorOverlayPreferences{std::make_unique<LC_InfoCursorOverlayPrefs>()}{
    m_viewport->addViewportListener(this);

    LC_TracyGpuMonitor::instance().configurePlots();
    auto *gpuUsageTimer = new QTimer(this);
    connect(gpuUsageTimer, &QTimer::timeout, this, []() {
        LC_TracyGpuMonitor::instance().sampleGpuUsage();
    });
    gpuUsageTimer->start(200);

#ifdef OPENGLACTIVE
    if (detectGpuCapability()) {
        m_useOpenGL = true;
        m_glWidget = new LC_OpenGLCanvas(this);
        m_glWidget->setGeometry(rect());
        m_glWidget->lower();
    } else {
        m_useOpenGL = false;
        m_glWidget = nullptr;
    }
#else
    m_useOpenGL = false;
    m_glWidget = nullptr;
#endif
}

void RS_GraphicView::loadSettings() {
    LC_GROUP("Appearance");
    {
        m_panOnZoom = LC_GET_BOOL("PanOnZoom", false);
        m_skipFirstZoom = LC_GET_BOOL("FirstTimeNoZoom", false);

    }
    LC_GROUP_END();

    infoCursorOverlayPreferences->loadSettings();
    m_viewport->loadSettings();
    m_renderer->loadSettings();
}

RS_GraphicView::~RS_GraphicView() = default;

/**
 * Must be called by any derived class in the destructor.
 */
void RS_GraphicView::cleanUp() {
    m_bIsCleanUp = true;
}

/**
 * Sets the pointer to the graphic which contains the entities
 * which are visualized by this widget.
 */
void RS_GraphicView::setContainer(RS_EntityContainer *c) {
    container = c;
    m_viewport->setContainer(c);
//adjustOffsetControls();
}

/**
 * @return Current action or nullptr.
 */
RS_ActionInterface *RS_GraphicView::getDefaultAction() const {
    if (m_eventHandler!=nullptr) {
        return m_eventHandler->getDefaultAction();
    } else {
        return nullptr;
    }
}

void RS_GraphicView::hideOptions() const {
    if (m_eventHandler != nullptr) {
        auto defaultAction = m_eventHandler->getDefaultAction();
        if (defaultAction != nullptr) {
            defaultAction->hideOptions();
        }
    }
}

/**
 * Sets the default action of the event handler.
 */
void RS_GraphicView::setDefaultAction(RS_ActionInterface *action) const {
    if (m_eventHandler !=nullptr) {
        m_eventHandler->setDefaultAction(action);
    }
}

/**
 * @return Current action or nullptr.
 */
RS_ActionInterface *RS_GraphicView::getCurrentAction() const {
    return (nullptr != m_eventHandler) ? m_eventHandler->getCurrentAction() : nullptr;
}

QString RS_GraphicView::getCurrentActionName() const {
    if (m_eventHandler !=nullptr) {
        QAction* qaction = m_eventHandler->getQAction();
        if (qaction != nullptr){
          // todo - sand - actually, this is bad dependency, should be refactored
          return LC_ShortcutsManager::getPlainActionToolTip(qaction);
        }
    }
    return "";
}

QIcon RS_GraphicView::getCurrentActionIcon() const {
    if (m_eventHandler != nullptr) {
        QAction* qaction = m_eventHandler->getQAction();
        if (qaction != nullptr){
            return qaction->icon();
        }
    }
    return {};
}

bool RS_GraphicView::setEventHandlerAction(std::shared_ptr<RS_ActionInterface> action){
    bool actionActive = m_eventHandler->setCurrentAction(action);
    if (actionActive) {
        if (m_eventHandler->hasAction()) {
            emit currentActionChanged(action.get());
        }
        else {
            notifyNoActiveAction();
        }
    }
    return actionActive;
}

/**
 * Sets the current action of the event handler.
 */
bool RS_GraphicView::setCurrentAction(std::shared_ptr<RS_ActionInterface> action) {
    if (m_eventHandler != nullptr) {
        m_viewport->markRelativeZero();
        return setEventHandlerAction(action);
    }

    return false;
}

/**
 * Kills all running selection actions. Called when a selection action
 * is launched to reduce confusion.
 */
// fixme - sand - files - review this method again. Why it is so special and different from killAllActions?
// actually, selection may be performed by any inherited class of RS_ActionSelectBase...
// leave if for now as it, yet return later.
void RS_GraphicView::killSelectActions() const {
    if (m_eventHandler != nullptr) {
        m_eventHandler->killSelectActions();
    }
}

/**
 * Kills all running actions.
 */
void RS_GraphicView::killAllActions() const {
    if (m_eventHandler != nullptr && forcedActionKillAllowed) {
        m_eventHandler->killAllActions();
    }
}

/**
 * Go back in menu or current action.
 */
void RS_GraphicView::back() const {
    if (m_eventHandler && m_eventHandler->hasAction()) {
        m_eventHandler->back();
    }
}

/**
 * Go forward with the current action.
 */
void RS_GraphicView::processEnterKey() {
    if (m_eventHandler && m_eventHandler->hasAction()) {
        m_eventHandler->enter();
    }
}

void keyPressEvent(QKeyEvent *event);

void RS_GraphicView::keyPressEvent(QKeyEvent *event) {
    if (m_eventHandler && m_eventHandler->hasAction()) {
        m_eventHandler->keyPressEvent(event);
    }
}

/**
 * Called by the actual GUI class which implements a command line.
 */
void RS_GraphicView::commandEvent(RS_CommandEvent *e) {
    if (m_eventHandler) {
        m_eventHandler->commandEvent(e);
    }
}

/**
 * Enables coordinate input in the command line.
 */
void RS_GraphicView::enableCoordinateInput() {
    if (m_eventHandler) {
        m_eventHandler->enableCoordinateInput();
    }
}

/**
 * Disables coordinate input in the command line.
 */
void RS_GraphicView::disableCoordinateInput() {
    if (m_eventHandler) {
        m_eventHandler->disableCoordinateInput();
    }
}

void RS_GraphicView::zoomAuto(bool axis){
    m_viewport->zoomAuto(axis);
}

void RS_GraphicView::zoomPage() {
    m_viewport->zoomPage();
}

void RS_GraphicView::zoomPageEx() {
    m_viewport->zoomPageEx();
}

void RS_GraphicView::onViewportChanged() {
    ZoneScopedNC("RS_GraphicView::onViewportChanged (Pan/Zoom)", tracy::Color::Yellow);
    const char *backendTag = m_useOpenGL ? "Mode: GPU OpenGL" : "Mode: CPU Fallback";
    ZoneText(backendTag, std::strlen(backendTag));
    adjustOffsetControls();
    adjustZoomControls();
    QString info = m_viewport->getGrid()->getInfo();
    updateGridStatusWidget(info);
    redraw();
}

void RS_GraphicView::onViewportRedrawNeeded() {
    redraw(RS2::RedrawDrawing);
}

void RS_GraphicView::onUCSChanged(LC_UCS* ucs) {
    emit ucsChanged(ucs);
    QString info = m_viewport->getGrid()->getInfo();
    updateGridStatusWidget(info);
    redraw();
}

void RS_GraphicView::notifyNoActiveAction(){
    emit currentActionChanged(nullptr);
}

void RS_GraphicView::onRelativeZeroChanged(const RS_Vector &pos) {
    emit relativeZeroChanged(pos);
    redraw(RS2::RedrawOverlay);
}

/**
 * @return Pointer to the static pattern struct that belongs to the
 * given pattern type or nullptr.
 */
const RS_LineTypePattern *RS_GraphicView::getPattern(RS2::LineType t) {
    return RS_LineTypePattern::getPattern(t);
}

RS2::SnapRestriction RS_GraphicView::getSnapRestriction() const {
    return defaultSnapRes;
}

RS_SnapMode RS_GraphicView::getDefaultSnapMode() const {
    return *defaultSnapMode;
}

/**
 * Sets the default snap mode used by newly created actions.
 */
void RS_GraphicView::setDefaultSnapMode(RS_SnapMode sm) {
    *defaultSnapMode = sm;
    if (m_eventHandler) {
        m_eventHandler->setSnapMode(sm);
    }
}

/**
 * Sets a snap restriction (e.g. orthogonal).
 */
void RS_GraphicView::setSnapRestriction(RS2::SnapRestriction sr) {
    defaultSnapRes = sr;

    if (m_eventHandler) {
        m_eventHandler->setSnapRestriction(sr);
    }
}

RS_EventHandler *RS_GraphicView::getEventHandler() const {
    return m_eventHandler.get();
}

RS_Graphic *RS_GraphicView::getGraphic() const {
    if (container && container->rtti() == RS2::EntityGraphic) {
        return static_cast<RS_Graphic *>(container);
    }

    return nullptr;
}

RS_EntityContainer *RS_GraphicView::getContainer() const {
    return container;
}

void RS_GraphicView::switchToDefaultAction() {
    killAllActions();
    RS_Selection s(*container, m_viewport.get());
    s.selectAll(false);
    redraw(RS2::RedrawAll);
}

bool RS_GraphicView::isCleanUp(void) const {
    return m_bIsCleanUp;
}

/* Sets the hidden state for the relative-zero marker. */
void RS_GraphicView::setRelativeZeroHiddenState(bool isHidden) {
    return m_viewport->setRelativeZeroHiddenState(isHidden);
}

bool RS_GraphicView::isRelativeZeroHidden() {
    return m_viewport->isRelativeZeroHidden();
}

RS2::EntityType RS_GraphicView::getTypeToSelect() const {
    return typeToSelect;
}

void RS_GraphicView::setTypeToSelect(RS2::EntityType mType) {
    typeToSelect = mType;
}

void RS_GraphicView::setForcedActionKillAllowed(bool enabled) {
    forcedActionKillAllowed = enabled;
}

QString RS_GraphicView::obtainEntityDescription([[maybe_unused]]RS_Entity *entity, [[maybe_unused]]RS2::EntityDescriptionLevel descriptionLevel) {
    return "";
}

void RS_GraphicView::setShowEntityDescriptionOnHover(bool show) {
    showEntityDescriptionOnHover = show;
}

bool RS_GraphicView::getPanOnZoom() const{
    return m_panOnZoom;
}

bool RS_GraphicView::getSkipFirstZoom() const{
    return m_skipFirstZoom;
}

LC_InfoCursorOverlayPrefs* RS_GraphicView::getInfoCursorOverlayPreferences(){
    return infoCursorOverlayPreferences.get();
}

void RS_GraphicView::initializeGL() {
#ifdef OPENGLACTIVE
    ZoneScopedNC("RS_GraphicView::initializeGL [GPU]", tracy::Color::Green);
    if (!m_useOpenGL || m_glWidget == nullptr || !m_glWidget->isValid() || m_glWidget->context() == nullptr) {
        fallbackToCpu();
        return;
    }
    QOpenGLFunctions *funcs = m_glWidget->context()->functions();
    if (funcs != nullptr) {
        funcs->initializeOpenGLFunctions();
    }
    LC_TracyGpuMonitor::instance().initOpenGLContext();
    TracyMessageLC("[OpenGL Canvas] initializeGL berhasil", tracy::Color::Green);
#endif
}

void RS_GraphicView::resizeGL([[maybe_unused]] int w, [[maybe_unused]] int h) {
#ifdef OPENGLACTIVE
    ZoneScopedNC("RS_GraphicView::resizeGL [GPU]", tracy::Color::Green);
    if (!m_useOpenGL || m_glWidget == nullptr || !m_glWidget->isValid()) {
        fallbackToCpu();
        return;
    }
    if (m_viewport != nullptr) {
        m_viewport->setSize(getWidth(), getHeight());
    }
    if (m_renderer != nullptr) {
        m_renderer->invalidate(RS2::RedrawAll);
    }
#endif
}

void RS_GraphicView::paintGL() {
}

QPaintDevice* RS_GraphicView::getPaintDevice() {
#ifdef OPENGLACTIVE
    if (m_useOpenGL && m_glWidget != nullptr) {
        return m_glWidget;
    }
#endif
    return this;
}

void RS_GraphicView::fallbackToCpu() {
#ifdef OPENGLACTIVE
    ZoneScopedNC("RS_GraphicView::fallbackToCpu", tracy::Color::Red);
    if (!m_useOpenGL && m_glWidget == nullptr) {
        return;
    }
    LC_LOG << "RS_GraphicView: Melakukan fallback ke CPU rendering.";
    TracyMessageLC("[RENDER MODE] Fallback ke CPU diaktifkan!", tracy::Color::Red);
    TracyPlot("RenderBackend (1=GPU, 0=CPU)", 0LL);
    m_useOpenGL = false;
    if (m_glWidget != nullptr) {
        m_glWidget->hide();
        m_glWidget->deleteLater();
        m_glWidget = nullptr;
    }
    if (m_renderer != nullptr) {
        m_renderer->setPaintDevice(this);
        m_renderer->setUseOpenGL(false);
        m_renderer->invalidate(RS2::RedrawAll);
    }
    update();
#endif
}

void RS_GraphicView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    int w = getWidth();
    int h = getHeight();
#ifdef OPENGLACTIVE
    if (m_useOpenGL && m_glWidget != nullptr) {
        m_glWidget->setGeometry(0, 0, w, h);
        m_glWidget->lower();
    }
#endif
    m_viewport->setSize(w, h);
}

bool RS_GraphicView::isPrintPreview() const {
    return printPreview;
}

void RS_GraphicView::setPrintPreview(bool pv) {
    printPreview = pv;
}

void RS_GraphicView::setLineWidthScaling(bool state){
    m_renderer->setLineWidthScaling(state);
}

bool RS_GraphicView::getLineWidthScaling() const{
    return m_renderer->getLineWidthScaling();
}

LC_WidgetViewPortRenderer* RS_GraphicView::getRenderer() const{
    return m_renderer.get();
}

void RS_GraphicView::setRenderer(std::unique_ptr<LC_WidgetViewPortRenderer> renderer){
    m_renderer = std::move(renderer);
#ifdef OPENGLACTIVE
    if (m_renderer != nullptr) {
        m_renderer->setPaintDevice(getPaintDevice());
        m_renderer->setUseOpenGL(m_useOpenGL);
    }
#endif
}
