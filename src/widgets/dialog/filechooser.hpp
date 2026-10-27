/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>
#include <functional>

class QObject;
class QWidget;

/**
 * Asks for a file or directory through the desktop's FileChooser portal (omafiles, GTK, KDE...),
 * falling back to Qt's own dialog when the portal is unavailable.
 * The callback receives the chosen path, or an empty string if cancelled. It is not called if @p context is destroyed first.
 */
namespace FileChooser
{
enum class Mode {
    OpenFile,
    SaveFile,
    OpenDirectory,
};

void choose(QWidget *parent,
            Mode mode,
            const QString &title,
            const QString &filterName,
            const QStringList &patterns,
            const QString &currentName,
            QObject *context,
            const std::function<void(const QString &)> &callback);

/// Blocking variant for callers that must have the answer before continuing. Returns an empty string if cancelled.
QString getPath(QWidget *parent, Mode mode, const QString &title, const QString &filterName, const QStringList &patterns, const QString &currentName);
}
