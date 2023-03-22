/**************************************************************************\
* Copyright (c) 2021 Adrian Przekwas <adrian.v.przekwas@gmail.com>        *
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
#include "XRInteraction.h"
#include <View3DInventor.h>
#include <View3DInventorViewer.h>
#include <ViewProvider.h>
#include "ViewProviderDocumentObject.h"
#include "ViewProviderExtern.h"

XRInteraction::XRInteraction()
{
    /*doc = App::GetApplication().getActiveDocument();

    cmd = QString::fromLatin1("import Part, math, pivy");
    Gui::Command::doCommand(Gui::Command::Doc, cmd.toUtf8());*/

    ParameterGrp::handle xrGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/XRViewer");
    movementSpeed = static_cast<float>(xrGrp->GetFloat("MovementSpeed", 1.0));
    rotationSpeed = static_cast<float>(xrGrp->GetFloat("RotationSpeed", 1.0));
    selectedControlScheme = static_cast<int32_t>(xrGrp->GetInt("ControlScheme", 0));

    //menu  
    menuSep = new SoSeparator;
    menuSep->ref();
    SoTranslation *menuTrans = new SoTranslation;
    menuTrans->translation.setValue(SbVec3f(0.0f, 0.12f,-0.15f));
    menuSep->addChild(menuTrans);
    SoPointLight *menuLight = new SoPointLight;
    menuLight->location = SbVec3f(0.0, 0.0f, 1.0f);
    menuSep->addChild(menuLight);

    SoTranslation *textTrans = new SoTranslation;
    textTrans->translation.setValue(SbVec3f(0.0f, 0.05f,0.01f));
    menuSep->addChild(textTrans);
    menuText = new SoText3; //this is status bar
    SoScale * textScale = new SoScale;
    textScale->scaleFactor.setValue(SbVec3f(0.002f, 0.002f, 0.002f));
    menuText->string = "Trigger toggles menu";
    menuSep->addChild(textScale);
    menuSep->addChild(menuText);

    SoTranslation *lineSpacing = new SoTranslation;
    SoTranslation *barSpacing = new SoTranslation;
    lineSpacing->translation.setValue(SbVec3f(0.0f, -15.0f, 0.0f));
    barSpacing->translation.setValue(SbVec3f(50.0f, 3.0f, -2.0f));
    menuSep->addChild(lineSpacing);
    menuTextLine0 = new SoText3; //this menu item
    menuTextLine0->string = "Movement speed";
    menuSep->addChild(menuTextLine0);
    //this is bar representing value
    SoSeparator * menuBar0Sep =  new SoSeparator;
    menuBar0 = new SoCube();
    menuBar0->width.setValue(100.0f);
    menuBar0->height.setValue(12.0f);
    menuBar0->depth.setValue(1.0f);
    menuBar0Col = new SoBaseColor;
    menuBar0Col->rgb = deactivatedColor;
    menuBar0Sep->addChild(menuBar0Col);
    menuBar0Sep->addChild(barSpacing);
    menuBar0Sep->addChild(menuBar0);
    menuSep->addChild(menuBar0Sep);
    menuBar0->width.setValue(100 * movementSpeed);

    menuSep->addChild(lineSpacing);
    menuTextLine1 = new SoText3; //this menu item
    menuTextLine1->string = "Rotation speed";
    menuSep->addChild(menuTextLine1);
    SoSeparator * menuBar1Sep =  new SoSeparator;
    menuBar1 = new SoCube();
    menuBar1->width.setValue(100.0f);
    menuBar1->height.setValue(12.0f);
    menuBar1->depth.setValue(1.0f);
    menuBar1Col = new SoBaseColor;
    menuBar1Col->rgb = deactivatedColor;
    menuBar1Sep->addChild(menuBar1Col);
    menuBar1Sep->addChild(barSpacing);
    menuBar1Sep->addChild(menuBar1);
    menuSep->addChild(menuBar1Sep);
    menuBar1->width.setValue(100 * rotationSpeed);

    menuSep->addChild(lineSpacing);
    menuTextLine2 = new SoText3; //this menu item
    menuTextLine2->string = "[Free] [Arch]";
    menuSep->addChild(menuTextLine2);
    SoSeparator * menuBar2Sep =  new SoSeparator;
    menuBar2 = new SoCube();
    menuBar2->width.setValue(32.0f);
    menuBar2->height.setValue(12.0f);
    menuBar2->depth.setValue(1.0f);
    menuBar2Col = new SoBaseColor;
    menuBar2Col->rgb = deactivatedColor;
    menuBar2Sep->addChild(menuBar2Col);
    menuBar2Sep->addChild(barSpacing);
    toggleBar2Spacing = new SoTranslation;
    toggleBar2Spacing->translation.setValue(SbVec3f(-35.0f, 0.0f, 0.0f));
    menuBar2Sep->addChild(toggleBar2Spacing);
    menuBar2Sep->addChild(menuBar2);
    updateMenuBar2();
    menuSep->addChild(menuBar2Sep);


    //ray for picking objects
    rSep = new SoSeparator();
    rSep->ref();
    rayVtxs = new SoVertexProperty();
    rayVtxs->vertex.set1Value(0, 0, 0, 0);  // Set first vertex, later update to center of the controller
    rayVtxs->vertex.set1Value(1, 0, 0, 1);  // Set second vertex, later update to point of intersection ray with hit object
    rayLine = new SoLineSet();
    rayLine->vertexProperty = rayVtxs;
    SoBaseColor * rayCol = new SoBaseColor; //red sphere to show intersection point
    rayCol->rgb = SbColor(1, 0, 0);
    rSep->addChild(rayCol);
    rSep->addChild(rayLine); //only one controller will shoot the ray
    sphTrans = new SoTranslation;
    sphTrans->translation.setValue(0, 0, 0);
    rSep->addChild(sphTrans);
    raySph = new SoSphere;
    raySph->radius.setValue(0.02f);
    rSep->addChild(raySph);

    for (uint32_t id = 0; id < hands; id++)
    {
        currTriggerVal[id] = 0.0f;
        currXStickVal[id] = 0.0f;
        currYStickVal[id] = 0.0f;
        oldTriggerVal[id] = 0.0f;
        oldXStickVal[id] = 0.0f;
        oldYStickVal[id] = 0.0f;
    }

}

