// SPDX-FileCopyrightText: 2026 Drawy contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemschemecontroller.hpp"
#include "drawyglobalconfig.h"

#include <KColorScheme>
#include <KSharedConfig>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

using namespace Qt::StringLiterals;
using SystemPalette::Mode;

SystemSchemeController::SystemSchemeController(QObject *parent)
    : QObject{parent}
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(60);
    // Symlink retargets don't always raise a watcher event, so poll a cheap signature too.
    m_poll.setInterval(400);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_debounce, &QTimer::timeout, this, &SystemSchemeController::reload);
    connect(&m_poll, &QTimer::timeout, this, [this]() {
        // defaultSource() parses palettes, so only re-detect while nothing has been found yet.
        if (signature(m_sourceCache.isEmpty() ? source() : m_sourceCache) != m_lastSignature) {
            reload();
        }
    });
}

std::optional<Mode> SystemSchemeController::mode() const
{
    return m_mode;
}

void SystemSchemeController::activate(Mode mode)
{
    m_mode = mode;
    m_lastApplied.reset();
    DrawyGlobalConfig::setSystemScheme(mode == Mode::Dark ? u"dark"_s : u"light"_s);
    DrawyGlobalConfig::self()->save();
    m_poll.start();
    reload();
    Q_EMIT modeChanged();
}

void SystemSchemeController::deactivate()
{
    if (!m_mode) {
        return;
    }
    m_mode.reset();
    m_poll.stop();
    m_debounce.stop();
    const auto watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }
    // A KDE entry may already have installed its own path; only clear ours.
    if (!m_colorsPath.isEmpty() && qApp->property("KDE_COLOR_SCHEME_PATH").toString() == m_colorsPath) {
        qApp->setProperty("KDE_COLOR_SCHEME_PATH", QVariant());
    }
    DrawyGlobalConfig::setSystemScheme(QString());
    DrawyGlobalConfig::self()->save();
    Q_EMIT modeChanged();
}

void SystemSchemeController::restore()
{
    const auto saved = DrawyGlobalConfig::systemScheme();
    if (saved == u"dark"_s) {
        activate(Mode::Dark);
    } else if (saved == u"light"_s) {
        activate(Mode::Light);
    }
}

QString SystemSchemeController::source() const
{
    const auto configured = DrawyGlobalConfig::systemPaletteSource();
    return configured.isEmpty() ? SystemPalette::defaultSource() : configured;
}

QString SystemSchemeController::signature(const QString &source) const
{
    if (source.isEmpty()) {
        return {};
    }
    QStringList parts{QFileInfo(source).canonicalFilePath()};
    for (const auto &file : SystemPalette::paletteFiles(source)) {
        const QFileInfo info(file);
        parts.append(file + u'@' + QString::number(info.lastModified().toMSecsSinceEpoch()) + u':' + QString::number(info.size()));
    }
    return parts.join(u'|');
}

void SystemSchemeController::updateWatches(const QString &source)
{
    QStringList wanted = SystemPalette::paletteFiles(source);
    const auto files = wanted;
    for (const auto &file : files) {
        wanted.append(QFileInfo(file).absolutePath());
    }
    wanted.removeDuplicates();
    const auto current = m_watcher.files() + m_watcher.directories();
    for (const auto &path : current) {
        if (!wanted.contains(path)) {
            m_watcher.removePath(path);
        }
    }
    const auto kept = m_watcher.files() + m_watcher.directories();
    for (const auto &path : std::as_const(wanted)) {
        if (!kept.contains(path)) {
            m_watcher.addPath(path);
        }
    }
}

bool SystemSchemeController::reload()
{
    if (!m_mode) {
        return false;
    }
    const auto path = source();
    m_sourceCache = path;
    m_lastSignature = signature(path);
    if (!path.isEmpty()) {
        updateWatches(path);
    }
    const auto palette = path.isEmpty() ? std::nullopt : SystemPalette::read(path);
    // Unreadable or half-written palette (mid theme switch): keep what is on screen.
    const auto &usable = palette ? palette : m_lastPalette;
    if (!usable) {
        return false;
    }
    if (m_lastPalette == usable && m_lastApplied == m_mode) {
        return false;
    }
    m_lastPalette = usable;
    apply(*usable);
    return true;
}

void SystemSchemeController::apply(const SystemPalette::RawPalette &palette)
{
    const bool dark = *m_mode == Mode::Dark;
    const auto name = dark ? u"System Dark"_s : u"System Light"_s;
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/system-schemes"_s;
    const QString path = directory + (dark ? u"/SystemDark.colors"_s : u"/SystemLight.colors"_s);
    if (!SystemPalette::writeColorsFile(SystemPalette::map(palette, *m_mode), name, path)) {
        return;
    }
    // KSharedConfig caches by path; the file was just rewritten underneath it.
    const auto config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    config->reparseConfiguration();
    m_colorsPath = path;
    qApp->setProperty("KDE_COLOR_SCHEME_PATH", path);
    qApp->setPalette(KColorScheme::createApplicationPalette(config));
    m_lastApplied = m_mode;
    Q_EMIT schemeApplied();
}

std::optional<SystemPalette::Scheme> SystemSchemeController::preview(Mode mode) const
{
    if (!m_lastPalette) {
        return std::nullopt;
    }
    return SystemPalette::map(*m_lastPalette, mode);
}

#include "moc_systemschemecontroller.cpp"
