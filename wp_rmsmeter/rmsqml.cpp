#include "rmsqml.h"

void RMSQML::registerTypes(const char *uri)
{
    Q_ASSERT(uri == QLatin1String("WaverRMSMeter"));
    qmlRegisterType<RMSVisual>(uri, 1, 0, "RMSVisual");
}
