/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "filechooser.hpp"
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QObject>
#include <QTimer>
#ifdef DRAWY_USE_DBUS
#include "dbus/portalfilechooser.hpp"
#endif

namespace
{
QString qtDialog(QWidget *parent,
                 FileChooser::Mode mode,
                 const QString &title,
                 const QString &filterName,
                 const QStringList &patterns,
                 const QString &currentName)
{
    const QString filter = patterns.isEmpty() ? QString{} : QStringLiteral("%1 (%2)").arg(filterName, patterns.join(QLatin1Char(' ')));
    switch (mode) {
    case FileChooser::Mode::SaveFile:
        return QFileDialog::getSaveFileName(parent, title, QDir::home().filePath(currentName), filter);
    case FileChooser::Mode::OpenDirectory:
        return QFileDialog::getExistingDirectory(parent, title, QDir::homePath(), QFileDialog::ShowDirsOnly);
    case FileChooser::Mode::OpenFile:
        break;
    }
    return QFileDialog::getOpenFileName(parent, title, QDir::homePath(), filter);
}
}

void FileChooser::choose(QWidget *parent,
                         Mode mode,
                         const QString &title,
                         const QString &filterName,
                         const QStringList &patterns,
                         const QString &currentName,
                         QObject *context,
                         const std::function<void(const QString &)> &callback)
{
    // Deferred so the modal dialog never runs inside the event handler that triggered it.
    QTimer::singleShot(0, context, [=] {
#ifdef DRAWY_USE_DBUS
        const auto portalMode = mode == Mode::SaveFile ? PortalFileChooser::Mode::SaveFile
            : mode == Mode::OpenDirectory              ? PortalFileChooser::Mode::OpenDirectory
                                                       : PortalFileChooser::Mode::OpenFile;
        auto *chooser = new PortalFileChooser(parent);
        QObject::connect(chooser, &PortalFileChooser::finished, context, [callback](const QString &fileName) {
            callback(fileName);
        });
        if (chooser->show(portalMode, title, filterName, patterns, currentName)) {
            return;
        }
        delete chooser;
#endif
        callback(qtDialog(parent, mode, title, filterName, patterns, currentName));
    });
}

QString FileChooser::getPath(QWidget *parent, Mode mode, const QString &title, const QString &filterName, const QStringList &patterns, const QString &currentName)
{
#ifdef DRAWY_USE_DBUS
    const auto portalMode = mode == Mode::SaveFile ? PortalFileChooser::Mode::SaveFile
        : mode == Mode::OpenDirectory              ? PortalFileChooser::Mode::OpenDirectory
                                                   : PortalFileChooser::Mode::OpenFile;
    auto *chooser = new PortalFileChooser(parent);
    QEventLoop loop;
    QString result;
    QObject::connect(chooser, &PortalFileChooser::finished, &loop, [&](const QString &fileName) {
        result = fileName;
        loop.quit();
    });
    if (chooser->show(portalMode, title, filterName, patterns, currentName)) {
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        return result;
    }
    delete chooser;
#endif
    return qtDialog(parent, mode, title, filterName, patterns, currentName);
}
