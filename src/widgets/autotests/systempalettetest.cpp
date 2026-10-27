/*
 * SPDX-FileCopyrightText: 2026 Drawy contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "systempalettetest.hpp"
#include "drawyglobalconfig.h"
#include "theme/systempalette.hpp"
#include "theme/systemschemecontroller.hpp"
#include "theme/systemschememenu.hpp"

#include <KActionMenu>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QMenu>
#include <cstdio>
#include <QFile>
#include <QPalette>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

QTEST_MAIN(SystemPaletteTest)
using namespace Qt::Literals::StringLiterals;
using namespace SystemPalette;

namespace
{
// The shell's real dark-only, pastel dynamic palette.
const QByteArray islandJson = R"({"mode":"dark","base":"121520","text":"d8cdb1","subtext0":"6f7177","primary":"9dadd8",
"red":"d69a8e","green":"d8b987","yellow":"e1c390","surface0":"23252c"})";

const QByteArray lightQml = R"(pragma Singleton
import QtQuick
Singleton {
    readonly property color background: "#f6f1e7"
    readonly property color foreground: "#2b2a33"
    readonly property color foregroundInactive: "#9a968c"
    readonly property color primary: "#a9c7b0"
    readonly property color danger: "#e8a090"
    readonly property color warning: "#f0d080"
    readonly property color success: "#b8e0a8"
})";

const QByteArray noctaliaBoth = R"({"dark":{"mPrimary":"#e37b66","mSurface":"#101318","mOnSurface":"#eaeff5","mError":"#dd6952"},
"light":{"mPrimary":"#c24a33","mSurface":"#fbf7f2","mOnSurface":"#1a1c1f","mError":"#b3261e"}})";

void write(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}

void verifyGroup(const Group &group, double normal, const char *name)
{
    for (const auto &background : {group.background, group.alternate}) {
        QVERIFY2(contrast(group.normal, background) >= normal, name);
        QVERIFY2(contrast(group.inactive, background) >= 4.5, name);
        QVERIFY2(contrast(group.link, background) >= 4.5, name);
        QVERIFY2(contrast(group.negative, background) >= 3.0, name);
        QVERIFY2(contrast(group.neutral, background) >= 3.0, name);
        QVERIFY2(contrast(group.positive, background) >= 3.0, name);
    }
    QVERIFY2(contrast(group.focus, group.background) >= 3.0, name);
}

void verifyScheme(const Scheme &scheme)
{
    verifyGroup(scheme.view, 7.0, "view");
    verifyGroup(scheme.window, 7.0, "window");
    verifyGroup(scheme.button, 7.0, "button");
    verifyGroup(scheme.tooltip, 7.0, "tooltip");
    verifyGroup(scheme.selection, 7.0, "selection");
    // The scheme must actually be the requested mode, not a light palette mapped as dark.
    QCOMPARE(scheme.view.background.lightnessF() < 0.5, scheme.dark);
}
}

SystemPaletteTest::SystemPaletteTest(QObject *parent)
    : QObject{parent}
{
    QStandardPaths::setTestModeEnabled(true);
}

void SystemPaletteTest::shouldMeetContrastForDarkIslandPalette_data()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("dark") << true;
    QTest::newRow("light") << false;
}

void SystemPaletteTest::shouldMeetContrastForDarkIslandPalette()
{
    QFETCH(bool, dark);
    QTemporaryDir dir;
    write(dir.filePath(u"palette.json"_s), islandJson);
    const auto palette = read(dir.path());
    QVERIFY(palette);
    QCOMPARE(palette->mode, u"dark"_s);
    const auto scheme = map(*palette, dark ? Mode::Dark : Mode::Light);
    QCOMPARE(scheme.dark, dark);
    verifyScheme(scheme);
    // Pale lavender highlight must not carry light text.
    QCOMPARE(scheme.view.background.lightnessF() < 0.5, dark);
    QVERIFY(contrast(scheme.selection.normal, scheme.selection.background) >= 7.0);
}

void SystemPaletteTest::shouldMeetContrastForLightStaticPalette()
{
    QTemporaryDir dir;
    write(dir.filePath(u"Colors.qml"_s), lightQml);
    const auto palette = read(dir.path());
    QVERIFY(palette);
    QCOMPARE(palette->mode, u"light"_s);
    verifyScheme(map(*palette, Mode::Light));
    verifyScheme(map(*palette, Mode::Dark));
}

void SystemPaletteTest::shouldUseAlternateModeWhenSourceShipsBoth()
{
    QTemporaryDir dir;
    write(dir.filePath(u"noctalia.json"_s), noctaliaBoth);
    const auto palette = read(dir.path());
    QVERIFY(palette);
    const auto light = map(*palette, Mode::Light);
    verifyScheme(light);
    // Taken from the shipped light sub-palette, not synthesised from the dark one.
    QCOMPARE(light.view.background.name(), u"#fbf7f2"_s);
    verifyScheme(map(*palette, Mode::Dark));
}

void SystemPaletteTest::shouldSynthesizeLightFromDarkOnlyPalette()
{
    QTemporaryDir dir;
    write(dir.filePath(u"palette.json"_s), islandJson);
    const auto light = map(*read(dir.path()), Mode::Light);
    QVERIFY(light.view.background.lightnessF() > 0.9);
    // The pastel accent (about 2:1 on white) is darkened enough to be usable as a link.
    QVERIFY(contrast(light.view.link, light.view.background) >= 4.5);
}

void SystemPaletteTest::shouldRejectBrokenPalette()
{
    QTemporaryDir dir;
    write(dir.filePath(u"palette.json"_s), "{\"mode\":\"dark\",\"base\":");
    QVERIFY(!read(dir.path()));
    QVERIFY(!read(dir.filePath(u"missing"_s)));
}

void SystemPaletteTest::shouldFollowSymlinkRetargets()
{
    QTemporaryDir dir;
    QVERIFY(QDir().mkpath(dir.filePath(u"dark"_s)));
    QVERIFY(QDir().mkpath(dir.filePath(u"light"_s)));
    write(dir.filePath(u"dark/palette.json"_s), islandJson);
    write(dir.filePath(u"light/Colors.qml"_s), lightQml);
    const auto link = dir.filePath(u"active"_s);
    QVERIFY(QFile::link(dir.filePath(u"dark"_s), link));

    DrawyGlobalConfig::setSystemPaletteSource(link);
    const QPalette original = qApp->palette();
    SystemSchemeController controller;
    controller.activate(Mode::Dark);
    const QColor first = qApp->palette().color(QPalette::Base);
    QVERIFY(first.lightnessF() < 0.5);

    // theme-apply style switch: replace the symlink atomically with another theme.
    QVERIFY(QFile::link(dir.filePath(u"light"_s), dir.filePath(u"active.tmp"_s)));
    QCOMPARE(std::rename(QFile::encodeName(dir.filePath(u"active.tmp"_s)).constData(), QFile::encodeName(link).constData()), 0);
    QTRY_VERIFY_WITH_TIMEOUT(qApp->palette().color(QPalette::Base) != first, 3000);

    // A half-written palette keeps the last good colors.
    const QColor second = qApp->palette().color(QPalette::Base);
    write(dir.filePath(u"light/Colors.qml"_s), "");
    QTest::qWait(900);
    QCOMPARE(qApp->palette().color(QPalette::Base), second);

    controller.deactivate();
    qApp->setPalette(original);
    DrawyGlobalConfig::setSystemPaletteSource(QString());
}

void SystemPaletteTest::shouldMapEveryInstalledShellTheme()
{
    const QString themes = QDir::homePath() + u"/.config/themes"_s;
    if (!QDir(themes).exists()) {
        QSKIP("No shell themes installed");
    }
    QStringList folders;
    for (const auto &entry : QDir(themes).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
        folders.append(entry.absoluteFilePath());
        if (entry.fileName() == u".dynamic"_s) {
            for (const auto &dynamic : QDir(entry.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                folders.append(dynamic.absoluteFilePath());
            }
        }
    }
    int checked = 0;
    for (const auto &folder : std::as_const(folders)) {
        const auto palette = read(folder);
        if (!palette) {
            continue;
        }
        ++checked;
        qInfo() << folder << palette->mode;
        verifyScheme(map(*palette, Mode::Dark));
        verifyScheme(map(*palette, Mode::Light));
    }
    QVERIFY(checked > 0);
}

void SystemPaletteTest::shouldKeepMenuConsistentAcrossSchemeChoices()
{
    QTemporaryDir dir;
    write(dir.filePath(u"palette.json"_s), islandJson);
    DrawyGlobalConfig::setSystemPaletteSource(dir.path());
    DrawyGlobalConfig::setSystemScheme(QString());
    const QPalette original = qApp->palette();

    SystemSchemeController controller;
    auto schemeMenu = KColorSchemeMenu::createMenu(KColorSchemeManager::instance(), nullptr);
    SystemSchemeMenu::install(schemeMenu, &controller);
    auto menu = schemeMenu->menu();
    Q_EMIT menu->aboutToShow();

    const auto actions = menu->actions();
    QVERIFY(actions.size() >= 5);
    QVERIFY(actions.at(1)->text().contains(u"System Dark"_s));
    QVERIFY(actions.at(2)->text().contains(u"System Light"_s));
    QVERIFY(actions.at(3)->isSeparator());
    QAction *kde = nullptr;
    for (auto action : actions) {
        if (action->text().contains(u"Breeze Dark"_s)) {
            kde = action;
        }
    }
    QVERIFY(kde);
    const auto checkedCount = [&]() {
        int count = 0;
        for (auto action : menu->actions()) {
            count += action->isChecked();
        }
        return count;
    };

    kde->trigger();
    const QColor kdeBase = qApp->palette().color(QPalette::Base);
    const QString kdePath = qApp->property("KDE_COLOR_SCHEME_PATH").toString();
    QVERIFY(!controller.mode());

    actions.at(1)->trigger();
    QVERIFY(controller.mode() == Mode::Dark);
    QVERIFY(qApp->palette().color(QPalette::Base) != kdeBase);
    QCOMPARE(checkedCount(), 1);
    QVERIFY(actions.at(1)->isChecked());

    // Re-picking the scheme that was active before must really switch back.
    kde->trigger();
    QVERIFY(!controller.mode());
    QCOMPARE(qApp->palette().color(QPalette::Base), kdeBase);
    QCOMPARE(qApp->property("KDE_COLOR_SCHEME_PATH").toString(), kdePath);
    QCOMPARE(checkedCount(), 1);
    QVERIFY(kde->isChecked());
    QVERIFY(!actions.at(1)->isChecked());

    actions.at(2)->trigger();
    QVERIFY(controller.mode() == Mode::Light);
    QCOMPARE(checkedCount(), 1);

    controller.deactivate();
    qApp->setPalette(original);
    DrawyGlobalConfig::setSystemPaletteSource(QString());
}

#include "moc_systempalettetest.cpp"
