// SPDX-FileCopyrightText: 2026 Abdelhadi Wael <waelhadi380@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "canvascolorwidget.hpp"
#include "drawyglobalconfig.h"

#include <KColorButton>
#include <KLocalizedString>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>

CanvasColorWidget::CanvasColorWidget(QWidget *parent)
    : ConfigGroupInterface{parent, i18n("Canvas")}
    , m_lightThemeColor{new KColorButton{DrawyGlobalConfig::backgroundColorLight(), this}}
    , m_darkThemeColor{new KColorButton{DrawyGlobalConfig::backgroundColorDark(), this}}
    , m_canvasMode{new QComboBox{this}}
    , m_paletteSource{new QLineEdit{this}}
{
    const auto widget{new QWidget{this}};
    const auto layout{new QFormLayout{widget}};
    layout->setContentsMargins(0, 0, 0, 0);

    m_canvasMode->addItem(i18n("Inherit scheme"), DrawyGlobalConfig::EnumCanvasMode::InheritScheme);
    m_canvasMode->addItem(i18n("Light"), DrawyGlobalConfig::EnumCanvasMode::Light);
    m_canvasMode->addItem(i18n("Dark"), DrawyGlobalConfig::EnumCanvasMode::Dark);
    m_canvasMode->setToolTip(i18n("Inherit scheme follows the light or dark mode of the active color scheme."));
    m_paletteSource->setClearButtonEnabled(true);
    m_paletteSource->setPlaceholderText(i18n("Auto-detect from the active shell theme"));
    m_paletteSource->setToolTip(i18n("Palette file or folder (Colors.qml, palette.json, noctalia.json, kitty.conf) followed by System Dark and System Light."));

    layout->addRow(i18n("Canvas color"), m_canvasMode);
    layout->addRow(i18n("Light theme color"), m_lightThemeColor);
    layout->addRow(i18n("Dark theme color"), m_darkThemeColor);
    layout->addRow(i18n("System palette"), m_paletteSource);

    m_layout->addWidget(widget);
}

void CanvasColorWidget::save()
{
    DrawyGlobalConfig::setBackgroundColorLight(m_lightThemeColor->color());
    DrawyGlobalConfig::setBackgroundColorDark(m_darkThemeColor->color());
    DrawyGlobalConfig::setCanvasMode(m_canvasMode->currentData().toInt());
    DrawyGlobalConfig::setSystemPaletteSource(m_paletteSource->text().trimmed());
    DrawyGlobalConfig::self()->save();
}

void CanvasColorWidget::load()
{
    m_lightThemeColor->setColor(DrawyGlobalConfig::backgroundColorLight());
    m_darkThemeColor->setColor(DrawyGlobalConfig::backgroundColorDark());
    m_canvasMode->setCurrentIndex(qMax(0, m_canvasMode->findData(DrawyGlobalConfig::canvasMode())));
    m_paletteSource->setText(DrawyGlobalConfig::systemPaletteSource());
}

#include "moc_canvascolorwidget.cpp"