void XRInteraction::applyInput(uint32_t conId)
{
        if (currTriggerVal[conId] > 0.9f && oldTriggerVal[conId] <= 0.9f)
            {
            if (conId == primaryConId)
                {
                    toggleMenuMode();
                }
                else
                {

                }
            }


            //uncomment to enable creating cubes in space when a trigger is pressed
            /*double l = 200.0;
            double w = 100.0;
            double h = 500.0;

            cmd = QString::fromLatin1("box%1 = App.getDocument('%2').addObject('%3','%4')\n"
            "box%1.Length = %5\n"
            "box%1.Width = %6\n"
            "box%1.Height = %7\n"
            "q = (%11, %12, %13, %14)\n"
            "r=FreeCAD.Rotation(*q)\n"
            "box%1.Placement.rotate(App.Vector(0, 0, 0), r.Axis, r.Angle*180/math.pi)\n"
            "box%1.Placement.move(App.Vector(%8, %9, %10))\n"
            )
            .arg(QString::number(objCount))
            .arg(QString::fromLatin1(doc->getName()))
            .arg(QString::fromLatin1("Part::Box"))
            .arg(QString::fromLatin1("Cube") + QString::number(objCount))
            .arg(l)
            .arg(w)
            .arg(h)
             //meters to milimeters conversion
            .arg((conTransVec[conId][0]) * 1000)
            .arg((conTransVec[conId][1]) * 1000)
            .arg((conTransVec[conId][2]) * 1000)
            .arg(conRotatQuat[conId][0])
            .arg(conRotatQuat[conId][1])
            .arg(conRotatQuat[conId][2])
            .arg(conRotatQuat[conId][3]);


            Gui::Command::doCommand(Gui::Command::Doc, cmd.toUtf8());
            //doc->recompute();
            objCount++;

            std::string s = "Cube " + std::to_string(objCount);
            menuText->string = s.c_str();*/

        if (conId == primaryConId)
        {
            if (isMenuMode())
            {
                if (currYStickVal[conId] > 0.5f && oldYStickVal[conId] <= 0.5f)
                {
                    selectedMenuItem--;
                }
                if (currYStickVal[conId] < -0.5f && oldYStickVal[conId] >= -0.5f)
                {
                    selectedMenuItem++;
                }
                if (selectedMenuItem > numberOfMenuItems)
                        selectedMenuItem = 0;
                if (selectedMenuItem < 0)
                        selectedMenuItem = numberOfMenuItems;
                switch (selectedMenuItem) {
                case 0:
                    menuBar0Col->rgb = activatedColor;
                    menuBar1Col->rgb = deactivatedColor;
                    menuBar2Col->rgb = deactivatedColor;
                    changeMovementSpeed(currXStickVal[conId]);
                    menuBar0->width.setValue(100 * movementSpeed);
                    break;
                case 1:
                    menuBar0Col->rgb = deactivatedColor;
                    menuBar1Col->rgb = activatedColor;
                    menuBar2Col->rgb = deactivatedColor;
                    changeRotationSpeed(currXStickVal[conId]);
                    menuBar1->width.setValue(100 * rotationSpeed);
                    break;
                case 2:
                    menuBar0Col->rgb = deactivatedColor;
                    menuBar1Col->rgb = deactivatedColor;
                    menuBar2Col->rgb = activatedColor;
                    toggleMenuBar2(conId);
                    menuBar1->width.setValue(100 * rotationSpeed);
                    break;
                default:
                    menuBar0Col->rgb = deactivatedColor;
                    menuBar1Col->rgb = deactivatedColor;
                    menuBar2Col->rgb = deactivatedColor;
                    break;
                }

            }
            else
            {
                menuBar0Col->rgb = deactivatedColor;
                menuBar1Col->rgb = deactivatedColor;
                menuBar2Col->rgb = deactivatedColor;
            }

        }

        oldTriggerVal[conId] = currTriggerVal[conId];
        oldXStickVal[conId] = currXStickVal[conId];
        oldYStickVal[conId] = currYStickVal[conId];

}


