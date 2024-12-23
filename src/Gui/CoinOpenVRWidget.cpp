/**************************************************************************\
* Copyright (c) 2019 Adrian Przekwas <adrian.v.przekwas@gmail.com>        *
* Copyright (c) Bastiaan Veelo <Bastiaan a_t Veelo d_o_t net>             *
*            &  Juergen Riegel <FreeCAD@juergen-riegel.net>               *
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


#include "CoinOpenVRWidget.h"

#include <Base/Console.h>

#include <Inventor/nodes/SoBaseColor.h>

#if BUILD_OPENVR

#undef max

CoinOpenVRWidget::CoinOpenVRWidget() : QOpenGLWidget()
{

    QSurfaceFormat format;
    format = this->format();
    oldFormat = format;
    format.setSwapInterval(0); //Disable vsync, otherwise Qt would sync HMD with flat screen which is a lot slower in most cases
    format.setSamples(8);
    this->format().setDefaultFormat(format);

    movSpeed = 0.0f; //speed of movement when analog input (stick, trackpad) is used
    scaleMod = 1.0f;

    eTimer.start();

    eyes[0] = vr::Eye_Left;
    eyes[1] = vr::Eye_Right;
    for (int eye = 0; eye < 2; eye++) {
        frameBufferID[eye] = 0;
        depthBufferID[eye] = 0;

    }
    // Loading the SteamVR Runtime
    vr::EVRInitError eError = vr::VRInitError_None;
    m_pHMD = vr::VR_Init( &eError, vr::VRApplication_Scene );

    if (eError != vr::VRInitError_None){
        m_pHMD = nullptr;
        qDebug() << "Unable to init VR runtime:" << vr::VR_GetVRInitErrorAsEnglishDescription( eError );
        throw std::runtime_error("Unable to init VR runtime");
    }
    float dispFreq = m_pHMD->GetFloatTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float );
    // Configure stereo settings.
    uint32_t flatScale = 2;
    m_pHMD->GetRecommendedRenderTargetSize( &m_nRenderWidth, &m_nRenderHeight );
    m_nScreenWidth = m_nRenderWidth / flatScale;
    m_nScreenHeight = m_nRenderHeight / flatScale;
    resize(static_cast<int>(m_nScreenWidth), static_cast<int>(m_nScreenHeight));

    Base::Console().Warning("Frequency: %f Width: %d Height: %d \n", dispFreq, m_nRenderWidth, m_nRenderHeight);

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

    for (int eye = 0; eye < 2; eye++) {
        frameBufferID[eye] = 0;
        depthBufferID[eye] = 0;
    }

    for (int id = 0; id < 2; id++) { //drawing controllers gizmos
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
    worldTransform = new SoTransform(); //use for navigation in the world
    transfMod = new SoTransform(); //modifier of transformation

    for (int eye = 0; eye < 2; eye++) {
        eyeHead[eye] = m_pHMD->GetEyeToHeadTransform(eyes[eye]);
        camTrans[eye] = new SoTranslation();
        camTrans[eye]->translation.setValue(eyeHead->m[0][3], 0, 0); //0 3 is eye to center
        rootScene[eye] = new SoSeparator();
        cGrp[eye] = new SoGroup();
        sGrp[eye] = new SoGroup();
        rootScene[eye]->ref();
        camera[eye] = new SoFrustumCamera();
        camera[eye]->position.setValue(basePosition);
        camera[eye]->focalDistance.setValue(5.0f);
        camera[eye]->viewportMapping.setValue(SoCamera::LEAVE_ALONE);

        rootScene[eye]->addChild(cGrp[eye]);
        cGrp[eye]->addChild(camTrans[eye]);
        cGrp[eye]->addChild(camera[eye]);
        rootScene[eye]->addChild(sGrp[eye]);
        sGrp[eye]->addChild(light);
        sGrp[eye]->addChild(light2);

        //controllers
        con0Sep[eye] = new SoSeparator();
        con1Sep[eye] = new SoSeparator();
        sGrp[eye]->addChild(con0Sep[eye]);
        con0Sep[eye]->addChild(conTrans[0]);
        con0Sep[eye]->addChild(conRotat[0]);
        con0Sep[eye]->addChild(conGizmo[0]);
        con0Sep[eye]->addChild(stickRotat[0]);
        con0Sep[eye]->addChild(conStick[0]);
        sGrp[eye]->addChild(con1Sep[eye]);
        con1Sep[eye]->addChild(conTrans[1]);
        con1Sep[eye]->addChild(conRotat[1]);
        con1Sep[eye]->addChild(conGizmo[1]);
        con1Sep[eye]->addChild(stickRotat[1]);
        con1Sep[eye]->addChild(conStick[1]);

        //add scene
        sGrp[eye]->addChild(worldTransform);
        sGrp[eye]->addChild(scene);


    }

    static const float nearPlane = 0.01f;
    static const float farPlane = 10000.0f;

    for (int eye = 0; eye < 2; eye++) {
        float pfLeft, pfRight, pfTop, pfBottom;
        m_pHMD->GetProjectionRaw(eyes[eye], &pfLeft, &pfRight, &pfBottom, &pfTop); //top and bottom are reversed
        camera[eye]->aspectRatio.setValue((pfTop - pfBottom)/(pfRight - pfLeft));
        camera[eye]->nearDistance.setValue(nearPlane);
        camera[eye]->farDistance.setValue(farPlane);
        camera[eye]->left.setValue(nearPlane * pfLeft);
        camera[eye]->right.setValue(nearPlane * pfRight);
        camera[eye]->top.setValue(nearPlane * pfTop);
        camera[eye]->bottom.setValue(nearPlane * pfBottom);
    }

    for (int contr = 0; contr < 2; contr++)
    {
        currTriggerVal[contr] = 0.0f;
        oldTriggerVal[contr] = 0.0f;
        oldConPos[contr] = SbVec3f(0.0f, 0.0f, 0.0f);
    }
}


CoinOpenVRWidget::~CoinOpenVRWidget()
{
    makeCurrent();
    for (int eye = 0; eye < 2; eye++) {
            rootScene[eye]->unref();
            if (frameBufferID[eye] != 0) {
                glDeleteFramebuffers(1, &frameBufferID[eye]);
                frameBufferID[eye] = 0;
            }
            if (depthBufferID[eye] != 0) {
                glDeleteRenderbuffers(1, &depthBufferID[eye]);
                depthBufferID[eye] = 0;
            }
        }
    scene = nullptr;
    if(m_pHMD){
        vr::VR_Shutdown();
        m_pHMD = nullptr;
    }
    delete m_sceneManager;
    format().setDefaultFormat(oldFormat);
    doneCurrent();
    Base::Console().Warning("OpenVR session closed \n");
}

void CoinOpenVRWidget::setBackgroundColor(const SbColor &Col)
{
    BackgroundColor = Col;
}

void CoinOpenVRWidget::setSceneGraph(SoNode *sceneGraph)
{
    sGrp[0]->replaceChild(scene, sceneGraph);
    sGrp[1]->replaceChild(scene, sceneGraph);
    scene = sceneGraph;
}

void CoinOpenVRWidget::resizeGL(int width, int height)
{
    int side = qMin(width, height);
    glViewport((width - side) / 2, (height - side) / 2, side, side);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, 0.0, 1000.0);
    glMatrixMode(GL_MODELVIEW);
}

void CoinOpenVRWidget::initializeGL()
{
        initializeOpenGLFunctions();
        glEnable(GL_MULTISAMPLE);
        // Store old framebuffer.
        GLint oldfb;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &oldfb);
        // Create rendering target textures.
        glEnable(GL_TEXTURE_2D);
        for (int eye = 0; eye < 2; eye++) {
            glGenFramebuffers(1, &frameBufferID[eye]);
            glBindFramebuffer(GL_FRAMEBUFFER_EXT, frameBufferID[eye]);
            // Create the render buffer.
            glGenRenderbuffers(1, &depthBufferID[eye]);
            glBindRenderbuffer(GL_RENDERBUFFER_EXT, depthBufferID[eye]);
            glRenderbufferStorage(GL_RENDERBUFFER_EXT,
                                  GL_DEPTH24_STENCIL8,
                                  static_cast<GLsizei>(m_nRenderWidth),
                                  static_cast<GLsizei>(m_nRenderHeight));
            // Attach renderbuffer to framebuffer.
            glFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT,
                                      GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER_EXT,
                                      depthBufferID[eye]);
            glGenTextures(1, &textureIds[eye] );
            glBindTexture( GL_TEXTURE_2D, textureIds[eye] );
            Q_ASSERT(!glGetError());
            // Allocate storage for the texture.
            glTexImage2D(
              GL_TEXTURE_2D, 0, GL_RGBA8,static_cast<GLsizei>(m_nRenderWidth), static_cast<GLsizei>(m_nRenderHeight), 0, GL_RGBA, GL_UNSIGNED_BYTE,
              nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            Q_ASSERT(!glGetError());
            // Attach texture to framebuffer color object.
            glFramebufferTexture2D(
              GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
              textureIds[eye], 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
                    GL_FRAMEBUFFER_COMPLETE)
                qDebug() << "ERROR: FrameBuffer is not operational!";
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, static_cast<GLuint>(oldfb));
        doneCurrent();
}

void CoinOpenVRWidget::paintGL()
{

    if ( !m_pHMD )
        return;

    makeCurrent();

    glEnable(GL_TEXTURE_2D);

    if ( m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
    {
        vr::TrackedDevicePose_t headPose = m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd];
        headToWorld = headPose.mDeviceToAbsoluteTracking;
    }

    SbRotation hmdrot = extractRotation(headToWorld);
    SbVec3f hmdpos = extractTranslation(headToWorld);

    //Searching for controllers
    int ccnt = -1;
    //Base::Console().Warning("Tracket devices count: %d \n",vr::k_unMaxTrackedDeviceCount);
    for (unsigned int id = 0; id < vr::k_unMaxTrackedDeviceCount; id++)  {
        vr::ETrackedDeviceClass trackedDeviceClass = m_pHMD->GetTrackedDeviceClass(id);
        if (trackedDeviceClass != vr::ETrackedDeviceClass::TrackedDeviceClass_Controller || !m_pHMD->IsTrackedDeviceConnected(id))
            continue;

        vr::TrackedDevicePose_t trackedDevicePose;
        vr::VRControllerState_t controllerState;
        m_pHMD->GetControllerStateWithPose(vr::TrackingUniverseStanding, id, &controllerState, sizeof(controllerState), &trackedDevicePose);
        ccnt++;

        vr::HmdMatrix34_t controllerPos;
        //Base::Console().Warning("Controller id: %d \n",id);
        if (ccnt !=-1 && ccnt < 2) { // Only first 2 controllers
            controllerPos = trackedDevicePose.mDeviceToAbsoluteTracking;
            SbRotation conrot = extractRotation(controllerPos);
            SbVec3f conpos = extractTranslation(controllerPos);
            conTrans[ccnt]->translation.setValue(conpos);
            conRotat[ccnt]->rotation.setValue(conrot);
            //read axes
            uint32_t idpad = 0;
            uint32_t idtrigger = 0;
            for(uint32_t x=0; x<vr::k_unControllerStateAxisCount; x++ ){
                int prop = m_pHMD->GetInt32TrackedDeviceProperty(id, static_cast<vr::ETrackedDeviceProperty>(vr::ETrackedDeviceProperty::Prop_Axis0Type_Int32 + x));
                if( prop==vr::k_eControllerAxis_Trigger ){
                   if (!idtrigger) //stop search if the first trigger has been found. This is important for Valve Index controllers reporting multiple triggers
                   {
                       idtrigger = x;
                   }
                }
                else if( prop==vr::k_eControllerAxis_TrackPad ){
                   idpad = x;
                }
            }

            float xaxis = controllerState.rAxis[idpad].x;
            float yaxis = controllerState.rAxis[idpad].y;
            currTriggerVal[ccnt] = controllerState.rAxis[idtrigger].x;

            SoRotationXYZ *xrot = new SoRotationXYZ;
            xrot->axis.setValue(SoRotationXYZ::Z); //X of a controller rotates around worls's Z
            xrot->angle.setValue(-xaxis);
            SoRotationXYZ *yrot = new SoRotationXYZ;
            yrot->axis.setValue(SoRotationXYZ::X); //Y of a controller rotates around worls's X
            yrot->angle.setValue(-yaxis);
            xrot->getRotation();
            stickRotat[ccnt]->rotation.setValue(xrot->getRotation() * yrot->getRotation());
            vr::HmdMatrix34_t tm = controllerPos;
            if (ccnt == 0){
                SbVec3f step = SbVec3f(0.0f, 0.0f, 0.0f);
                float z0 = tm.m[0][2];
                float z1 = tm.m[1][2];
                float z2 = tm.m[2][2];
                step = SbVec3f(yaxis * z0 * movSpeed, yaxis * z1 * movSpeed, yaxis * z2 * movSpeed);
                worldTransform->translation.setValue(worldTransform->translation.getValue() + step);
                //Base::Console().Warning("Controller rotation: %f %f %f  \n", z0, z1, z2);

            }
            if (ccnt == 1){
                    transfMod->center.setValue(conpos);
                    SbVec3f conXaxis = SbVec3f(tm.m[0][0], tm.m[1][0], tm.m[2][0]);
                    SbVec3f conZaxis = SbVec3f(tm.m[0][2], tm.m[1][2], tm.m[2][2]);
                    SbRotation conXrot = SbRotation(conXaxis, yaxis); //stick moves world around one of controller axes
                    SbRotation conZrot = SbRotation(conZaxis, xaxis);
                    SbRotation padrot = SbRotation();
                    padrot = conXrot * conZrot;
                    padrot.scaleAngle(0.5f * movSpeed);
                    transfMod->rotation.setValue(padrot);
                    worldTransform->combineRight(transfMod);
            }
        } 
        float tresh = 0.3f;
        if (oldTriggerVal[0] > tresh && currTriggerVal[0] > tresh && oldTriggerVal[1] > tresh && currTriggerVal[1] > tresh) //use both controllers to change world scale
        {
            SbVec3f diff = conTrans[1]->translation.getValue() - conTrans[0]->translation.getValue();
            SbVec3f olddiff = oldConPos[1] - oldConPos[0];
            float dlen = diff.length();
            float doldlen = olddiff.length();
            if (dlen > 0.0f && doldlen > 0.0f){
                scaleMod = scaleMod * dlen / doldlen;
                worldTransform->scaleFactor.setValue(SbVec3f(1.0f, 1.0f, 1.0f) * scaleMod);
            }
        }
    }
    for (int contr = 0; contr < 2; contr++){
        oldTriggerVal[contr] = currTriggerVal[contr];
        oldConPos[contr] = conTrans[contr]->translation.getValue();
    }

    for (int eye = 0; eye < 2; eye++) {
        camera[eye]->orientation.setValue(hmdrot);
        camera[eye]->position.setValue(basePosition + hmdpos);
        // Clear state pollution from OpenVR SDK.
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        GLint oldfb;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &oldfb);
        // Set up framebuffer for rendering.
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, frameBufferID[eye]);
        m_sceneManager->setSceneGraph(rootScene[eye]);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        m_sceneManager->render();
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glClearDepth(1.0);

        textures[eye] = { reinterpret_cast<void*>(textureIds[eye]), vr::TextureType_OpenGL, vr::ColorSpace_Gamma };

        if (eye == 0){
            //copy to screen
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldfb));
            glBlitFramebuffer(
            0, 0, static_cast<int>(m_nRenderWidth), static_cast<int>(m_nRenderHeight),
            0, 0, static_cast<int>(m_nScreenWidth), static_cast<int>(m_nScreenHeight),
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        // Continue rendering to the original frame buffer (likely 0, the onscreen buffer).
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, static_cast<GLuint>(oldfb));
        Q_ASSERT(!glGetError());
    }
    update(); //schedule QOpenGL composition

    //submit textures to compositor
    vr::VRCompositor()->Submit(vr::Eye_Left, &textures[0] );
    vr::VRCompositor()->Submit(vr::Eye_Right, &textures[1] );

    //getting HMD position, waitgetposes also waits 3ms before vsync
    vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0 );

    doneCurrent();

    qint64 et = eTimer.nsecsElapsed();
    eTimer.restart();

    movSpeed = et * 0.000000005f;
    if (et > 0)
    {
        qint64 frt = 1000000000 / et;
        this->setWindowTitle(QString::fromStdString("FreeCAD OpenVR - Framerate: ") + QString::number(frt) + QString::fromStdString(" FPS"));
    }
}

SbRotation CoinOpenVRWidget::extractRotation(vr::HmdMatrix34_t tmat)
{
    float qw = sqrt(fmax(0.0f, 1.0f + tmat.m[0][0] + tmat.m[1][1] + tmat.m[2][2])) / 2.0f;
    float qx = sqrt(fmax(0.0f, 1.0f + tmat.m[0][0] - tmat.m[1][1] - tmat.m[2][2])) / 2.0f;
    float qy = sqrt(fmax(0.0f, 1.0f - tmat.m[0][0] + tmat.m[1][1] - tmat.m[2][2])) / 2.0f;
    float qz = sqrt(fmax(0.0f, 1.0f - tmat.m[0][0] - tmat.m[1][1] + tmat.m[2][2])) / 2.0f;
    qx = copysign(qx, tmat.m[2][1] - tmat.m[1][2]);
    qy = copysign(qy, tmat.m[0][2] - tmat.m[2][0]);
    qz = copysign(qz, tmat.m[1][0] - tmat.m[0][1]);
    SbRotation hmdrot = SbRotation(qx, qy, qz, qw);
    return hmdrot;
}

SbVec3f CoinOpenVRWidget::extractTranslation(vr::HmdMatrix34_t tmat)
{
    SbVec3f hmdpos = SbVec3f(tmat.m[0][3], tmat.m[1][3], tmat.m[2][3]);
    return hmdpos;
}



#endif //BUILD_OPENVR
