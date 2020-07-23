/**************************************************************************\
* Copyright (c) 2020 Adrian Przekwas <adrian.v.przekwas@gmail.com>
* Copyright (c) Bradley Austin Davis
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#ifndef GUI_CoinXRWidget
#define GUI_CoinXRWidget

#if BUILD_OPENXR

#define XR_USE_GRAPHICS_API_OPENGL//needed for xr::GraphicsRequirementsOpenGLKHR and xr::SwapchainImageOpenGLKHR
#if defined(WIN32)
#define XR_USE_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__ANDROID__)
#define XR_USE_PLATFORM_ANDROID
#elif defined(__linux__)
#define XR_USE_PLATFORM_XLIB
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

#include <3rdParty/openxr/openxr.hpp>

#undef Bool
#undef CursorShape
#undef Expose
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef None
#undef Status
#undef Unsorted
#undef True
#undef False
#undef Complex

#include <QApplication>
#include <QOpenGLWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <QThread>
#include <QOpenGLFunctions_4_5_Core>
#include <QKeyEvent>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoSceneManager.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoFrustumCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoTransform.h>



//QOpenGLWidget is a modern replacement for QGLWidget, it performs offscreen rendering
class CoinXRWidget  : public QOpenGLWidget , protected QOpenGLFunctions_4_5_Core
{
    SoSceneManager *m_sceneManager;
    SoSeparator *rootScene[2];
    SoFrustumCamera *camera[2];
    SoNode *scene;
    SoTranslation *camTrans[2];
    SoGroup *cGrp[2];
    SoGroup *sGrp[2];
    SoSeparator *con0Sep[2];
    SoSeparator *con1Sep[2];
    SoCube *conGizmo[2];
    SoTranslation *conTrans[2];
    SoRotation *conRotat[2];
    SoCylinder *conStick[2];
    SoRotation *stickRotat[2];
    QSurfaceFormat oldFormat;

    bool quit{ false };
    static const uint32_t hands = 2;
    uint32_t m_nRenderWidth;
    uint32_t m_nRenderHeight;
    uint32_t m_nScreenWidth;
    uint32_t m_nScreenHeight;
    constexpr static const float nearPlane = 0.01f;
    constexpr static const float farPlane = 10000.0f;
    struct GLFBO {
        GLuint id{ 0 };
        GLuint depthBuffer{ 0 };
    } fbo;
    xr::Instance instance;
    xr::SystemId systemId;
    xr::Session session;
    xr::FrameState frameState;
    xr::ActionSet actionSet;
    xr::Action poseAction;
    xr::Path handPaths[hands]; //avoid duplication af actions for both hands
    xr::Space handSpaces[hands];
    std::vector<xr::View> eyeViewStates;
    xr::DispatchLoaderDynamic dispatch;
    xr::GraphicsRequirementsOpenGLKHR graphicsRequirements;
    xr::SessionState sessionState{ xr::SessionState::Idle };
    xr::SwapchainCreateInfo swapchainCreateInfo;
    std::vector<xr::Swapchain> swapchains; //one swapchain per eye
    std::vector<std::vector<xr::SwapchainImageOpenGLKHR> > swapchainImagesArr; //two swapchain images (for both eyes)
    std::array<xr::CompositionLayerProjectionView, 2> projectionLayerViews;
    xr::CompositionLayerProjection projectionLayer{ {}, {}, 2, projectionLayerViews.data() };
    xr::Space& space{ projectionLayer.space };

    std::vector<xr::CompositionLayerBaseHeader*> layersPointers;

    void prepareXrInstance();
    void prepareXrSystem();
    void prepareXrSession();
    void prepareXrSwapchain();
    void prepareXrCompositionLayers();
    void prepareControls();
    void prepareScene();
    bool startXrFrame();
    void endXrFrame();
    void pollXrEvents();
    void updateXrViews();
    void updateXrControls();
    void onSessionStateChanged(const xr::EventDataSessionStateChanged& sessionStateChangedEvent);

public:
    explicit CoinXRWidget();
    ~CoinXRWidget();
    virtual void setSceneGraph(SoNode *sceneGraph);
    void setBase(const SbVec3f &pos){basePosition=pos;}
    void setBackgroundColor(const SbColor &Col);
    void quitRendering();

    SbVec3f    basePosition;
    SbRotation baseOrientation;

protected:
    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);
    SbColor BackgroundColor;
};

#endif //BUILD_OPENXR

#endif // GUI_CoinXRWidget
