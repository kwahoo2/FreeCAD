/**************************************************************************\
* Copyright (c) 2020 Adrian Przekwas <adrian.v.przekwas@gmail.com>        *
* Copyright (c) Bradley Austin Davis                                      *
*                                                                         *
* All rights reserved.                                                    *
*                                                                         *
* Redistribution and use in source and binary forms, with or without      *
* modification, are permitted provided that the following conditions are  *
* met:                                                                    *
*                                                                         *
* Redistributions of source code must retain the above copyright notice,  *
* this list of conditions and the following disclaimer.                   *
*                                                                         *
* Redistributions in binary form must reproduce the above copyright       *
* notice, this list of conditions and the following disclaimer in the     *
* documentation and/or other materials provided with the distribution.    *
*                                                                         *
* Neither the name of the copyright holder nor the names of its           *
* contributors may be used to endorse or promote products derived from    *
* this software without specific prior written permission.                *
*                                                                         *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS     *
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT       *
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR   *
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT    *
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT        *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,   *
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY   *
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT     *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE   *
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.    *
\**************************************************************************/

#include "PreCompiled.h"
#include "CoinXRWidget.h"

#include "Base/Exception.h"
#include <Base/Console.h>

#if BUILD_OPENXR

#undef max

CoinXRWidget::CoinXRWidget() : QOpenGLWidget()
{
     mXRi = new XRInteraction();
     mXRi->setPriAndSecController(primaryConId, secondaryConId);

    uint32_t flatScale = 2;
    QSurfaceFormat format;
    format = this->format();
    oldFormat = format;
    format.setSwapInterval(0); //Disable vsync, otherwise Qt would sync HMD with flat screen which is a lot slower in most cases
    format.setProfile(QSurfaceFormat::OpenGLContextProfile::CoreProfile);
    format.setMajorVersion(4);
    format.setMinorVersion(5);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSamples(8); //MSAA
    this->format().setDefaultFormat(format);

    prepareXrInstance();
    prepareXrSystem();
    m_nScreenWidth = m_nRenderWidth / flatScale;
    m_nScreenHeight = m_nRenderHeight / flatScale;
    resize(static_cast<int>(m_nScreenWidth), static_cast<int>(m_nScreenHeight));
    prepareXrSession();
    prepareXrSwapchain();
    prepareXrCompositionLayers();
    prepareControls();
    prepareScene();

    eTimer.start();

}

CoinXRWidget::~CoinXRWidget()
{
    makeCurrent();

    xr::for_each_side_index([&](uint32_t eyeIndex) {
        rootScene[eyeIndex]->unref();
    });

    if (fbo.id != 0) {
        glDeleteFramebuffers(1, &fbo.id);
        fbo.id = 0;
    }

    if (fbo.depthBuffer != 0) {
        glDeleteRenderbuffers(1, &fbo.depthBuffer);
        fbo.depthBuffer = 0;
    }

    if (swapchain) {
        swapchain.destroy();
        swapchain = nullptr;
    }

    if (session) {
        session.destroy();
        session = nullptr;
    }

    if (instance) {
        instance.destroy();
        instance = nullptr;
    }
    delete  mXRi;
    delete m_sceneManager;
    format().setDefaultFormat(oldFormat);
    doneCurrent();
    Base::Console().Warning("OpenXR session closed \n");
}

void CoinXRWidget::setBackgroundColor(const SbColor &Col)
{
    BackgroundColor = Col;
}

void CoinXRWidget::setSceneGraph(SoNode *sceneGraph)
{
    wSep->replaceChild(scene, sceneGraph);
    scene = sceneGraph;
}

void CoinXRWidget::resizeGL(int width, int height)
{
    int side = qMin(width, height);
    glViewport((width - side) / 2, (height - side) / 2, side, side);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, 0.0, 1000.0);
    glMatrixMode(GL_MODELVIEW);
}

