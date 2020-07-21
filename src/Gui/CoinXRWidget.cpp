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

#include "PreCompiled.h"
#include "CoinXRWidget.h"

#include "Base/Exception.h"
#include <Base/Console.h>

#if BUILD_OPENXR

#undef max

CoinXRWidget::CoinXRWidget() : QOpenGLWidget()
{
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
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);

    format.setSamples(8);
    this->format().setDefaultFormat(format);

    prepareXrInstance();
    prepareXrSystem();
    m_nScreenWidth = m_nRenderWidth / flatScale;
    m_nScreenHeight = m_nRenderHeight / flatScale;
    resize(static_cast<int>(m_nScreenWidth), static_cast<int>(m_nScreenHeight));
    prepareXrSession();
    prepareXrSwapchain();
    prepareXrCompositionLayers();
    prepareScene();

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

    for (xr::Swapchain swapchain : swapchains) {
        if (swapchain)
        {
            swapchain.destroy();
            swapchain = nullptr;
        }
    }

    if (session) {
        session.destroy();
        session = nullptr;
    }

    if (instance) {
        instance.destroy();
        instance = nullptr;
    }
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
    sGrp[0]->replaceChild(scene, sceneGraph);
    sGrp[1]->replaceChild(scene, sceneGraph);
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
    makeCurrent();

    if (startXrFrame()) {
        updateXrViews();
        if (frameState.shouldRender) {
            xr::for_each_side_index([&](uint32_t eyeIndex){
                // Clear state pollution
                glUseProgram(0);
                GLint oldfb;
                glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldfb);

                uint32_t swapchainIndex;

                xr::Swapchain swapchain = swapchains[eyeIndex];

                swapchain.acquireSwapchainImage(xr::SwapchainImageAcquireInfo{}, &swapchainIndex);
                swapchain.waitSwapchainImage(xr::SwapchainImageWaitInfo{ xr::Duration::infinite() });

                glBindFramebuffer(GL_FRAMEBUFFER, fbo.id);

                glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, swapchainImagesArr[eyeIndex][swapchainIndex].image, 0);

                // "render" to the swapchain image
                m_sceneManager->setSceneGraph(rootScene[eyeIndex]);
                glEnable(GL_CULL_FACE);
                glEnable(GL_DEPTH_TEST);
                m_sceneManager->render();
                glDisable(GL_CULL_FACE);
                glDisable(GL_DEPTH_TEST);
                glClearDepth(1.0);

                if (eyeIndex == 0){
                    //copy to screen
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldfb));
                    glBlitFramebuffer(
                    0, 0, static_cast<int>(m_nRenderWidth), static_cast<int>(m_nRenderHeight),
                    0, 0, static_cast<int>(m_nScreenWidth), static_cast<int>(m_nScreenHeight),
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);

                }
                // Continue rendering to the original frame buffer (likely 0, the onscreen buffer).
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldfb));
                Q_ASSERT(!glGetError());
                swapchain.releaseSwapchainImage(xr::SwapchainImageReleaseInfo{});
            });
            update(); //schedule QOpenGL composition
            doneCurrent();
        }
        endXrFrame();
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
                                { "gl_single_file_example", 0, "openXrSamples", 0, xr::Version::current() },
                                0,
                                nullptr,
                                static_cast<uint32_t>(requestedExtensions.size()),
                                requestedExtensions.data() };

    // Create the actual instance
    instance = xr::createInstance(ici);

    // Having created the isntance, the very first thing to do is populate the dynamic dispatch, loading
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

    m_nRenderWidth = viewConfigViews[0].recommendedImageRectWidth; //renderTargetSize = { viewConfigViews[0].recommendedImageRectWidth, viewConfigViews[0].recommendedImageRectHeight };
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
        Display* xDisplay = XOpenDisplay(nullptr);
        int xScreen = XDefaultScreen(xDisplay);
        GLXContext glxContext = glXGetCurrentContext();
        GLXDrawable glxDrawable = glXGetCurrentDrawable();

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
        space = session.createReferenceSpace(xr::ReferenceSpaceCreateInfo{ xr::ReferenceSpaceType::Local });

        auto swapchainFormats = session.enumerateSwapchainFormats();
        doneCurrent();
}
void CoinXRWidget::prepareXrSwapchain()
{
    xr::for_each_side_index([&](uint32_t eyeIndex){
        xr::Swapchain swapchain;
        std::vector<xr::SwapchainImageOpenGLKHR> swapchainImages;

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

        swapchains.push_back(swapchain);
        swapchainImagesArr.push_back(swapchainImages);
    });
}
void CoinXRWidget::prepareXrCompositionLayers()
{
    projectionLayer.viewCount = 2;
    projectionLayer.views = projectionLayerViews.data();
    layersPointers.push_back(&projectionLayer);
    // Finish setting up the layer submission
    xr::for_each_side_index([&](uint32_t eyeIndex) {
        auto& layerView = projectionLayerViews[eyeIndex];
        layerView.subImage.swapchain = swapchains[eyeIndex];
        layerView.subImage.imageRect.offset = {0, 0};
        layerView.subImage.imageRect.extent = { static_cast<int32_t>(m_nRenderWidth), static_cast<int32_t>(m_nRenderHeight) };
    });
}

