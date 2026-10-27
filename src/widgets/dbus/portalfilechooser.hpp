/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QWidget;

/**
 * Opens a file through the org.freedesktop.portal.FileChooser portal, i.e. the file chooser
 * configured for the desktop (omafiles, GTK, KDE...). Deletes itself once finished.
 */
class PortalFileChooser : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        OpenFile,
        SaveFile,
        OpenDirectory,
    };

    explicit PortalFileChooser(QWidget *parentWindow);
    ~PortalFileChooser() override = default;

    /// Returns false if the portal could not be reached; fileSelected()/cancelled() are then never emitted.
    bool show(Mode mode, const QString &title, const QString &filterName, const QStringList &patterns, const QString &currentName = {});

Q_SIGNALS:
    /// Emitted with the chosen local file path, or an empty string if cancelled or failed.
    void finished(const QString &fileName);

private Q_SLOTS:
    void onResponse(uint response, const QVariantMap &results);

private:
    QWidget *const m_parentWindow;
};