void CoinXRWidget::initializeGL()
{
    makeCurrent();
    initializeOpenGLFunctions();
    glEnable(GL_MULTISAMPLE);
    // Store old framebuffer.
    GLint oldfb;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldfb);
    // Create a depth renderbuffer compatible with the Swapchain sample count and size
    glGenRenderbuffers(1, &fbo.depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, fbo.depthBuffer);
    if (swapchainCreateInfo.sampleCount == 1) {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, static_cast<GLsizei>(swapchainCreateInfo.width), static_cast<GLsizei>(swapchainCreateInfo.height));
    } else {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, static_cast<GLsizei>(swapchainCreateInfo.sampleCount), GL_DEPTH24_STENCIL8,
                                         static_cast<GLsizei>(swapchainCreateInfo.width), static_cast<GLsizei>(swapchainCreateInfo.height));
    }

    // Create a framebuffer and attach the depth buffer to it
    glGenFramebuffers(1, &fbo.id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo.id);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fbo.depthBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldfb));
    doneCurrent();
}
void CoinXRWidget::paintGL()
{
    pollXrEvents();
    if (quit) {
        return;
    }
    GLint oldfb;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldfb);

    if (startXrFrame()) {
        updateXrViews();
        updateXrControls();
        if (frameState.shouldRender) {
            // Clear state pollution
            glUseProgram(0);

            uint32_t swapchainIndex;

            swapchain.acquireSwapchainImage(xr::SwapchainImageAcquireInfo{}, &swapchainIndex);
            swapchain.waitSwapchainImage(xr::SwapchainImageWaitInfo{ xr::Duration::infinite() });
#if defined XR_USE_PLATFORM_XLIB
            glXMakeCurrent(xDisplay, glxDrawable, glxContext); //SteamVR bug workaround https://github.com/ValveSoftware/SteamVR-for-Linux/issues/421
#endif
            glBindFramebuffer(GL_FRAMEBUFFER, fbo.id);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, swapchainImages[swapchainIndex].image, 0);

            // "render" to the swapchain image
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, static_cast<int32_t>(m_nRenderWidth) / 2, static_cast<int32_t>(m_nRenderHeight));
            vpReg.setViewportPixels(0, 0, static_cast<short>(m_nRenderWidth) / 2, static_cast<short>(m_nRenderHeight));
            m_sceneManager->setViewportRegion(vpReg);
            m_sceneManager->setSceneGraph(rootScene[0]);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            m_sceneManager->render();
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glClearDepth(1.0);

            glScissor(static_cast<int32_t>(m_nRenderWidth) / 2, 0, static_cast<int32_t>(m_nRenderWidth) / 2, static_cast<int32_t>(m_nRenderHeight));
            vpReg.setViewportPixels(static_cast<short>(m_nRenderWidth) / 2, 0, static_cast<short>(m_nRenderWidth) / 2, static_cast<short>(m_nRenderHeight));
            m_sceneManager->setViewportRegion(vpReg);
            m_sceneManager->setSceneGraph(rootScene[1]);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            m_sceneManager->render();
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glClearDepth(1.0);

            // fast blit from the fbo to the window surface
            glDisable(GL_SCISSOR_TEST);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldfb));
            glBlitFramebuffer(
                        0, 0, static_cast<int>(m_nRenderWidth), static_cast<int>(m_nRenderHeight),
                        0, 0, static_cast<int>(m_nScreenWidth), static_cast<int>(m_nScreenHeight),
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);


            // Continue rendering to the original frame buffer (likely 0, the onscreen buffer).
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldfb));
            Q_ASSERT(!glGetError());
            swapchain.releaseSwapchainImage(xr::SwapchainImageReleaseInfo{});

        }
        qint64 et = eTimer.nsecsElapsed();
        eTimer.restart();

        movSpeed = static_cast<float>(et) * 0.000000005f;
        if (et > 0)
        {
            qint64 frt = 1000000000 / et;
            this->setWindowTitle(QString::fromStdString("FreeCAD OpenXR - Framerate: ") + QString::number(frt) + QString::fromStdString(" FPS"));
        }
        endXrFrame();
        update(); //schedule QOpenGL composition
        updateXrGui();
    }
}

void CoinXRWidget::prepareXrInstance()
{
    std::unordered_map<std::string, xr::ExtensionProperties> discoveredExtensions;
    for (const auto& extensionProperties : xr::enumerateInstanceExtensionProperties(nullptr)) {
        discoveredExtensions.insert({ extensionProperties.extensionName, extensionProperties });
    }

    std::vector<const char*> requestedExtensions;
    if (0 == discoveredExtensions.count(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME)) {
        std::stringstream str;
        str << "Required Graphics API extension not available: " << XR_KHR_OPENGL_ENABLE_EXTENSION_NAME << std::ends;
        throw Base::TypeError(str.str());
    }
    requestedExtensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);

    xr::InstanceCreateInfo ici{ {},
                                { "FreeCAD_XR", 0, "openXrSamples", 0, xr::Version::current() },
                                0,
                                nullptr,
                                static_cast<uint32_t>(requestedExtensions.size()),
                requestedExtensions.data() };

    // Create the actual instance
    instance = xr::createInstance(ici);

    // Having created the instance, the very first thing to do is populate the dynamic dispatch, loading
    // all the available functions from the runtime
    dispatch = xr::DispatchLoaderDynamic::createFullyPopulated(instance, &xrGetInstanceProcAddr);
}

