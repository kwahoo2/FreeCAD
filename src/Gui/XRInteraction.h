#ifndef GUI_XRInteraction
#define GUI_XRInteraction

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoCylinder.h>

class XRInteraction : public QThread
{
public:
    explicit XRInteraction();
    void stop() {worker_stopped = true;}

    void setControllerState(uint32_t id, const SoTransform *wt, const SoTranslation *st, const SoRotation *sr, float tv);
    void setMenuSeparator(SoSeparator *cs);
    void pauseThread ();
    void resumeThread();

private:
    void run() override;
    volatile bool worker_stopped = false;
    QMutex sync;
    QWaitCondition pauseCond;
    volatile bool pause = false;

    static const uint32_t hands = 2;
    float currTriggerVal[hands];
    float oldTriggerVal[hands];
    SbVec3f conTransVec[hands];
    SbRotation conRotatQuat[hands];
    SbVec3f wTransVec;
    SbVec3f wCenter;
    SbRotation wRotatQuat;
    //SoSeparator *conMenuSep;
};

#endif // GUI_XRInteraction
