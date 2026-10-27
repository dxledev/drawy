/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "confirmmodal.hpp"
#include <QGuiApplication>
#include "modalmotion.hpp"

#include <QApplication>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

ConfirmModal::ConfirmModal(QWidget *parent)
    : QWidget(parent)
    , m_card(new QWidget(this))
    , m_title(new QLabel(m_card))
    , m_body(new QLabel(m_card))
    , m_confirmButton(new QPushButton(m_card))
    , m_cancelButton(new QPushButton(m_card))
    , m_motion(new ModalMotion(m_card))
    , m_tint(parent->palette().color(QPalette::Window))
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoSystemBackground);
    parent->installEventFilter(this);

    m_card->setObjectName(u"confirmCard"_s);
    m_card->setAttribute(Qt::WA_StyledBackground);
    m_card->setGraphicsEffect(m_motion);
    applyTheme();


    m_title->setObjectName(u"confirmTitle"_s);
    m_title->setWordWrap(true);
    m_body->setWordWrap(true);
    m_confirmButton->setObjectName(u"confirmPrimary"_s);
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->setCursor(Qt::PointingHandCursor);

    auto layout = new QVBoxLayout(m_card);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);
    layout->addWidget(m_title);
    layout->addWidget(m_body);
    auto buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addStretch();
    buttons->addWidget(m_cancelButton);
    buttons->addWidget(m_confirmButton);
    layout->addLayout(buttons);

    connect(m_motion, &ModalMotion::progressChanged, this, [this]() {
        update();
    });
    connect(m_motion, &ModalMotion::finished, this, [this](bool open) {
        if (!open && m_closing) {
            finishDismiss();
        }
    });
    connect(m_confirmButton, &QPushButton::clicked, this, [this]() {
        beginDismiss(true);
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        beginDismiss(false);
    });
    hide();
}

ConfirmModal::~ConfirmModal() = default;

void ConfirmModal::setTint(const QColor &color)
{
    m_tint = color;
    applyTheme();
}

void ConfirmModal::applyTheme()
{
    // Follow the canvas (light/dark/themed), not the widget palette, which may not match the selected scheme.
    const bool dark = m_tint.lightnessF() < 0.5;
    const QColor accent = QGuiApplication::palette().color(QPalette::Highlight);
    const QColor card = dark ? m_tint.lighter(135) : m_tint.darker(104);
    const QColor text = dark ? QColor(0xee, 0xee, 0xee) : QColor(0x1d, 0x1d, 0x1d);
    const QColor button = dark ? m_tint.lighter(170) : m_tint.darker(112);
    const QColor border = dark ? m_tint.lighter(210) : m_tint.darker(125);
    const QColor accentText = accent.lightnessF() < 0.55 ? QColor(Qt::white) : QColor(0x10, 0x10, 0x10);
    m_card->setStyleSheet(
        u"QWidget#confirmCard {background:%1;border:1px solid %3;border-radius:14px;}"
        u"QWidget#confirmCard QLabel {background:transparent;border:none;color:%2;}"
        u"QLabel#confirmTitle {font-size:20px;font-weight:600;}"
        u"QPushButton {min-height:20px;padding:9px 14px;background:%4;color:%2;border:1px solid %3;border-radius:7px;}"
        u"QPushButton:hover {border-color:%5;}"
        u"QPushButton:focus {border:2px solid %5;padding:8px 13px;}"
        u"QPushButton#confirmPrimary {background:%5;color:%6;border-color:%5;}"_s
            .arg(card.name(), text.name(), border.name(), button.name(), accent.name(), accentText.name()));
}

void ConfirmModal::ask(const QString &title, const QString &body, const QString &confirmText, const QString &cancelText, const std::function<void()> &onConfirm)
{
    if (isVisible()) {
        return;
    }
    connect(
        this,
        &ConfirmModal::chosen,
        this,
        [onConfirm](bool confirmed) {
            if (confirmed && onConfirm) {
                onConfirm();
            }
        },
        Qt::SingleShotConnection);

    m_previousFocus = QApplication::focusWidget();
    m_motion->reset();
    m_closing = false;
    m_card->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_title->setText(title);
    m_body->setText(body);
    m_confirmButton->setText(confirmText);
    m_cancelButton->setText(cancelText);

    // Grab before showing, so the snapshot does not contain the modal itself.
    setGeometry(parentWidget()->rect());
    centerCard();
    refreshSnapshot();
    show();
    raise();
    qApp->installEventFilter(this);
    // The destructive action is never the default.
    m_cancelButton->setFocus();
    m_motion->animate(true);
}