void CoinXRWidget::prepareXrSystem()
{

    // We want to create an HMD example, so we ask for a runtime that supposts that form factor
    // and get a response in the form of a systemId
    systemId = instance.getSystem(xr::SystemGetInfo{ xr::FormFactor::HeadMountedDisplay });

    // Find out what view configurations we have available
    {
        auto viewConfigTypes = instance.enumerateViewConfigurations(systemId);
        auto viewConfigType = viewConfigTypes[0];
        if (viewConfigType != xr::ViewConfigurationType::PrimaryStereo) {
            throw std::runtime_error("Example only supports stereo-based HMD rendering");
        }

    }
    std::vector<xr::ViewConfigurationView> viewConfigViews =
            instance.enumerateViewConfigurationViews(systemId, xr::ViewConfigurationType::PrimaryStereo);

    if (viewConfigViews.size() != 2) {
        throw std::runtime_error("Unexpected number of view configurations");
    }

    if (viewConfigViews[0].recommendedImageRectHeight != viewConfigViews[1].recommendedImageRectHeight) {
        throw std::runtime_error("Per-eye images have different recommended heights");
    }

    m_nRenderWidth = viewConfigViews[0].recommendedImageRectWidth * 2; //renderTargetSize = { viewConfigViews[0].recommendedImageRectWidth, viewConfigViews[0].recommendedImageRectHeight };
    m_nRenderHeight = viewConfigViews[0].recommendedImageRectHeight;

    graphicsRequirements = instance.getOpenGLGraphicsRequirementsKHR(systemId, dispatch);
}
void CoinXRWidget::prepareXrSession()
{
#if defined XR_USE_PLATFORM_WIN32
    xr::GraphicsBindingOpenGLWin32KHR graphicsBinding{ wglGetCurrentDC(), wglGetCurrentContext() };
#endif
    //https://www.khronos.org/registry/OpenXR/specs/1.0/man/html/XrGraphicsBindingOpenGLXlibKHR.html
#if defined XR_USE_PLATFORM_XLIB

    makeCurrent();

    uint32_t visualid = 0;
    GLXFBConfig glxFBConfig = nullptr;
    xDisplay = XOpenDisplay(nullptr);
    //Display* xDisplay = XOpenDisplay(nullptr);
    int xScreen = XDefaultScreen(xDisplay);
    glxContext = glXGetCurrentContext();
    //GLXContext glxContext = glXGetCurrentContext();
    glxDrawable = glXGetCurrentDrawable();
    //GLXDrawable glxDrawable = glXGetCurrentDrawable();


    /*To render using OpenGL into a GLX drawable, you must determine the appropriate GLXFBConfig that supports the rendering features your application requires. glXChooseFBConfig returns a GLXFBConfig matching the required attributes or NULL if no match is found. A complete list of GLXFBConfigs supported by a server can be obtained by calling glXGetFBConfigs. Attributes of a particular GLXFBConfig can be queried by calling glXGetFBConfigAttrib. */
    int fbConfigCount = 0;
    GLXFBConfig *fbConfigs = glXGetFBConfigs(xDisplay, xScreen, &fbConfigCount);


    if (fbConfigCount == 0) {
        throw std::runtime_error("No valid framebuffer configurations found");
    }

    int value;
    for (int i = 0; i < fbConfigCount; i++) {
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_FBCONFIG_ID, &value);
        if (value == 0) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_VISUAL_ID, &value);
        if (value == 0) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_DOUBLEBUFFER, &value);
        if (value == 0) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_RENDER_TYPE, &value);
        if ((value & GLX_RGBA_BIT) == 0) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_DRAWABLE_TYPE, &value);
        if ((value & GLX_WINDOW_BIT) == 0) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_RED_SIZE, &value);
        if (value != 8) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_GREEN_SIZE, &value);
        if (value != 8) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_BLUE_SIZE, &value);
        if (value != 8) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_ALPHA_SIZE, &value);
        if (value != 8) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_DEPTH_SIZE, &value);
        if (value != 24) {
            continue;
        }
        glXGetFBConfigAttrib(xDisplay, fbConfigs[i], GLX_VISUAL_ID, &value);
        visualid = static_cast<uint32_t>(value);
        glxFBConfig = fbConfigs[i];
        std::cout << "Visual id: " <<  value << std::endl;
        break;
    }

    xr::GraphicsBindingOpenGLXlibKHR graphicsBinding { xDisplay,
                visualid,
                glxFBConfig,
                glxDrawable,
                glxContext };

#endif

    xr::SessionCreateInfo sci{ {}, systemId };
    sci.next = &graphicsBinding;
    session = instance.createSession(sci);

    auto referenceSpaces = session.enumerateReferenceSpaces();
    //space = session.createReferenceSpace(xr::ReferenceSpaceCreateInfo{ xr::ReferenceSpaceType::Local }); //better for seated
    space = session.createReferenceSpace(xr::ReferenceSpaceCreateInfo{ xr::ReferenceSpaceType::Stage }); //better for room-scale, origin in the middle of the floor

    auto swapchainFormats = session.enumerateSwapchainFormats();
    doneCurrent();
}
void CoinXRWidget::prepareXrSwapchain()
{
        swapchainCreateInfo.usageFlags = xr::SwapchainUsageFlagBits::TransferDst;
        swapchainCreateInfo.format = static_cast<int64_t>(GL_SRGB8_ALPHA8);
        swapchainCreateInfo.sampleCount = 1;
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.width = m_nRenderWidth;
        swapchainCreateInfo.height = m_nRenderHeight;
        swapchain = session.createSwapchain(swapchainCreateInfo);
        swapchainImages = swapchain.enumerateSwapchainImages<xr::SwapchainImageOpenGLKHR>();

}
void CoinXRWidget::prepareXrCompositionLayers()
{
    //session.getReferenceSpaceBoundsRect(xr::ReferenceSpaceType::Local, bounds);
    projectionLayer.viewCount = 2;
    projectionLayer.views = projectionLayerViews.data();
    layersPointers.push_back(&projectionLayer);
    // Finish setting up the layer submission
    xr::for_each_side_index([&](uint32_t eyeIndex) {
        auto& layerView = projectionLayerViews[eyeIndex];
        layerView.subImage.swapchain = swapchain;
        layerView.subImage.imageRect.extent = { static_cast<int32_t>(m_nRenderWidth) / 2, static_cast<int32_t>(m_nRenderHeight) };
        if (eyeIndex == 1) {
            layerView.subImage.imageRect.offset.x = layerView.subImage.imageRect.extent.width;
        }
    });
}

