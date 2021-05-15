/**************************************************************************\
* Copyright (c) 2021 Adrian Przekwas <adrian.v.przekwas@gmail.com>
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

#ifndef GUI_XRInteraction
#define GUI_XRInteraction

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoScale.h>

#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/nodes/SoBaseColor.h>

#include "Document.h"
#include "Base/Exception.h"
#include <Base/Console.h>
#include <Command.h>
#include "Application.h"

class XRInteraction
{
public:
    explicit XRInteraction();
    ~XRInteraction();

    void applyInput();
    void setControllerState(uint32_t id, const SoTranslation *st, const SoRotation *sr, float tv);
    SoSeparator * getMenuSeparator();
    SoSeparator * getRaySeparator();
    const SoPickedPoint * findPickedObject(SoSeparator *sep, SbViewportRegion vpReg,
                          SbVec3f startVec, SbVec3f endVec, SbVec3f rayAxis,
                          float nearPlane, float farPlane);

private:

    static const uint32_t hands = 2;
    float currTriggerVal[hands];
    float oldTriggerVal[hands];
    SbVec3f conTransVec[hands];
    SbRotation conRotatQuat[hands];

    QString cmd;
    uint32_t objCount = 0;
    App::Document* doc;

    SoSeparator *rSep;
    SoVertexProperty *rayVtxs;
    SoLineSet *rayLine;
    SbVec3f rayAxis;
    SoSphere *raySph;
    SoTranslation *sphTrans;

    SoSeparator *menuSep;
    SoText3 *menuText;
    SoCube *menuCube;
};

#endif // GUI_XRInteraction
