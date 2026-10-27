// SPDX-FileCopyrightText: 2026 Drawy contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemschememenu.hpp"
#include "systemschemecontroller.hpp"

#include <KActionMenu>
#include <KLocalizedString>
#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

using SystemPalette::Mode;

namespace
{
QIcon swatch(const SystemPalette::Scheme &scheme)
{
    constexpr int size = 16;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(0, 0, size, size, scheme.window.normal);
    painter.fillRect(1, 1, size - 2, size - 2, scheme.window.background);
    painter.fillRect(size / 2, size / 2, size / 2 - 1, size / 2 - 1, scheme.selection.background);
    return QIcon(pixmap);
}
}

void SystemSchemeMenu::install(KActionMenu *schemeMenu, SystemSchemeController *controller)
{
    auto menu{schemeMenu->menu()};
    const QList<QAction *> existing{menu->actions()};
    // After "Default", before the KDE schemes. KColorSchemeMenu fills itself on creation.
    QAction *const before{existing.size() > 1 ? existing.at(1) : nullptr};
    // Sharing its group keeps exactly one entry checked, whichever menu item was used last.
    QActionGroup *group{existing.isEmpty() ? nullptr : existing.first()->actionGroup()};
    if (!group) {
        group = new QActionGroup{menu};
    }

    const auto addAction = [&](const QString &text) {
        auto action{new QAction{text, menu}};
        action->setCheckable(true);
        action->setActionGroup(group);
        menu->insertAction(before, action);
        return action;
    };
    auto dark{addAction(i18nc("@action:inmenu", "System Dark"))};
    auto light{addAction(i18nc("@action:inmenu", "System Light"))};
    menu->insertSeparator(before);

    const auto syncChecks = [controller, dark, light]() {
        const auto mode{controller->mode()};
        if (mode == Mode::Dark) {
            dark->setChecked(true);
        } else if (mode == Mode::Light) {
            light->setChecked(true);
        } else {
            dark->setChecked(false);
            light->setChecked(false);
        }
    };
    const auto refreshIcons = [controller, dark, light]() {
        if (const auto scheme{controller->preview(Mode::Dark)}) {
            dark->setIcon(swatch(*scheme));
        }
        if (const auto scheme{controller->preview(Mode::Light)}) {
            light->setIcon(swatch(*scheme));
        }
    };
    QObject::connect(controller, &SystemSchemeController::modeChanged, menu, syncChecks);
    QObject::connect(controller, &SystemSchemeController::schemeApplied, menu, refreshIcons);
    QObject::connect(menu, &QMenu::aboutToShow, menu, syncChecks);

    // Any other entry (Default or a KDE scheme) ends the live following, so the choice is honoured.
    QObject::connect(menu, &QMenu::triggered, menu, [controller, dark, light](QAction *action) {
        if (action == dark) {
            controller->activate(Mode::Dark);
        } else if (action == light) {
            controller->activate(Mode::Light);
        } else {
            controller->deactivate();
        }
    });
}
