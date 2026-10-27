/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "portalfilechooser.hpp"
#include "drawy_debug.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QGuiApplication>
#include <QUrl>
#include <QUuid>
#include <QWidget>

using namespace Qt::Literals::StringLiterals;

namespace
{
struct PortalFilterEntry {
    uint type;
    QString pattern;
};
using PortalFilterEntryList = QList<PortalFilterEntry>;

struct PortalFilter {
    QString name;
    PortalFilterEntryList entries;
};
using PortalFilterList = QList<PortalFilter>;

QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilterEntry &entry)
{
    arg.beginStructure();
    arg << entry.type << entry.pattern;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PortalFilterEntry &entry)
{
    arg.beginStructure();
    arg >> entry.type >> entry.pattern;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilter &filter)
{
    arg.beginStructure();
    arg << filter.name << filter.entries;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PortalFilter &filter)
{
    arg.beginStructure();
    arg >> filter.name >> filter.entries;
    arg.endStructure();
    return arg;
}
}

Q_DECLARE_METATYPE(PortalFilterEntry)
Q_DECLARE_METATYPE(PortalFilterEntryList)
Q_DECLARE_METATYPE(PortalFilter)
Q_DECLARE_METATYPE(PortalFilterList)

PortalFileChooser::PortalFileChooser(QWidget *parentWindow)
    : QObject{parentWindow}
    , m_parentWindow{parentWindow}
{
}

bool PortalFileChooser::show(Mode mode, const QString &title, const QString &filterName, const QStringList &patterns, const QString &currentName)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }

    qDBusRegisterMetaType<PortalFilterEntry>();
    qDBusRegisterMetaType<PortalFilterEntryList>();
    qDBusRegisterMetaType<PortalFilter>();
    qDBusRegisterMetaType<PortalFilterList>();

    // The Request object path is predictable, so subscribe to Response before making the call to avoid a race.
    const QString token = u"drawy_"_s + QUuid::createUuid().toString(QUuid::Id128);
    QString sender = bus.baseService();
    sender.remove(0, 1).replace(u'.', u'_');
    const QString requestPath = u"/org/freedesktop/portal/desktop/request/%1/%2"_s.arg(sender, token);

    if (!bus.connect(u"org.freedesktop.portal.Desktop"_s,
                     requestPath,
                     u"org.freedesktop.portal.Request"_s,
                     u"Response"_s,
                     this,
                     SLOT(onResponse(uint, QVariantMap)))) {
        return false;
    }

    PortalFilter filter;
    filter.name = filterName;
    for (const QString &pattern : patterns) {
        filter.entries.append({0, pattern});
    }

    QVariantMap options;
    options.insert(u"handle_token"_s, token);
    options.insert(u"modal"_s, true);
    if (mode == Mode::OpenDirectory) {
        options.insert(u"directory"_s, true);
    } else {
        if (!patterns.isEmpty()) {
            options.insert(u"filters"_s, QVariant::fromValue(PortalFilterList{filter}));
        }
        if (mode == Mode::SaveFile && !currentName.isEmpty()) {
            options.insert(u"current_name"_s, currentName);
        }
    }

    QString parentWindow;
    if (m_parentWindow && QGuiApplication::platformName() == "xcb"_L1) {
        parentWindow = u"x11:%1"_s.arg(QString::number(m_parentWindow->winId(), 16));
    }

    QDBusMessage message = QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s,
                                                          u"/org/freedesktop/portal/desktop"_s,
                                                          u"org.freedesktop.portal.FileChooser"_s,
                                                          mode == Mode::SaveFile ? u"SaveFile"_s : u"OpenFile"_s);
    message << parentWindow << title << options;

    const QDBusMessage reply = bus.call(message, QDBus::Block, 5000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(DRAWY_LOG) << "FileChooser portal unavailable:" << reply.errorMessage();
        bus.disconnect(u"org.freedesktop.portal.Desktop"_s,
                       requestPath,
                       u"org.freedesktop.portal.Request"_s,
                       u"Response"_s,
                       this,
                       SLOT(onResponse(uint, QVariantMap)));
        return false;
    }
    return true;
}

void PortalFileChooser::onResponse(uint response, const QVariantMap &results)
{
    QString fileName;
    if (response == 0) {
        const QStringList uris = results.value(u"uris"_s).toStringList();
        if (!uris.isEmpty()) {
            fileName = QUrl(uris.constFirst()).toLocalFile();
        }
    }
    Q_EMIT finished(fileName);
    deleteLater();
}
