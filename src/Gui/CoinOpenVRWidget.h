/**************************************************************************\
* Copyright (c) 2019 Adrian Przekwas <adrian.v.przekwas@gmail.com>
* Copyright (c) Bastiaan Veelo (Bastiaan a_t Veelo d_o_t net)
*               & Juergen Riegel (FreeCAD@juergen-riegel.net)
* All rights reserved. Contact me if the below is too restrictive for you.
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
#ifndef GUI_CoinOpenVRWidget
#define GUI_CoinOpenVRWidget

#if BUILD_OPENVR

#include <algorithm>
#include <QApplication>
#include <QOpenGLWidget>
#include <QTimer>
#include <QDebug>
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
#include <Inventor/nodes/SoCone.h>

#include <openvr/openvr.h>
#include <QOpenGLFunctions_3_3_Core> //for glGenFramebuffers

#include <iostream>


//QOpenGLWidget is a modern replacement for QGLWidget, it performs offscreen rendering
class CoinOpenVRWidget : public QOpenGLWidget , protected QOpenGLFunctions_3_3_Core
{
    // A SoSceneManager has a SoRenderManager to do the rendering -- should we not use SoRenderManager instead?
    // We are probably not that interested in events. SoSceneManager::setSceneGraph() searches for the camera
    // and sets it in SoRenderManager, but its is actually only used for built-in stereo rendering.
    // FIXME: We should probably eliminate that search...
    SoSceneManager *m_sceneManager;
    SoSeparator *rootScene[2];
    SoFrustumCamera *camera[2];
    SoNode *scene;
    SoTranslation *camtrans[2];
    SoGroup *cgrp[2];
    SoGroup *sgrp[2];
    SoSeparator *con0sep[2];
    SoSeparator *con1sep[2];
    SoCone *conGizmo[2];
    SoTranslation *contrans[2];
    SoRotation *conrotat[2];

    GLuint frameBufferID[2], depthBufferID[2];
    vr::IVRSystem *m_pHMD;
    uint32_t m_nRenderWidth;
    uint32_t m_nRenderHeight;
    vr::Texture_t textures[2];
    vr::Hmd_Eye eyes[2];
    vr::HmdMatrix34_t eyehead[2];
    vr::TrackedDevicePose_t m_rTrackedDevicePose[ vr::k_unMaxTrackedDeviceCount ];
    GLuint texture_ids[2];
    vr::HmdMatrix34_t headToWorld;

    SbRotation extractRotation(vr::HmdMatrix34_t tmat);
    SbVec3f extractTranslation(vr::HmdMatrix34_t tmat);

public:
    explicit CoinOpenVRWidget();
    ~CoinOpenVRWidget();
    virtual void setSceneGraph(SoNode *sceneGraph);
    void setBase(const SbVec3f &pos){basePosition=pos;}
    void setBackgroundColor(const SbColor &Col);

    SbVec3f    basePosition;
    SbRotation baseOrientation;

protected:
    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);

    SbColor BackgroundColor;
    SoTranslation *lightTranslation;

};

#endif //BUILD_VR

#endif // GUI_CoinOpenVRWidget