void ConfirmModal::centerCard()
{
    m_card->setFixedWidth(std::min(440, std::max(200, width() - 40)));
    m_card->layout()->activate();
    m_card->adjustSize();
    m_card->move((width() - m_card->width()) / 2, (height() - m_card->height()) / 2);
}

void ConfirmModal::refreshSnapshot()
{
    m_backgroundFrame = parentWidget()->grab();
    if (m_backgroundFrame.isNull()) {
        return;
    }
    QSize small = (m_backgroundFrame.deviceIndependentSize().toSize() / 3).expandedTo(QSize(1, 1));
    if (small.width() > 640 || small.height() > 640) {
        small.scale(640, 640, Qt::KeepAspectRatio);
    }
    const QImage source = m_backgroundFrame.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).toImage().convertToFormat(QImage::Format_RGB32);
    if (source.isNull()) {
        return;
    }

    // Replicate the edges so the blur does not darken the borders.
    constexpr int padding = 32;
    QImage extended(source.size() + QSize(padding * 2, padding * 2), QImage::Format_RGB32);
    for (int y = 0; y < extended.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(source.constScanLine(std::clamp(y - padding, 0, source.height() - 1)));
        auto *out = reinterpret_cast<QRgb *>(extended.scanLine(y));
        std::fill_n(out, padding, row[0]);
        std::copy_n(row, source.width(), out + padding);
        std::fill_n(out + padding + source.width(), padding, row[source.width() - 1]);
    }

    QGraphicsScene scene;
    auto item = scene.addPixmap(QPixmap::fromImage(extended));
    auto blur = new QGraphicsBlurEffect;
    blur->setBlurRadius(5);
    blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
    item->setGraphicsEffect(blur);
    QPixmap blurred(small);
    blurred.fill(Qt::transparent);
    QPainter blurPainter(&blurred);
    scene.render(&blurPainter, QRectF(QPointF(), small), QRectF(QPointF(padding, padding), small));
    blurPainter.end();

    const qreal dpr = devicePixelRatioF();
    m_snapshot = QPixmap(QSize(qCeil(width() * dpr), qCeil(height() * dpr)));
    m_snapshot.setDevicePixelRatio(dpr);
    m_snapshot.fill(Qt::transparent);
    QPainter p(&m_snapshot);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawPixmap(rect(), blurred);
    QColor tint = m_tint;
    tint.setAlphaF(.7);
    p.fillRect(rect(), tint);
}

void ConfirmModal::beginDismiss(bool confirmed)
{
    if (!isVisible() || m_closing) {
        return;
    }
    m_closing = true;
    m_pendingChoice = confirmed;
    m_card->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_motion->animate(false);
}

void ConfirmModal::finishDismiss()
{
    const bool confirmed = m_pendingChoice;
    m_pendingChoice = false;
    m_closing = false;
    qApp->removeEventFilter(this);
    hide();
    m_snapshot = QPixmap();
    m_backgroundFrame = QPixmap();
    if (m_previousFocus && m_previousFocus->isEnabled() && m_previousFocus->isVisible()) {
        m_previousFocus->setFocus();
    }
    m_previousFocus.clear();
    Q_EMIT chosen(confirmed);
}

bool ConfirmModal::eventFilter(QObject *object, QEvent *event)
{
    if (object == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        setGeometry(parentWidget()->rect());
    }
    if (isVisible() && event->type() == QEvent::ShortcutOverride) {
        // Keep the window's single-key shortcuts (E, Delete, Ctrl+Z...) off the canvas behind the modal.
        auto widget = qobject_cast<QWidget *>(object);
        if (widget && widget->window() == window()) {
            event->accept();
            return true;
        }
    }
    if (m_closing && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease || event->type() == QEvent::Wheel)) {
        return true;
    }
    return QWidget::eventFilter(object, event);
}

bool ConfirmModal::focusNextPrevChild(bool next)
{
    // Keep Tab inside the card; the widgets behind it are still enabled.
    Q_UNUSED(next)
    QPushButton *target = m_confirmButton->hasFocus() ? m_cancelButton : m_confirmButton;
    target->setFocus(Qt::TabFocusReason);
    return true;
}

void ConfirmModal::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawPixmap(rect(), m_backgroundFrame);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setOpacity(m_motion->progress());
    p.drawPixmap(rect(), m_snapshot);
}

void ConfirmModal::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    centerCard();
}

void ConfirmModal::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        beginDismiss(false);
    }
    // Swallow everything else: nothing behind the modal should react.
    event->accept();
}

void ConfirmModal::mousePressEvent(QMouseEvent *event)
{
    if (!m_closing && !m_card->geometry().contains(event->position().toPoint())) {
        beginDismiss(false);
    }
    event->accept();
}

#include "moc_confirmmodal.cpp"
