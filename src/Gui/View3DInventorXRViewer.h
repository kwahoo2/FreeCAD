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


#ifndef GUI_View3DInventorXRViewer_H
#define GUI_View3DInventorXRViewer_H

#if BUILD_OPENXR

#include "CoinXRWidget.h"

namespace Gui {

class View3DInventorXRViewer : public CoinXRWidget
{
public:
    View3DInventorXRViewer();
    ~View3DInventorXRViewer();

virtual void setSceneGraph(SoNode *sceneGraph);

protected:
    SoGroup         *workplace;
    SoTranslation   *translation;
    SoRotationXYZ   *rotation1;
    SoRotationXYZ   *rotation2;
    SoScale         *scale;

protected:
    void keyPressEvent(QKeyEvent *);
};

} //namespace Gui

#endif //BUILD_OPENXR

#endif //GUI_View3DInventorXRViewer_H