void CoinXRWidget::prepareControls()
{
/*
    xr::ActionSetCreateInfo actionSetInfo{ };
    actionSet = instance.createActionSet(actionSetInfo);

    xr::DispatchLoaderDynamic dispatch{ instance }; //exposes all the core functionality and all the known extensions

    const int hands = 2;
    xr::Path handPaths[hands]; //avoid duplication af actions for both hands
    dispatch.xrStringToPath(instance, "/user/hand/left", handPaths[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right", handPaths[1].put());

    xr::ActionCreateInfo actionInfo { };
    actionInfo.next = nullptr;
    actionInfo.actionType = xr::ActionType::PoseInput; //XR_ACTION_TYPE_POSE_INPUT
    strcpy(actionInfo.actionName, "hand_pose");
    strcpy(actionInfo.localizedActionName, "Hand Pose");
    actionInfo.countSubactionPaths = hands;
    actionInfo.subactionPaths = handPaths;

    xr::Action poseAction;
    poseAction = actionSet.createAction(actionInfo);
    xr::Path posePaths[hands];
    dispatch.xrStringToPath(instance, "/user/hand/left/input/grip/pose", posePaths[0].put());
    dispatch.xrStringToPath(instance, "/user/hand/right/input/grip/pose", posePaths[1].put());

    xr::Path interactionProfilePath;
    dispatch.xrStringToPath(instance, "/interaction_profiles/khr/simple_controller", interactionProfilePath.put());
    const xr::ActionSuggestedBinding bindings[2];
    bindings[0].action. = poseAction;
    bindings[0].binding = posePaths[0];

    const XrActionSuggestedBinding bindings[] = {
        {
            .action = poseAction,
            .binding = posePaths[0].get()
        },
        {
            .action = poseAction,
            .binding = posePaths[1].get()
        },
    };

*/

}


void CoinXRWidget::prepareScene()
{
    m_sceneManager = new SoSceneManager();
    m_sceneManager->setViewportRegion(SbViewportRegion(static_cast<short>(m_nRenderWidth), static_cast<short>(m_nRenderHeight)));
    m_sceneManager->setBackgroundColor(SbColor(.0f, .0f, .8f));

    basePosition = SbVec3f(0.0f, 0.0f, 2.0f);

    // light handling
    SoDirectionalLight *light = new SoDirectionalLight();
    light->direction.setValue(1,-1,-1);
    SoDirectionalLight *light2 = new SoDirectionalLight();
    light2->direction.setValue(-1,-1,-1);
    light2->intensity.setValue(0.6f);
    light2->color.setValue(0.8f,0.8f,1.0f);

    scene = new SoSeparator(0); // Placeholder.

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
        //cGrp[eye]->addChild(camTrans[eye]);
        cGrp[eyeIndex]->addChild(camera[eyeIndex]);

        rootScene[eyeIndex]->addChild(sGrp[eyeIndex]);
        sGrp[eyeIndex]->addChild(light);
        sGrp[eyeIndex]->addChild(light2);


        //add scene
        //sGrp[eyeIndex]->addChild(worldTransform);
        sGrp[eyeIndex]->addChild(scene);

    });
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
        case xr::SessionState::Focused:
        case xr::SessionState::Synchronized:
        case xr::SessionState::Visible:
            session.waitFrame(xr::FrameWaitInfo{}, frameState);
            return xr::Result::Success == session.beginFrame(xr::FrameBeginInfo{});
        case xr::SessionState::Ready: //this is not necessary for Monado, but SteamVR would stuck at Ready state if xrBeginFrame wasn't called
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

        SbRotation hmdrot = SbRotation(viewState.pose.orientation.x, viewState.pose.orientation.y, viewState.pose.orientation.z, viewState.pose.orientation.w);
        SbVec3f hmdpos = SbVec3f(viewState.pose.position.x, viewState.pose.position.y, viewState.pose.position.z); //get global position and orientation for both cameras
        camera[eyeIndex]->orientation.setValue(hmdrot);
        camera[eyeIndex]->position.setValue(basePosition + hmdpos);

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