void CoinXRWidget::prepareControls()
{

    for (uint32_t i = 0; i < hands; i++)
    {
        currTriggerVal[i] = 0.0f;
        oldTriggerVal[i] = 0.0f;
    }

    xr::ActionSetCreateInfo actionSetInfo{ };
    actionSetInfo.priority = 0;
    actionSetInfo.type = xr::StructureType::ActionSetCreateInfo;
    actionSetInfo.next = nullptr;
    strcpy(actionSetInfo.actionSetName, "baseactionset");
    strcpy(actionSetInfo.localizedActionSetName, "Base Action Set");

    actionSet = instance.createActionSet(actionSetInfo);

    xr::DispatchLoaderDynamic dispatch{ instance }; //exposes all the core functionality and all the known extensions

    dispatch.xrStringToPath(instance, "/user/hand/left", handPaths[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right", handPaths[1].put());

    xr::ActionCreateInfo actionInfo { };
    actionInfo.next = nullptr;
    actionInfo.actionType = xr::ActionType::PoseInput; //XR_ACTION_TYPE_POSE_INPUT
    strcpy(actionInfo.actionName, "hand_pose");
    strcpy(actionInfo.localizedActionName, "Hand Pose");
    actionInfo.countSubactionPaths = hands;
    actionInfo.subactionPaths = handPaths;
    poseAction = actionSet.createAction(actionInfo);

    actionInfo.actionType = xr::ActionType::FloatInput; //XR_ACTION_TYPE_FLOAT_INPUT
    strcpy(actionInfo.actionName, "x_lever");
    strcpy(actionInfo.localizedActionName, "Move a lever left or right");
    xLeverAction = actionSet.createAction(actionInfo);

    strcpy(actionInfo.actionName, "y_lever");
    strcpy(actionInfo.localizedActionName, "Move a lever forward or backward");
    yLeverAction = actionSet.createAction(actionInfo);

    strcpy(actionInfo.actionName, "triggergrab");
    strcpy(actionInfo.localizedActionName, "Grab Object with Trigger Button");
    grabAction = actionSet.createAction(actionInfo);


    xr::Path posePaths[hands];
    dispatch.xrStringToPath(instance, "/user/hand/left/input/aim/pose", posePaths[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right/input/aim/pose", posePaths[1].put());

    xr::Path triggerValuePath[hands];
    dispatch.xrStringToPath(instance, "/user/hand/left/input/trigger/value", triggerValuePath[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right/input/trigger/value", triggerValuePath[1].put());

    xr::Path thumbstickXPath[hands];
    dispatch.xrStringToPath(instance, "/user/hand/left/input/thumbstick/x", thumbstickXPath[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right/input/thumbstick/x", thumbstickXPath[1].put());

    xr::Path thumbstickYPath[hands];
    dispatch.xrStringToPath(instance, "/user/hand/left/input/thumbstick/y", thumbstickYPath[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right/input/thumbstick/y", thumbstickYPath[1].put());

    xr::Path trackpadXPath[hands];
    dispatch.xrStringToPath(instance, "/user/hand/left/input/trackpad/x", trackpadXPath[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right/input/trackpad/x", trackpadXPath[1].put());

    xr::Path trackpadYPath[hands];
    dispatch.xrStringToPath(instance, "/user/hand/left/input/trackpad/y", trackpadYPath[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right/input/trackpad/y", trackpadYPath[1].put());


    xr::Path interactionProfilePath;
    xr::InteractionProfileSuggestedBinding suggestedBindings { };
    suggestedBindings.type = xr::StructureType::InteractionProfileSuggestedBinding; //XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING
    suggestedBindings.next = nullptr;
    //dispatch.xrStringToPath(instance, "/interaction_profiles/khr/simple_controller", interactionProfilePath.put());

    {
        dispatch.xrStringToPath(instance, "/interaction_profiles/valve/index_controller", interactionProfilePath.put());

        const xr::ActionSuggestedBinding bindings[] {
            {poseAction,
                        posePaths[0]},
            {poseAction,
                        posePaths[1]},
            {xLeverAction,
                        thumbstickXPath[0]},
            {xLeverAction,
                        thumbstickXPath[1]},
            {yLeverAction,
                        thumbstickYPath[0]},
            {yLeverAction,
                        thumbstickYPath[1]},
            {grabAction,
                        triggerValuePath[0]},
            {grabAction,
                        triggerValuePath[1]}
        };
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.countSuggestedBindings = sizeof(bindings) / sizeof(bindings[0]);
        instance.suggestInteractionProfileBindings(suggestedBindings);
    }

    {
        dispatch.xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", interactionProfilePath.put());
        const xr::ActionSuggestedBinding bindings[] {
            {poseAction,
                        posePaths[0]},
            {poseAction,
                        posePaths[1]},
            {xLeverAction,
                        thumbstickXPath[0]},
            {xLeverAction,
                        thumbstickXPath[1]},
            {yLeverAction,
                        thumbstickYPath[0]},
            {yLeverAction,
                        thumbstickYPath[1]},
            {grabAction,
                        triggerValuePath[0]},
            {grabAction,
                        triggerValuePath[1]}
        };
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.countSuggestedBindings = sizeof(bindings) / sizeof(bindings[0]);
        instance.suggestInteractionProfileBindings(suggestedBindings);
    }

    {
        dispatch.xrStringToPath(instance, "/interaction_profiles/htc/vive_controller", interactionProfilePath.put());

        const xr::ActionSuggestedBinding bindings[] {
            {poseAction,
                        posePaths[0]},
            {poseAction,
                        posePaths[1]},
            {xLeverAction,
                        trackpadXPath[0]},
            {xLeverAction,
                        trackpadXPath[1]},
            {yLeverAction,
                        trackpadYPath[0]},
            {yLeverAction,
                        trackpadYPath[1]},
            {grabAction,
                        triggerValuePath[0]},
            {grabAction,
                        triggerValuePath[1]}
        };
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.countSuggestedBindings = sizeof(bindings) / sizeof(bindings[0]);
        instance.suggestInteractionProfileBindings(suggestedBindings);
    }

    {
        dispatch.xrStringToPath(instance, "/interaction_profiles/microsoft/motion_controller", interactionProfilePath.put());

        const xr::ActionSuggestedBinding bindings[] {
            {poseAction,
                        posePaths[0]},
            {poseAction,
                        posePaths[1]},
            {xLeverAction,
                        thumbstickXPath[0]},
            {xLeverAction,
                        thumbstickXPath[1]},
            {yLeverAction,
                        thumbstickYPath[0]},
            {yLeverAction,
                        thumbstickYPath[1]},
            {grabAction,
                        triggerValuePath[0]},
            {grabAction,
                        triggerValuePath[1]}
        };
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.countSuggestedBindings = sizeof(bindings) / sizeof(bindings[0]);
        instance.suggestInteractionProfileBindings(suggestedBindings);
    }

    xr::ActionSpaceCreateInfo actionSpaceInfo {};
    actionSpaceInfo.type = xr::StructureType::ActionSpaceCreateInfo;
    actionSpaceInfo.next = nullptr;
    actionSpaceInfo.action = poseAction;
    actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
    actionSpaceInfo.subactionPath = handPaths[0];

    session.createActionSpace(actionSpaceInfo, handSpaces[0]);
    actionSpaceInfo.subactionPath = handPaths[1];
    session.createActionSpace(actionSpaceInfo, handSpaces[1]);

    xr::SessionActionSetsAttachInfo attachInfo {};
    attachInfo.type = xr::StructureType::SessionActionSetsAttachInfo;
    attachInfo.next = nullptr;
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &actionSet;

    session.attachSessionActionSets(attachInfo);

}


void CoinXRWidget::prepareScene()
{
    m_sceneManager = new SoSceneManager();
    vpReg = SbViewportRegion(static_cast<short>(m_nRenderWidth), static_cast<short>(m_nRenderHeight));
    m_sceneManager->setViewportRegion(vpReg);
    m_sceneManager->setBackgroundColor(SbColor(.0f, .0f, .8f));

    hmdrot = SbRotation(0.0f, 0.0f, 0.0f, 0.0f);
    hmdpos = SbVec3f(0.0f, 0.0f, 0.0f);

    // light handling
    SoDirectionalLight *light = new SoDirectionalLight();
    light->direction.setValue(1,-1,-1);
    SoDirectionalLight *light2 = new SoDirectionalLight();
    light2->direction.setValue(-1,-1,-1);
    light2->intensity.setValue(0.6f);
    light2->color.setValue(0.8f,0.8f,1.0f);

    for (uint32_t id = 0; id < hands; id++) { //drawing controllers gizmos
        conTrans[id] = new SoTranslation();
        conRotat[id] = new SoRotation();
        conTrans[id]->translation.setValue(0, 0, 0);
        conRotat[id]->rotation.setValue(0, 0, 0, 0);
        conGizmo[id] = new SoCube();
        conGizmo[id]->width.setValue(0.1f);
        conGizmo[id]->height.setValue(0.05f);
        conGizmo[id]->depth.setValue(0.3f);
        conStick[id] = new SoCylinder();
        stickRotat[id] = new SoRotation();
        stickRotat[id]->rotation.setValue(0, 0, 0, 0);
        conStick[id]->radius.setValue(0.03f);
        conStick[id]->height.setValue(0.1f);
    }

    scene = new SoSeparator(0); // Placeholder.

    wSep = new SoSeparator;
    worldTransform = new SoTransform(); //use for navigation in the world
    transfMod = new SoTransform(); //modifier of transformation

    //wSep->addChild(worldTransform);
    wSep->addChild(scene);

    conMenuSep = new SoSeparator;

    xr::for_each_side_index([&](uint32_t eyeIndex) {
        rootScene[eyeIndex] = new SoSeparator;
        cGrp[eyeIndex] = new SoGroup();
        sGrp[eyeIndex] = new SoGroup();
        rootScene[eyeIndex]->ref();

        camera[eyeIndex] = new SoFrustumCamera();
        camera[eyeIndex]->nearDistance.setValue(nearPlane);
        camera[eyeIndex]->farDistance.setValue(farPlane);
        camera[eyeIndex]->focalDistance.setValue(5.0f);
        camera[eyeIndex]->viewportMapping.setValue(SoCamera::LEAVE_ALONE);

        rootScene[eyeIndex]->addChild(cGrp[eyeIndex]);
        cGrp[eyeIndex]->addChild(camera[eyeIndex]);

        rootScene[eyeIndex]->addChild(sGrp[eyeIndex]);
        sGrp[eyeIndex]->addChild(light);
        sGrp[eyeIndex]->addChild(light2);

        //controllers
        con0Sep[eyeIndex] = new SoSeparator();
        con1Sep[eyeIndex] = new SoSeparator();
        sGrp[eyeIndex]->addChild(con0Sep[eyeIndex]);
        con0Sep[eyeIndex]->addChild(conTrans[primaryConId]);
        con0Sep[eyeIndex]->addChild(conRotat[primaryConId]);
        con0Sep[eyeIndex]->addChild(conGizmo[primaryConId]);
        con0Sep[eyeIndex]->addChild(conMenuSep); //menu follows controller placement
        con0Sep[eyeIndex]->addChild(stickRotat[primaryConId]);
        con0Sep[eyeIndex]->addChild(conStick[primaryConId]);
        sGrp[eyeIndex]->addChild(con1Sep[eyeIndex]);
        con1Sep[eyeIndex]->addChild(conTrans[secondaryConId]);
        con1Sep[eyeIndex]->addChild(conRotat[secondaryConId]);
        con1Sep[eyeIndex]->addChild(conGizmo[secondaryConId]);
        con1Sep[eyeIndex]->addChild(stickRotat[secondaryConId]);
        con1Sep[eyeIndex]->addChild(conStick[secondaryConId]);

        //add ray for picking objects for each eye
        sGrp[eyeIndex]->addChild(mXRi->getRaySeparator());

        //add scene
        sGrp[eyeIndex]->addChild(wSep);

    });
    conMenuSep->addChild(mXRi->getMenuSeparator());//Menu container modified in XRInteraction
}

void CoinXRWidget::pollXrEvents()
{
    while (true) {
        xr::EventDataBuffer eventBuffer;
        auto pollResult = instance.pollEvent(eventBuffer);
        if (pollResult == xr::Result::EventUnavailable) {
            break;
        }

        switch (eventBuffer.type) {
        case xr::StructureType::EventDataSessionStateChanged:
            onSessionStateChanged(reinterpret_cast<xr::EventDataSessionStateChanged&>(eventBuffer));
            break;

        default:
            break;
        }
    }
}

void CoinXRWidget::onSessionStateChanged(const xr::EventDataSessionStateChanged& sessionStateChangedEvent)
{
    sessionState = sessionStateChangedEvent.state;
    switch (sessionState) {
    case xr::SessionState::Ready:
        if (!quit) {
            session.beginSession(xr::SessionBeginInfo{ xr::ViewConfigurationType::PrimaryStereo });
        }
        break;

    case xr::SessionState::Stopping:
        session.endSession();
        quit = true;
        break;

    default:
        break;
    }
}

bool CoinXRWidget::startXrFrame()
{
    switch (sessionState) {
    case xr::SessionState::Ready: //this is not necessary for Monado, but SteamVR would stuck at Ready state if xrBeginFrame wasn't called
    case xr::SessionState::Focused:
    case xr::SessionState::Synchronized:
    case xr::SessionState::Visible:
        session.waitFrame(xr::FrameWaitInfo{}, frameState);
        return xr::Result::Success == session.beginFrame(xr::FrameBeginInfo{});
    default:
        break;
    }

    return false;
}

void CoinXRWidget::updateXrViews()
{
    xr::ViewState vs;
    xr::ViewLocateInfo vi{ xr::ViewConfigurationType::PrimaryStereo, frameState.predictedDisplayTime, space };
    eyeViewStates = session.locateViews(vi, &(vs.operator XrViewState&()));
    xr::for_each_side_index([&](size_t eyeIndex) {
        const auto& viewState = eyeViewStates[eyeIndex];

        hmdrot = SbRotation(viewState.pose.orientation.x, viewState.pose.orientation.y, viewState.pose.orientation.z, viewState.pose.orientation.w);
        hmdpos = SbVec3f(viewState.pose.position.x, viewState.pose.position.y, viewState.pose.position.z); //get global position and orientation for both cameras

        //Base::Console().Warning("Hmd location: %f %f %f\n",
         //                       hmdpos[0], hmdpos[1], hmdpos[2]);

        SoTransform *camTransform = new SoTransform();
        camTransform->translation.setValue(worldTransform->translation.getValue()); //transfer values only
        camTransform->rotation.setValue(worldTransform->rotation.getValue());
        camTransform->center.setValue(worldTransform->center.getValue());

        SoTransform *hmdTransf = new SoTransform();
        hmdTransf->translation.setValue(hmdpos);
        hmdTransf->rotation.setValue(hmdrot);

        camTransform->combineLeft(hmdTransf); //combine real hmd and arificial (stick-driven) camera movement

        camera[eyeIndex]->orientation.setValue(camTransform->rotation.getValue());
        camera[eyeIndex]->position.setValue(camTransform->translation.getValue());

        float pfLeft = tan(viewState.fov.angleLeft);
        float pfRight = tan(viewState.fov.angleRight);
        float pfTop = tan(viewState.fov.angleUp);
        float pfBottom = tan(viewState.fov.angleDown);
        camera[eyeIndex]->aspectRatio.setValue((pfTop - pfBottom)/(pfRight - pfLeft));
        camera[eyeIndex]->nearDistance.setValue(nearPlane);
        camera[eyeIndex]->farDistance.setValue(farPlane);
        camera[eyeIndex]->left.setValue(nearPlane * pfLeft);
        camera[eyeIndex]->right.setValue(nearPlane * pfRight);
        camera[eyeIndex]->top.setValue(nearPlane * pfTop);
        camera[eyeIndex]->bottom.setValue(nearPlane * pfBottom);

    });
}

void CoinXRWidget::updateXrControls()
{
    userMovSpeed = mXRi->getMovementSpeed();
    userRotSpeed = mXRi->getRotationSpeed();
    controlScheme = mXRi->getControlScheme();

    const xr::ActiveActionSet activeActionSet {actionSet};

    xr::ActionsSyncInfo syncInfo {};
    syncInfo.type = xr::StructureType::ActionsSyncInfo;
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;

    session.syncActions(syncInfo);

    xr::SpaceLocation spaceLocation[hands]; //xrSpaceLocation contains "pose" field with position and orientation
    xr::ActionStateFloat xLeverValue[hands]; //analog stick
    xr::ActionStateFloat yLeverValue[hands];
    xr::ActionStateFloat grabValue[hands];

    SbRotation conrot[2];
    SbVec3f conpos[2];
    for (uint32_t i = 0; i < hands; i++)
    {
        xr::ActionStatePose poseState {};
        poseState.type = xr::StructureType::ActionStatePose;
        poseState.next = nullptr;

        xr::ActionStateGetInfo getInfo {};
        getInfo.type = xr::StructureType::ActionStateGetInfo;
        getInfo.next = nullptr;
        getInfo.action = poseAction;
        getInfo.subactionPath = handPaths[i];

        session.getActionStatePose(getInfo, poseState);

        spaceLocation[i].type = xr::StructureType::SpaceLocation;
        spaceLocation[i].next = nullptr;

        handSpaces[i].locateSpace(space, frameState.predictedDisplayTime, spaceLocation[i]);
        /*Base::Console().Warning("Hand pose %d: %f %f %f %f, %f %f %f\n", i,
                                spaceLocation[i].pose.orientation.x, spaceLocation[i].pose.orientation.y,
                                spaceLocation[i].pose.orientation.z, spaceLocation[i].pose.orientation.w,
                                spaceLocation[i].pose.position.x, spaceLocation[i].pose.position.y,
                                spaceLocation[i].pose.position.z);*/

        conrot[i] = SbRotation(spaceLocation[i].pose.orientation.x, spaceLocation[i].pose.orientation.y,
                               spaceLocation[i].pose.orientation.z, spaceLocation[i].pose.orientation.w);
        conpos[i] = SbVec3f(spaceLocation[i].pose.position.x, spaceLocation[i].pose.position.y,
                            spaceLocation[i].pose.position.z);



        SoTransform *conTransform = new SoTransform();
        conTransform->translation.setValue(worldTransform->translation.getValue());
        conTransform->rotation.setValue(worldTransform->rotation.getValue());
        conTransform->center.setValue(worldTransform->center.getValue());

        SoTransform *localConTransf = new SoTransform();
        localConTransf->translation.setValue(conpos[i]);
        localConTransf->rotation.setValue(conrot[i]);
        conTransform->combineLeft(localConTransf); //combine real hmd and arificial (stick-driven) movement

        conTrans[i]->translation.setValue(conTransform->translation.getValue());
        conRotat[i]->rotation.setValue(conTransform->rotation.getValue());


        //analog sticks
        getInfo.action = xLeverAction;
        session.getActionStateFloat(getInfo, xLeverValue[i]);

        getInfo.action = yLeverAction;
        session.getActionStateFloat(getInfo, yLeverValue[i]);


        /*Base::Console().Warning("Stick %d: Xval %f Yval %f \n", i,
                                xLeverValue[i].currentState, yLeverValue[i].currentState);*/

        //triggers
        getInfo.action = grabAction;
        session.getActionStateFloat(getInfo, grabValue[i]);

        /*Base::Console().Warning("Trigger %d: %f \n", i,
                                grabValue[i].currentState);*/

        //scene transformations
        float xaxis = xLeverValue[i].currentState;
        float yaxis = yLeverValue[i].currentState;
        currTriggerVal[i] = grabValue[i].currentState;

        SoRotationXYZ *xrot = new SoRotationXYZ;
        xrot->axis.setValue(SoRotationXYZ::Z); //X of a controller rotates around worls's Z
        xrot->angle.setValue(-xaxis);
        SoRotationXYZ *yrot = new SoRotationXYZ;
        yrot->axis.setValue(SoRotationXYZ::X); //Y of a controller rotates around worls's X
        yrot->angle.setValue(-yaxis);
        xrot->getRotation();
        stickRotat[i]->rotation.setValue(xrot->getRotation() * yrot->getRotation());

        float finalMovSpeed = movSpeed * userMovSpeed;

        if (controlScheme == 0)
        {
            float qx = localConTransf->rotation.getValue()[0];
            float qy = localConTransf->rotation.getValue()[1];
            float qz = localConTransf->rotation.getValue()[2];
            float qw = localConTransf->rotation.getValue()[3];

            //https://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
            float mat00 = 1-2*qy*qy-2*qz*qz;
            //float mat01 = 2*qx*qy-2*qz*qw;
            float mat02 = 2*qx*qz+2*qy*qw;
            float mat10 = 2*qx*qy+2*qz*qw;
            //float mat11 = 1-2*qx*qx-2*qz*qz;
            float mat12 = 2*qy*qz-2*qx*qw;
            float mat20 = 2*qx*qz-2*qy*qw;
            //float mat21 = 2*qy*qz+2*qx*qw;
            float mat22 = 1-2*qx*qx-2*qy*qy;

            if (i == primaryConId){
                SbVec3f step = SbVec3f(0.0f, 0.0f, 0.0f);
                float z0 = mat02;
                float z1 = mat12;
                float z2 = mat22;
                step = SbVec3f(-yaxis * z0 * finalMovSpeed, -yaxis * z1 * finalMovSpeed, -yaxis * z2 * finalMovSpeed);
                transfMod->translation.setValue(step);

                if (!mXRi->isMenuMode()) //do not allow user movement if menumode is active
                {
                    worldTransform->combineLeft(transfMod);
                }

            }
            if (i == secondaryConId){
                SbVec3f conXaxis = SbVec3f(mat00, mat10, mat20);
                SbVec3f conZaxis = SbVec3f(mat02, mat12, mat22);
                SbRotation conXrot = SbRotation(conXaxis, -yaxis); //stick moves world around one of controller axes
                SbRotation conZrot = SbRotation(conZaxis, -xaxis);
                SbRotation padrot = SbRotation();
                padrot = conXrot * conZrot;
                padrot.scaleAngle(0.5f * movSpeed * userRotSpeed);
                transfMod->center.setValue(conpos[i]);
                transfMod->rotation.setValue(padrot);
                if (!mXRi->isMenuMode())
                {
                    worldTransform->combineLeft(transfMod);
                }
            }
        }
        if (controlScheme == 1)
        {
            float qx = hmdrot[0];
            float qy = hmdrot[1];
            float qz = hmdrot[2];
            float qw = hmdrot[3];

            float mat02 = 2*qx*qz+2*qy*qw;
            float mat22 = 1-2*qx*qx-2*qy*qy;

            if (i == primaryConId){
                SbVec3f step = SbVec3f(0.0f, 0.0f, 0.0f);
                step = SbVec3f(0.0f, yaxis * finalMovSpeed, 0.0f);
                float z0 = mat02;
                float z2 = mat22;
                step = step + SbVec3f(xaxis * z2 * finalMovSpeed, 0.0f, -xaxis * z0 * finalMovSpeed);
                transfMod->center.setValue(hmdpos);
                transfMod->translation.setValue(step);
                if (!mXRi->isMenuMode()) //do not allow user movement if menumode is active
                {
                    worldTransform->combineLeft(transfMod);
                }


            }
            if (i == secondaryConId){
                SbVec3f step = SbVec3f(0.0f, 0.0f, 0.0f);
                float z0 = mat02;
                float z2 = mat22;
                step = step + SbVec3f(-yaxis * z0 * finalMovSpeed, 0.0f, -yaxis * z2 * finalMovSpeed);
                transfMod->center.setValue(hmdpos);
                transfMod->translation.setValue(step);
                SbRotation worldZrot = SbRotation(SbVec3f(0.0f, 1.0f,  0.0f), -xaxis);
                worldZrot.scaleAngle(0.5f * movSpeed * userRotSpeed);
                transfMod->rotation.setValue(worldZrot);
                if (!mXRi->isMenuMode())
                {
                    worldTransform->combineLeft(transfMod);
                }

            }

        }
        float gqx = conRotat[secondaryConId]->rotation.getValue()[0];
        float gqy = conRotat[secondaryConId]->rotation.getValue()[1];
        float gqz = conRotat[secondaryConId]->rotation.getValue()[2];
        float gqw = conRotat[secondaryConId]->rotation.getValue()[3];

        float gmat02 = 2*gqx*gqz+2*gqy*gqw;
        float gmat12 = 2*gqy*gqz-2*gqx*gqw;
        float gmat22 = 1-2*gqx*gqx-2*gqy*gqy;

        rayAxis = SbVec3f(gmat02, gmat12, gmat22);

        //XRInteraction
        mXRi->setControllerState(i, conTrans[i], conRotat[i], currTriggerVal[i], xaxis, yaxis);
    }

    for (uint32_t i = 0; i < hands; i++)
    {
        oldTriggerVal[i] = currTriggerVal[i];
    }
}

void CoinXRWidget::updateXrGui()
{
    //picking ray
    SbVec3f startVec = conTrans[secondaryConId]->translation.getValue();
    SbVec3f endVec = conTrans[secondaryConId]->translation.getValue() - rayAxis;


    if (currTriggerVal[secondaryConId] >= 0.5f) //do traversal only if trigger pressed, because it is expensive
    {

        mXRi->findPickedObject(wSep, vpReg,
                                    startVec, endVec, rayAxis,
                                    nearPlane, farPlane,
                                    isNewPickedPoint, pickedPCoords);

    }
    if (currTriggerVal[secondaryConId] < 0.5f && isNewPickedPoint == true) //teleport when trigger is released
    {
        isNewPickedPoint = false;
        SoTransform *teleportTransf = new SoTransform;
        teleportTransf->translation.setValue(pickedPCoords - camera[0]->position.getValue() + SbVec3f(0.0f, hmdpos[1], 0.0f));
        worldTransform->combineRight(teleportTransf);
    }

    //prepare and execute commands
    mXRi->applyInput(primaryConId);


}

void CoinXRWidget::endXrFrame()
{
    xr::FrameEndInfo frameEndInfo{ frameState.predictedDisplayTime, xr::EnvironmentBlendMode::Opaque };
    if (frameState.shouldRender) {
        xr::for_each_side_index([&](uint32_t eyeIndex) {
            auto& layerView = projectionLayerViews[eyeIndex];
            const auto& eyeView = eyeViewStates[eyeIndex];
            layerView.fov = eyeView.fov;
            layerView.pose = eyeView.pose;

        });
        frameEndInfo.layerCount = static_cast<uint32_t>(layersPointers.size());
        frameEndInfo.layers = layersPointers.data();
    }
    session.endFrame(frameEndInfo);
}

void CoinXRWidget::quitRendering()
{
    quit = true;
    Base::Console().Warning("XR rendering stopped \n");
}

#endif //BUILD_OPENXR
