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

#include "PreCompiled.h"
#include "XRInteraction.h"


XRInteraction::XRInteraction()
{
    doc = App::GetApplication().getActiveDocument();
    cmd = QString::fromLatin1("import Part, math");
    Gui::Command::doCommand(Gui::Command::Doc, cmd.toUtf8());

    //menu
    menuSep = new SoSeparator;
    menuSep->ref();
    SoTranslation *menuTrans = new SoTranslation;
    menuTrans->translation.setValue(SbVec3f(0.0f, 0.12f,-0.15f));
    menuSep->addChild(menuTrans);
    menuCube = new SoCube();
    menuCube->width.setValue(0.2f);
    menuCube->height.setValue(0.2f);
    menuCube->depth.setValue(0.001f);
    menuSep->addChild(menuCube);
    SoTranslation *textTrans = new SoTranslation;
    textTrans->translation.setValue(SbVec3f(0.0f, 0.1f,0.01f));
    menuSep->addChild(textTrans);
    menuText = new SoText3;
    SoScale * textScale = new SoScale;
    textScale->scaleFactor.setValue(SbVec3f(0.005f, 0.005f, 0.005f));
    menuText->string = "Press triggers to enable ray";
    menuSep->addChild(textScale);
    menuSep->addChild(menuText);


    //ray for picking objects
    rSep = new SoSeparator();
    rSep->ref();
    rayVtxs = new SoVertexProperty();
    rayVtxs->vertex.set1Value(0, 0, 0, 0);  // Set first vertex, later update to center of the controller
    rayVtxs->vertex.set1Value(1, 0, 0, 1);  // Set second vertex, later update to point of intersection ray with hit object
    rayLine = new SoLineSet();
    rayLine->vertexProperty = rayVtxs;
    SoBaseColor * rayCol = new SoBaseColor; //blue sphere to show intersection point
    rayCol->rgb = SbColor(1, 0, 0);
    rSep->addChild(rayCol);
    rSep->addChild(rayLine); //only one controller will shoot the ray
    sphTrans = new SoTranslation;
    sphTrans->translation.setValue(0, 0, 0);
    rSep->addChild(sphTrans);
    raySph = new SoSphere;
    raySph->radius.setValue(0.02f);
    rSep->addChild(raySph);

}

void XRInteraction::applyInput()
{
        for (uint32_t i = 0; i < hands; i++){
            if (currTriggerVal[i] > 0.9 && oldTriggerVal[i] <= 0.9)
                {

                double l = 200.0;
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
                 /*meters to milimeters conversion*/
                .arg((conTransVec[i][0]) * 1000)
                .arg((conTransVec[i][1]) * 1000)
                .arg((conTransVec[i][2]) * 1000)
                .arg(conRotatQuat[i][0])
                .arg(conRotatQuat[i][1])
                .arg(conRotatQuat[i][2])
                .arg(conRotatQuat[i][3]);


                Gui::Command::doCommand(Gui::Command::Doc, cmd.toUtf8());
                //doc->recompute();
                objCount++;

                std::string s = "Cube " + std::to_string(objCount);
                menuText->string = s.c_str();
            }
            oldTriggerVal[i] = currTriggerVal[i];
        }

}

void XRInteraction::setControllerState(uint32_t id, const SoTranslation *st, const SoRotation *sr, float tv)
{
    conTransVec[id] = st->translation.getValue();
    conRotatQuat[id] = sr->rotation.getValue();
    currTriggerVal[id] = tv;

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
                                     float nearPlane, float farPlane)
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

    }
    return pickedPoint;
}


XRInteraction::~XRInteraction()
{
    menuSep->unref();
    rSep->unref();
}
