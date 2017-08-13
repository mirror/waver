/*
    This file is part of Waver

    Copyright (C) 2017 Peter Papp <peter.papp.p@gmail.com>

    Please visit https://launchpad.net/waver for details

    Waver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Waver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    (GPL.TXT) along with Waver. If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef TAGLIBINFO_H
#define TAGLIBINFO_H

#include "wp_taglibinfo_global.h"

#include <QFile>
#include <QUrl>
#include <QUuid>
#include <taglib/fileref.h>
#include <taglib/tstring.h>

#include "../waver/pluginfactory.h"
#include "../waver/API/0.0.1/plugininfo.h"


extern "C" WP_TAGLIBINFO_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_TAGLIBINFO_EXPORT TagLibInfo : public PluginInfo {
        Q_OBJECT

    public:

        explicit TagLibInfo();

        int     pluginType()                   override;
        QString pluginName()                   override;
        int     pluginVersion()                override;
        QString waverVersionAPICompatibility() override;
        QUuid   persistentUniqueId()           override;
        bool    hasUI()                        override;
        void    setUrl(QUrl url)               override;


    private:

        QUuid id;
        QUrl  url;


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

};

#endif // TAGLIBINFO_H
