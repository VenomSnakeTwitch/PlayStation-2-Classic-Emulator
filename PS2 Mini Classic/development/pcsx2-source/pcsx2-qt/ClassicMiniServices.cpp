// SPDX-License-Identifier: GPL-3.0+
// This data-management unit uses exceptions for transactional filesystem recovery.
#undef _HAS_EXCEPTIONS
#define _HAS_EXCEPTIONS 1
#include "QtHost.h"
#include "pcsx2/Host.h"
#include "pcsx2/Config.h"
#include "pcsx2/VMManager.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiFullscreen.h"
#include "pcsx2/MTGS.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QProcess>
#include <QtCore/QSet>
#ifdef CLASSIC_MINI_DATA_SELFTEST
namespace Host { void RunClassicMiniDataSelfTest(); }
#endif
#include "ClassicMiniServices.inl"
