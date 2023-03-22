/***************************************************************************
 *   Copyright (c) 2014 Juergen Riegel <FreeCAD@juergen-riegel.net>		   *                                                                      *
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

#if BUILD_OPENXR

#include <Base/Console.h>
#include "View3DInventorXRViewer.h"
#include <App/Application.h>
#include <cmath>

using namespace Gui;

View3DInventorXRViewer::View3DInventorXRViewer() : CoinXRWidget()
{
    workplace = new SoGroup();

    rotation1     = new SoRotationXYZ   ;
    rotation1->axis.setValue(SoRotationXYZ::X);
    rotation1->angle.setValue(-M_PI_2);
    workplace->addChild(rotation1);

    //translation  = new SoTranslation   ;
    //translation->translation.setValue(0.0f, 0.0f, -1.0f);
    //workplace->addChild(translation);

    scale        = new SoScale         ;
    scale->scaleFactor.setValue(0.001f,0.001f,0.001f); // scale from mm to m as needed by the HMD
    workplace->addChild(scale);

    setBackgroundColor(SbColor(51,51,101));
    basePosition = SbVec3f(0.0f, 0.5f, 0.8f);
}
View3DInventorXRViewer::~View3DInventorXRViewer()
{

}
void View3DInventorXRViewer::setSceneGraph(SoNode *sceneGraph)
{

    workplace->addChild(sceneGraph);

    CoinXRWidget::setSceneGraph(workplace);
}

void View3DInventorXRViewer::keyPressEvent(QKeyEvent *event)
{
    static const float increment = 0.02f; // move two centimeter per key
    static const float rotIncrement = M_PI/4; // move two 90° per key


    if (event->key() == Qt::Key_Plus) {
        scale->scaleFactor.setValue(scale->scaleFactor.getValue() * 2.0f) ;
    } else if (event->key() == Qt::Key_Minus) {
        scale->scaleFactor.setValue(scale->scaleFactor.getValue() * 0.2f) ;
    } else if (event->key() == Qt::Key_S) {
            basePosition += SbVec3f(0,0,increment) ;
    } else if (event->key() == Qt::Key_W) {
            basePosition += SbVec3f(0,0,-increment) ;
    } else if (event->key() == Qt::Key_Up) {
            basePosition += SbVec3f(0,-increment,0) ;
    } else if (event->key() == Qt::Key_Down) {
            basePosition += SbVec3f(0,increment,0) ;
    } else if (event->key() == Qt::Key_Left) {
        rotation1->angle.setValue( rotation1->angle.getValue() + rotIncrement);
    } else if (event->key() == Qt::Key_Right) {
        rotation1->angle.setValue( rotation1->angle.getValue() - rotIncrement);
    } else if (event->key() == Qt::Key_A) {
            basePosition += SbVec3f(-increment,0,0) ;
    } else if (event->key() == Qt::Key_D) {
        basePosition += SbVec3f(increment,0,0) ;
    } else {

        CoinXRWidget::keyPressEvent(event);
    }
}
// static test code ================================================================================
static View3DInventorXRViewer *window=0;

void xrStop()
{

    if(window){
        window->quitRendering();
        delete window;
        window = 0;
    }

}

bool xrUp(void)
{
    return window!=0;
}

View3DInventorXRViewer* xrStart(void)
{
    if(window)
        return window;

    window = new View3DInventorXRViewer;
    window->show();

    return window;

}
#endif //BUILD_OPENXR
