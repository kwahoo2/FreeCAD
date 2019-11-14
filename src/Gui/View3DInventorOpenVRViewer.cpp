/***************************************************************************
 *   Copyright (c) 2014 Juergen Riegel <FreeCAD@juergen-riegel.net>		   *
 *   Copyright (c) 2019 Adrian Przekwas <adrian.v.przekwas@gmail.com>	   *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#if BUILD_OPENVR

#include <Base/Console.h>
#include "View3DInventorOpenVRViewer.h"
#include <App/Application.h>

using namespace Gui;

View3DInventorOpenVRViewer::View3DInventorOpenVRViewer() : CoinOpenVRWidget()
{
    workplace = new SoGroup();
    //translation  = new SoTranslation   ;
    //translation->translation.setValue(0,-1,0);
    //workplace->addChild(translation);

    rotation1     = new SoRotationXYZ   ;
    rotation1->axis.setValue(SoRotationXYZ::X);
    rotation1->angle.setValue(0);
    workplace->addChild(rotation1);

    rotation2     = new SoRotationXYZ   ;
    rotation2->axis.setValue(SoRotationXYZ::Z);
    rotation2->angle.setValue(0);
    workplace->addChild(rotation2);


    scale        = new SoScale         ;
    scale->scaleFactor.setValue(0.001f,0.001f,0.001f); // scale from mm to m as needed by the HMD
    //scale->scaleFactor.setValue(1.0f,1.0f,1.0f);
    workplace->addChild(scale);

    setBackgroundColor(SbColor(51,51,101));
    basePosition = SbVec3f(0.0f, 0.0f, 0.0f);
}

View3DInventorOpenVRViewer::~View3DInventorOpenVRViewer()
{

}

void View3DInventorOpenVRViewer::setSceneGraph(SoNode *sceneGraph)
{

    workplace->addChild(sceneGraph);

    CoinOpenVRWidget::setSceneGraph(workplace);
}


// static test code ================================================================================
static View3DInventorOpenVRViewer *window=0;

void openvrStop()
{
    if(window){
        delete window;
        window = 0;
    }

}

bool openvrUp(void)
{
    return window!=0;
}

View3DInventorOpenVRViewer* openvrStart(void)
{
    if(window)
        return window;

    window = new View3DInventorOpenVRViewer;
    window->show();

    return window;

}

#endif //BUILD_OPENVR
