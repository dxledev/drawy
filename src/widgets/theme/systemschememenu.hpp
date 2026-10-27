// SPDX-FileCopyrightText: 2026 Drawy contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libdrawywidgets_private_export.h"

class KActionMenu;
class SystemSchemeController;

namespace SystemSchemeMenu
{
// Adds "System Dark" and "System Light" after "Default" in a KColorSchemeMenu and keeps the
// controller, the checkmarks and the KDE entries consistent.
LIBDRAWYWIDGETS_TESTS_EXPORT void install(KActionMenu *schemeMenu, SystemSchemeController *controller);
}
