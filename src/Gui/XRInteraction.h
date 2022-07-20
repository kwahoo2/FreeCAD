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
#include <Inventor/nodes/SoPointLight.h>

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

    bool isMenuMode();
    void setPriAndSecController (uint32_t pri, uint32_t sec);
    void applyInput(uint32_t conId);
    void setControllerState(uint32_t id, const SoTranslation *st, const SoRotation *sr, float tv, float xv, float yv);
    float getMovementSpeed();
    float getRotationSpeed();
    int32_t getControlScheme();

    SoSeparator * getMenuSeparator();
    SoSeparator * getRaySeparator();
    const SoPickedPoint * findPickedObject(SoSeparator *sep, SbViewportRegion vpReg,
                          SbVec3f startVec, SbVec3f endVec, SbVec3f rayAxis,
                          float nearPlane, float farPlane,
                          bool &isNewPickedPoint, SbVec3f &ppCoords);


private:

    void changeMovementSpeed (float xv);
    void changeRotationSpeed (float xv);
    void toggleMenuMode();
    void toggleMenuBar2(uint32_t conId);
    void updateMenuBar2();

    static const uint32_t hands = 2;
    float currTriggerVal[hands];
    float oldTriggerVal[hands];
    float currXStickVal[hands];
    float currYStickVal[hands];
    float oldXStickVal[hands];
    float oldYStickVal[hands];
    float movementSpeed = 1.0f;
    float rotationSpeed = 1.0f;
    SbVec3f conTransVec[hands];
    SbRotation conRotatQuat[hands];
    uint32_t primaryConId = 0;
    uint32_t secondaryConId = 1;
    SbColor activatedColor = SbColor (0.0f, 1.0f, 0.0f);
    SbColor deactivatedColor = SbColor (0.5f, 0.2f ,0.2f);
    int32_t selectedMenuItem = 0;
    int32_t selectedControlScheme = 0;
    int32_t numberOfMenuItems = 2;

    QString cmd;
    uint32_t objCount = 0;
    //App::Document* doc;
    bool menuMode = 0;

    SoSeparator *rSep;
    SoVertexProperty *rayVtxs;
    SoLineSet *rayLine;
    SbVec3f rayAxis;
    SoSphere *raySph;
    SoTranslation *sphTrans;

    SoSeparator *menuSep;
    SoText3 *menuText;
    SoText3 *menuTextLine0;
    SoText3 *menuTextLine1;
    SoText3 *menuTextLine2;
    SoCube *menuBar0;
    SoBaseColor *menuBar0Col;
    SoCube *menuBar1;
    SoBaseColor *menuBar1Col;
    SoCube *menuBar2;
    SoBaseColor *menuBar2Col;
    SoTranslation *toggleBar2Spacing;

};

#endif // GUI_XRInteraction
