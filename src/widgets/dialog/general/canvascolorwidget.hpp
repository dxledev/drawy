// SPDX-FileCopyrightText: 2026 Abdelhadi Wael <waelhadi380@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dialog/configgroupinterface.hpp"
#include "libdrawywidgets_private_export.h"
class KColorButton;
class QCheckBox;
class QComboBox;
class QLineEdit;

class LIBDRAWYWIDGETS_TESTS_EXPORT CanvasColorWidget : public ConfigGroupInterface
{
    Q_OBJECT
public:
    explicit CanvasColorWidget(QWidget *parent = nullptr);

    void save() override;
    void load() override;

private:
    KColorButton *const m_lightThemeColor;
    KColorButton *const m_darkThemeColor;
    QComboBox *const m_canvasMode;
    QCheckBox *const m_canvasThemed;
    QLineEdit *const m_paletteSource;
};
