/*
 * SPDX-FileCopyrightText: 2026 Drawy contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>

class SystemPaletteTest : public QObject
{
    Q_OBJECT
public:
    explicit SystemPaletteTest(QObject *parent = nullptr);

private Q_SLOTS:
    void shouldMeetContrastForDarkIslandPalette_data();
    void shouldMeetContrastForDarkIslandPalette();
    void shouldMeetContrastForLightStaticPalette();
    void shouldUseAlternateModeWhenSourceShipsBoth();
    void shouldSynthesizeLightFromDarkOnlyPalette();
    void shouldRejectBrokenPalette();
    void shouldFollowSymlinkRetargets();
    void shouldMapEveryInstalledShellTheme();
    void shouldKeepMenuConsistentAcrossSchemeChoices();
};
