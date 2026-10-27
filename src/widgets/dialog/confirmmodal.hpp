/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QColor>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class ModalMotion;

/**
 * In-window confirmation card over a blurred, tinted snapshot of the window.
 * Covers its parent widget; Escape or a click outside the card cancels.
 */
class ConfirmModal : public QWidget
{
    Q_OBJECT
public:
    explicit ConfirmModal(QWidget *parent);
    ~ConfirmModal() override;

    /// Runs onConfirm only if the user picks the confirm button.
    void ask(const QString &title, const QString &body, const QString &confirmText, const QString &cancelText, const std::function<void()> &onConfirm);

    void setTint(const QColor &color);
    void applyTheme();

Q_SIGNALS:
    void chosen(bool confirmed);

protected:
    bool focusNextPrevChild(bool next) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void beginDismiss(bool confirmed);
    void finishDismiss();
    void refreshSnapshot();
    void centerCard();

    QPointer<QWidget> m_previousFocus;
    QWidget *const m_card;
    QLabel *const m_title;
    QLabel *const m_body;
    QPushButton *const m_confirmButton;
    QPushButton *const m_cancelButton;
    ModalMotion *const m_motion;
    QPixmap m_backgroundFrame;
    QPixmap m_snapshot;
    QColor m_tint;
    bool m_closing = false;
    bool m_pendingChoice = false;
};
