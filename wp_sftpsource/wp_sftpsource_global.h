/*
    This file is part of Waver

    Copyright (C) 2018 Peter Papp <peter.papp.p@gmail.com>

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

#ifndef WP_SFTPSOURCE_GLOBAL_H
#define WP_SFTPSOURCE_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(WP_SFTPSOURCE_LIBRARY)
    #define WP_SFTPSOURCE_EXPORT Q_DECL_EXPORT
#else
    #define WP_SFTPSOURCE_EXPORT Q_DECL_IMPORT
#endif

#endif // WP_SFTPSOURCE_GLOBAL_H
