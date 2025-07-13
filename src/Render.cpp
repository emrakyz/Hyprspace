
#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprlang.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

class CFakeDamageElement : public IPassElement {
public:
    CBox       box;

    CFakeDamageElement(const CBox& box) {
        this->box = box;
    }
    virtual ~CFakeDamageElement() = default;

    virtual void                draw(const CRegion& damage) {
        return;
    }
    virtual bool                needsLiveBlur() {
        return true;
    }
    virtual bool                needsPrecomputeBlur() {
        return false;
    }
    virtual std::optional<CBox> boundingBox() {
        return box.copy().scale(1.f / g_pHyprOpenGL->m_renderData.pMonitor->m_scale).round();
    }
    virtual CRegion             opaqueRegion() {
        return CRegion{};
    }
    virtual const char* passName() {
        return "CFakeDamageElement";
    }

};


void renderRect(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderRectWithBlur(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    rectdata.blur = true;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderBorder(CBox box, CGradientValueData gradient, int size) {
    CBorderPassElement::SBorderData data;
    data.box = box;
    data.grad1 = gradient;
    data.round = 0;
    data.a = 1.f;
    data.borderSize = size;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(data));
}

void renderWindowStub(PHLWINDOW pWindow, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, timespec* time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride || !time) return;

    SRenderModifData renderModif;

    const auto oWorkspace = pWindow->m_workspace;
    const auto oFullscreen = pWindow->m_fullscreenState;
    const auto oRealPosition = pWindow->m_realPosition->value();
    const auto oSize = pWindow->m_realSize->value();
    const auto oUseNearestNeighbor = pWindow->m_windowData.nearestNeighbor;
    const auto oPinned = pWindow->m_pinned;
    const auto oDraggedWindow = g_pInputManager->m_currentlyDraggedWindow;
    const auto oDragMode = g_pInputManager->m_dragMode;
    const auto oRenderModifEnable = g_pHyprOpenGL->m_renderData.renderModif.enabled;
    const auto oFloating = pWindow->m_isFloating;

    const float curScaling = rectOverride.w / (oSize.x * pMonitor->m_scale);

    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, (pMonitor->m_position * pMonitor->m_scale) + (rectOverride.pos() / curScaling) - (oRealPosition * pMonitor->m_scale)});
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    renderModif.enabled = true;
    pWindow->m_workspace = pWorkspaceOverride;
    pWindow->m_fullscreenState = SFullscreenState{FSMODE_NONE};
    pWindow->m_windowData.nearestNeighbor = false;
    pWindow->m_isFloating = false;
    pWindow->m_pinned = true;
    pWindow->m_windowData.rounding = CWindowOverridableVar<Hyprlang::INT>(pWindow->rounding() * curScaling * pMonitor->m_scale, eOverridePriority::PRIORITY_SET_PROP);
    g_pInputManager->m_currentlyDraggedWindow = pWindow;
    g_pInputManager->m_dragMode = MBIND_RESIZE;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{renderModif}));
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        });

    g_pHyprRenderer->damageWindow(pWindow);

    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, true, RENDER_PASS_ALL, false, false);

    pWindow->m_workspace = oWorkspace;
    pWindow->m_fullscreenState = oFullscreen;
    pWindow->m_windowData.nearestNeighbor = oUseNearestNeighbor;
    pWindow->m_isFloating = oFloating;
    pWindow->m_pinned = oPinned;
    pWindow->m_windowData.rounding.unset(eOverridePriority::PRIORITY_SET_PROP);
    g_pInputManager->m_currentlyDraggedWindow = oDraggedWindow;
    g_pInputManager->m_dragMode = oDragMode;
}

void renderLayerStub(PHLLS pLayer, PHLMONITOR pMonitor, CBox rectOverride, timespec* time) {
    if (!pLayer || !pMonitor || !time) return;

    if (!pLayer->m_mapped || pLayer->m_readyToDelete || !pLayer->m_layerSurface) return;

    Vector2D oRealPosition = pLayer->m_realPosition->value();
    Vector2D oSize = pLayer->m_realSize->value();
    float oAlpha = pLayer->m_alpha->value();
    const auto oRenderModifEnable = g_pHyprOpenGL->m_renderData.renderModif.enabled;
    const auto oFadingOut = pLayer->m_fadingOut;

    const float curScaling = rectOverride.w / (oSize.x);

    SRenderModifData renderModif;

    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, pMonitor->m_position + (rectOverride.pos() / curScaling) - oRealPosition});
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    renderModif.enabled = true;
    pLayer->m_alpha->setValue(1);
    pLayer->m_fadingOut = false;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{renderModif}));
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        });

    (*(tRenderLayer)pRenderLayer)(g_pHyprRenderer.get(), pLayer, pMonitor, time, false);

    pLayer->m_fadingOut = oFadingOut;
    pLayer->m_alpha->setValue(oAlpha);
}

