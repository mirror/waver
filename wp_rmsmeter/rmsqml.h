#ifndef RMSQML_H
#define RMSQML_H

#include <QObject>
#include <QQmlExtensionPlugin>
#include <QQmlExtensionInterface>
#include "rmsvisual.h"


class RMSQML : public QQmlExtensionPlugin {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

    public:
        void registerTypes(const char *uri) override;
};

#endif // RMSQML_H