void XRInteraction::setControllerState(uint32_t id, const SoTranslation *st, const SoRotation *sr, float tv, float xv, float yv)
{
    conTransVec[id] = st->translation.getValue();
    conRotatQuat[id] = sr->rotation.getValue();
    currTriggerVal[id] = tv;
    currXStickVal[id] = xv;
    currYStickVal[id] = yv;

}

SoSeparator * XRInteraction::getMenuSeparator()
{
    return menuSep;
}

SoSeparator * XRInteraction::getRaySeparator()
{
    return rSep;
}

const SoPickedPoint * XRInteraction::findPickedObject(SoSeparator *sep, SbViewportRegion vpReg,
                                     SbVec3f startVec, SbVec3f endVec, SbVec3f rayAxis,
                                     float nearPlane, float farPlane,
                                     bool &isNewPickedPoint, SbVec3f &ppCoords)
{
    //picking ray
    SoRayPickAction conPickAction(vpReg);
    conPickAction.setRay(startVec, - rayAxis, //direction is reversed controller Z axis
                         nearPlane, farPlane);

    rayVtxs->vertex.set1Value(0, startVec);
    rayVtxs->vertex.set1Value(1, endVec);

    conPickAction.apply(sep); //traverse only the world, avoid picking controller gizmos or other non-world objects
    const SoPickedPoint *pickedPoint = conPickAction.getPickedPoint();

    if (pickedPoint != nullptr)
    {
        SbVec3f pickedPCoords = pickedPoint->getPoint();
        sphTrans->translation.setValue(pickedPCoords);
        rayVtxs->vertex.set1Value(1, pickedPCoords);
        ppCoords = pickedPCoords;
        isNewPickedPoint = true;

    }

    return pickedPoint;
}

bool XRInteraction::isMenuMode()
{
    return menuMode;
}

void XRInteraction::toggleMenuMode()
{
    menuMode = !menuMode;
    if (!menuMode)
    {
        ParameterGrp::handle xrGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/XRViewer");
        xrGrp->SetFloat("MovementSpeed", static_cast<double>(movementSpeed));
        xrGrp->SetFloat("RotationSpeed", static_cast<double>(rotationSpeed));
        xrGrp->SetInt("ControlScheme", (selectedControlScheme));
    }
}

void XRInteraction::setPriAndSecController (uint32_t pri, uint32_t sec)
{
    primaryConId = pri;
    secondaryConId = sec;
}

void XRInteraction::changeMovementSpeed (float xv)
{
    float step = 0.01f;
    if ((xv > 0.1f) || (xv < 0.1f))
    {
        movementSpeed = movementSpeed + step * xv;
    }
    if (movementSpeed < 0.01f)
    {
        movementSpeed = 0.01f;
    }
    if (movementSpeed > 2.0f)
    {
        movementSpeed = 2.0f;
    }
}

void XRInteraction::changeRotationSpeed (float xv)
{
    float step = 0.01f;
    if ((xv > 0.1f) || (xv < 0.1f))
    {
        rotationSpeed = rotationSpeed + step * xv;
    }
    if (rotationSpeed < 0.01f)
    {
        rotationSpeed = 0.01f;
    }
    if (rotationSpeed > 2.0f)
    {
        rotationSpeed = 2.0f;
    }
}

void XRInteraction::toggleMenuBar2(uint32_t conId)
{
    if (currXStickVal[conId] > 0.5f && oldXStickVal[conId] <= 0.5f)
    {
        selectedControlScheme++;
    }
    if (currXStickVal[conId] < -0.5f && oldXStickVal[conId] >= -0.5f)
    {
        selectedControlScheme--;
    }
    if (selectedControlScheme > 1)
        selectedControlScheme = 1;
    if (selectedControlScheme < 0)
        selectedControlScheme = 0;
    updateMenuBar2();
}
void XRInteraction::updateMenuBar2()
{
    if (selectedControlScheme == 0)
    {
        toggleBar2Spacing->translation.setValue(SbVec3f(-35.0f, 0.0f, 0.0f));
    }
    if (selectedControlScheme == 1)
    {
        toggleBar2Spacing->translation.setValue(SbVec3f(-3.0f, 0.0f, 0.0f));
    }

}

float XRInteraction::getMovementSpeed()
{
    return movementSpeed;
}

float XRInteraction::getRotationSpeed()
{
    return rotationSpeed;
}

int32_t XRInteraction::getControlScheme()
{
    return selectedControlScheme;
}

XRInteraction::~XRInteraction()
{
    menuSep->unref();
    rSep->unref();
}