void CHyprspaceWidget::draw() {

    workspaceBoxes.clear();

    if (!active && !curYOffset->isBeingAnimated()) return;

    auto owner = getOwner();

    if (!owner) return;

    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    g_pHyprOpenGL->m_renderData.pCurrentMonData->blurFBShouldRender = true;

    int bottomInvert = 1;
    if (Config::onBottom) bottomInvert = -1;

    CBox widgetBox = {owner->m_position.x, owner->m_position.y + (Config::onBottom * (owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->m_scale))) - (bottomInvert * curYOffset->value()), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale};

    widgetBox.x -= owner->m_position.x;
    widgetBox.y -= owner->m_position.y;

    g_pHyprOpenGL->m_renderData.clipBox = CBox({0, 0}, owner->m_transformedSize);

    if (!Config::disableBlur) {
        renderRectWithBlur(widgetBox, Config::panelBaseColor);
    }
    else {
        renderRect(widgetBox, Config::panelBaseColor);
    }

    if (Config::panelBorderWidth > 0) {
        CBox borderBox = {widgetBox.x, owner->m_position.y + (Config::onBottom * owner->m_transformedSize.y) + (Config::panelHeight + Config::reservedArea - curYOffset->value() * owner->m_scale) * bottomInvert, owner->m_transformedSize.x, static_cast<double>(Config::panelBorderWidth)};
        borderBox.y -= owner->m_position.y;

        renderRect(borderBox, Config::panelBorderColor);
    }

    g_pHyprRenderer->damageMonitor(owner);

    CFakeDamageElement fakeDamage = CFakeDamageElement(CBox({0, 0}, owner->m_transformedSize));
    g_pHyprRenderer->m_renderPass.add(makeUnique<CFakeDamageElement>(fakeDamage));

    std::vector<int> workspaces;

    if (Config::showSpecialWorkspace) {
        workspaces.push_back(SPECIAL_WORKSPACE_START);
    }

    int lowestID = INT_MAX;
    int highestID = 1;
    for (auto& ws : g_pCompositor->m_workspaces) {
        if (!ws) continue;
        if (ws->m_id < 1) continue;
        if (ws->m_monitor->m_id == ownerID) {
            workspaces.push_back(ws->m_id);
            if (highestID < ws->m_id) highestID = ws->m_id;
            if (lowestID > ws->m_id) lowestID = ws->m_id;
        }
    }

    if (Config::showEmptyWorkspace) {
        int wsIDStart = 1;
        int wsIDEnd = highestID;

        if (numWorkspaces > 0) {
            wsIDStart = std::min<int>(numWorkspaces * ownerID + 1, lowestID);
            wsIDEnd = std::max<int>(numWorkspaces * ownerID + 1, highestID);
        }

        for (int i = wsIDStart; i <= wsIDEnd; i++) {
            if (i == owner->activeSpecialWorkspaceID()) continue;
            const auto pWorkspace = g_pCompositor->getWorkspaceByID(i);
            if (pWorkspace == nullptr)
                workspaces.push_back(i);
        }
    }

    if (Config::showNewWorkspace) {
        while (g_pCompositor->getWorkspaceByID(highestID) != nullptr) highestID++;
        workspaces.push_back(highestID);
    }

    std::sort(workspaces.begin(), workspaces.end());

    int wsCount = workspaces.size();
    double monitorSizeScaleFactor = ((Config::panelHeight - 2 * Config::workspaceMargin) / (owner->m_transformedSize.y)) * owner->m_scale;
    double workspaceBoxW = owner->m_transformedSize.x * monitorSizeScaleFactor;
    double workspaceBoxH = owner->m_transformedSize.y * monitorSizeScaleFactor;
    double workspaceGroupWidth = workspaceBoxW * wsCount + (Config::workspaceMargin * owner->m_scale) * (wsCount - 1);
    double curWorkspaceRectOffsetX = Config::centerAligned ? workspaceScrollOffset->value() + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : workspaceScrollOffset->value() + Config::workspaceMargin;
    double curWorkspaceRectOffsetY = !Config::onBottom ? (((Config::reservedArea + Config::workspaceMargin) * owner->m_scale) - curYOffset->value()) : (owner->m_transformedSize.y - ((Config::reservedArea + Config::workspaceMargin) * owner->m_scale) - workspaceBoxH + curYOffset->value());
    double workspaceOverflowSize = std::max<double>(((workspaceGroupWidth - widgetBox.w) / 2) + (Config::workspaceMargin * owner->m_scale), 0);

    *workspaceScrollOffset = std::clamp<double>(workspaceScrollOffset->goal(), -workspaceOverflowSize, workspaceOverflowSize);

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;
    for (auto wsID : workspaces) {
        const auto ws = g_pCompositor->getWorkspaceByID(wsID);
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};

        if (ws == owner->m_activeWorkspace) {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceActiveBorder.a > 0) {
                renderBorder(curWorkspaceBox, CGradientValueData(Config::workspaceActiveBorder), Config::workspaceBorderSize);
            }
            if (!Config::disableBlur) {
                renderRectWithBlur(curWorkspaceBox, Config::workspaceActiveBackground);
            }
            else {
                renderRect(curWorkspaceBox, Config::workspaceActiveBackground);
            }
            if (!Config::drawActiveWorkspace) {
                curWorkspaceRectOffsetX += workspaceBoxW + (Config::workspaceMargin * owner->m_scale);
                continue;
            }
        }
        else {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceInactiveBorder.a > 0) {
                renderBorder(curWorkspaceBox, CGradientValueData(Config::workspaceInactiveBorder), Config::workspaceBorderSize);
            }
            if (!Config::disableBlur) {
                renderRectWithBlur(curWorkspaceBox, Config::workspaceInactiveBackground);
            }
            else {
                renderRect(curWorkspaceBox, Config::workspaceInactiveBackground);
            }
        }

        if (!Config::hideBackgroundLayers) {
            for (auto& ls : owner->m_layerSurfaceLayers[0]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->m_realPosition->value() - owner->m_position) * monitorSizeScaleFactor, ls->m_realSize->value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.lock(), owner, layerBox, &time);
                g_pHyprOpenGL->m_renderData.clipBox = CBox();
            }
            for (auto& ls : owner->m_layerSurfaceLayers[1]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->m_realPosition->value() - owner->m_position) * monitorSizeScaleFactor, ls->m_realSize->value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.lock(), owner, layerBox, &time);
                g_pHyprOpenGL->m_renderData.clipBox = CBox();
            }
        }

        if (owner->m_activeWorkspace == ws && Config::affectStrut) {
            CBox miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};
            if (Config::onBottom) miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY + workspaceBoxH - widgetBox.h * monitorSizeScaleFactor, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};

            if (!Config::disableBlur) {
                renderRectWithBlur(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }
            else {
                renderRect(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }

        }

        if (ws != nullptr) {
            for (auto& w : g_pCompositor->m_windows) {
                if (!w) continue;
                if (w->m_workspace == ws && !w->m_isFloating) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_realPosition->value().x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_realPosition->value().y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_realSize->value().x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_realSize->value().y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }
            }
            for (auto& w : g_pCompositor->m_windows) {
                if (!w) continue;
                if (w->m_workspace == ws && w->m_isFloating && ws->getLastFocusedWindow() != w) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_realPosition->value().x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_realPosition->value().y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_realSize->value().x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_realSize->value().y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }
            }
            if (ws->getLastFocusedWindow())
                if (ws->getLastFocusedWindow()->m_isFloating) {
                    const auto w = ws->getLastFocusedWindow();
                    double wX = curWorkspaceRectOffsetX + ((w->m_realPosition->value().x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_realPosition->value().y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_realSize->value().x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_realSize->value().y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }
        }

        if (owner->m_activeWorkspace != ws || !Config::hideRealLayers) {
            if (!Config::hideTopLayers)
                for (auto& ls : owner->m_layerSurfaceLayers[2]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->m_realPosition->value() - owner->m_position) * monitorSizeScaleFactor, ls->m_realSize->value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.lock(), owner, layerBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }

            if (!Config::hideOverlayLayers)
                for (auto& ls : owner->m_layerSurfaceLayers[3]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->m_realPosition->value() - owner->m_position) * monitorSizeScaleFactor, ls->m_realSize->value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.lock(), owner, layerBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }
        }

        curWorkspaceBox.scale(1 / owner->m_scale);

        curWorkspaceBox.x += owner->m_position.x;
        curWorkspaceBox.y += owner->m_position.y;
        workspaceBoxes.emplace_back(std::make_tuple(wsID, curWorkspaceBox));

        curWorkspaceRectOffsetX += workspaceBoxW + Config::workspaceMargin * owner->m_scale;
    }
}
