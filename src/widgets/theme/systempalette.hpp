// SPDX-FileCopyrightText: 2026 Drawy contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libdrawywidgets_private_export.h"

#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

/**
 * Reads a desktop-shell palette (caelestia, noctalia, island, waybar themes...) and
 * maps it to a KColorScheme-shaped set of colors with enforced WCAG contrast.
 */
namespace SystemPalette
{
struct RawPalette {
    QMap<QString, QColor> colors;
    // "dark" or "light" when the source states it, empty otherwise.
    QString mode;
    // Colors from the opposite mode, when the source ships both (noctalia).
    QMap<QString, QColor> alternate;
    QString alternateMode;
    bool operator==(const RawPalette &) const = default;
};

struct Group {
    QColor background;
    QColor alternate;
    QColor normal;
    QColor inactive;
    QColor active;
    QColor link;
    QColor visited;
    QColor negative;
    QColor neutral;
    QColor positive;
    QColor focus;
    QColor hover;
};

struct Scheme {
    bool dark{true};
    Group view;
    Group window;
    Group button;
    Group selection;
    Group tooltip;
    Group header;
};

enum class Mode { Dark, Light };

// Directory or file; a directory is searched for Colors.qml, palette.json, noctalia.json, kitty.conf.
[[nodiscard]] LIBDRAWYWIDGETS_TESTS_EXPORT std::optional<RawPalette> read(const QString &path);
// Files whose change should trigger a reload; used for the watcher and cheap polling.
[[nodiscard]] LIBDRAWYWIDGETS_TESTS_EXPORT QStringList paletteFiles(const QString &path);
// Palette location to follow when the user has not configured one; empty if nothing exists.
[[nodiscard]] LIBDRAWYWIDGETS_TESTS_EXPORT QString defaultSource();

[[nodiscard]] LIBDRAWYWIDGETS_TESTS_EXPORT Scheme map(const RawPalette &palette, Mode mode);
[[nodiscard]] LIBDRAWYWIDGETS_TESTS_EXPORT double contrast(const QColor &foreground, const QColor &background);
// Serialises to the KDE .colors format.
[[nodiscard]] LIBDRAWYWIDGETS_TESTS_EXPORT bool writeColorsFile(const Scheme &scheme, const QString &name, const QString &path);
}
