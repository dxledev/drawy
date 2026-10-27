// SPDX-FileCopyrightText: 2026 Drawy contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libdrawywidgets_private_export.h"
#include "systempalette.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <optional>

/**
 * Applies the System Dark / System Light color scheme and keeps it in sync with the
 * active shell palette. Theme switches swap symlinks and rewrite files, so an unreadable
 * palette keeps the last good one instead of flashing a default.
 */
class LIBDRAWYWIDGETS_TESTS_EXPORT SystemSchemeController : public QObject
{
    Q_OBJECT
public:
    explicit SystemSchemeController(QObject *parent = nullptr);

    [[nodiscard]] std::optional<SystemPalette::Mode> mode() const;
    void activate(SystemPalette::Mode mode);
    void deactivate();
    // Re-applies the scheme saved in the config, if any.
    void restore();
    // Re-reads the palette now; returns whether a new scheme was applied.
    bool reload();
    // The scheme this palette would produce, for menu swatches; empty until a palette was read.
    [[nodiscard]] std::optional<SystemPalette::Scheme> preview(SystemPalette::Mode mode) const;

Q_SIGNALS:
    void modeChanged();
    void schemeApplied();

private:
    [[nodiscard]] QString source() const;
    [[nodiscard]] QString signature(const QString &source) const;
    void updateWatches(const QString &source);
    void apply(const SystemPalette::RawPalette &palette);

    std::optional<SystemPalette::Mode> m_mode;
    std::optional<SystemPalette::RawPalette> m_lastPalette;
    std::optional<SystemPalette::Mode> m_lastApplied;
    QString m_lastSignature;
    QString m_colorsPath;
    QString m_sourceCache;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QTimer m_poll;
};
