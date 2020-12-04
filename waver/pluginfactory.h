#ifndef PLUGINFACTORY_H
#define PLUGINFACTORY_H

#include <QObject>
#include <QVector>

typedef QVector<QObject *> PluginFactoryResults;
typedef void (*WpPluginFactory)(int, PluginFactoryResults *);

#endif // PLUGINFACTORY_H
