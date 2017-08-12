#ifndef PLUGINFACTORY_H
#define PLUGINFACTORY_H

typedef QVector<QObject *> PluginFactoryResults;
typedef void (*WpPluginFactory)(int, PluginFactoryResults *);

#endif // PLUGINFACTORY_H
