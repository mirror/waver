/*
    This file is part of WaverIIR
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waveriir for details
*/


#ifndef WAVERIIR_GLOBAL_H
#define WAVERIIR_GLOBAL_H

#include <QtCore/QtGlobal>

#if defined(WAVERIIR_LIBRARY)
#  define WAVERIIR_EXPORT Q_DECL_EXPORT
#else
#  define WAVERIIR_EXPORT Q_DECL_IMPORT
#endif

#endif // WAVERIIR_GLOBAL_H
