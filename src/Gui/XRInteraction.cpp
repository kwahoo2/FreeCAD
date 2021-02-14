#include "PreCompiled.h"
#include "XRInteraction.h"

#include "Base/Exception.h"
#include <Base/Console.h>

#include <Command.h>
#include "Application.h"
#include "Document.h"


XRInteraction::XRInteraction() : QThread()
{

}

void XRInteraction::run()
{

    uint32_t objCount = 0;
    App::Document* doc = App::GetApplication().getActiveDocument();
    QString cmd = QString::fromLatin1("import Part, math");
    Gui::Command::doCommand(Gui::Command::Doc, cmd.toUtf8());

    while (!worker_stopped)
    {
        sync.lock();
        if(pause)
        {
            pauseCond.wait(&sync); // pause until resume called from CoinXRWidget
        }
        sync.unlock();

        for (uint32_t i = 0; i < hands; i++){
            if (currTriggerVal[i] > 0.5 && oldTriggerVal[i] <= 0.5)
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
                 /*meters to milimeters conversion
                  * TODO world transformation, translation only so far
                  *rotation using controller stick will put the box i wrong place*/
                .arg((conTransVec[i][0] - wTransVec[0]) * 1000)
                .arg((conTransVec[i][1] - wTransVec[1]) * 1000)
                .arg((conTransVec[i][2] - wTransVec[2]) * 1000)
                .arg(conRotatQuat[i][0])
                .arg(conRotatQuat[i][1])
                .arg(conRotatQuat[i][2])
                .arg(conRotatQuat[i][3]);


                Gui::Command::doCommand(Gui::Command::Doc, cmd.toUtf8());
                //doc->recompute();
                objCount++;
            }
            oldTriggerVal[i] = currTriggerVal[i];
        }
        pauseThread(); //do only one loop and wait for unlock
    }
}

void XRInteraction::setControllerState(uint32_t id, const SoTransform *wt, const SoTranslation *st, const SoRotation *sr, float tv)
{
    conTransVec[id] = st->translation.getValue();
    conRotatQuat[id] = sr->rotation.getValue();
    currTriggerVal[id] = tv;

    wTransVec = wt->translation.getValue(); //take note of worldtranformation, world can be moved relative to physical position
    wCenter = wt->center.getValue();
    wRotatQuat = wt->rotation.getValue();
    //wRotatQuat.invert();

    /*Base::Console().Warning("Hand pose %d: %f %f %f %f, %f %f %f\n", id,
                           conRotat[id]->rotation.getValue()[0], conRotat[id]->rotation.getValue()[1],conRotat[id]->rotation.getValue()[2],conRotat[id]->rotation.getValue()[3],
                           conTrans[id]->translation.getValue()[0], conTrans[id]->translation.getValue()[1], conTrans[id]->translation.getValue()[2]);*/
}

void XRInteraction::setMenuSeparator(SoSeparator *cs)
{
    //conMenuSep = cs;

    SoCylinder *menuSupport = new SoCylinder();
    menuSupport->radius.setValue(100.0f);
    menuSupport->height.setValue(50.0f);
    //conMenuSep->addChild(menuSupport);
}

void XRInteraction::resumeThread()
{
    sync.lock();
    pause = false;
    sync.unlock();
    pauseCond.wakeAll();
}

void XRInteraction::pauseThread()
{
    sync.lock();
    pause = true;
    sync.unlock();
}
