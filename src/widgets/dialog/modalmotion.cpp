/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "modalmotion.hpp"

#include <QPainter>
#include <QScreen>
#include <QWidget>

#include <algorithm>

ModalMotion::ModalMotion(QObject *parent)
    : QGraphicsEffect(parent)
{
    m_timeline.setParent(this);
    m_timeline.setDuration(duration);
    m_timeline.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_timeline, &QTimeLine::valueChanged, this, [this](qreal value) {
        setProgress(m_startProgress + ((m_targetOpen ? 1.0 : 0.0) - m_startProgress) * value);
    });
    connect(&m_timeline, &QTimeLine::finished, this, [this]() {
        Q_EMIT finished(m_targetOpen);
    });
}

void ModalMotion::setProgress(qreal value)
{
    m_progress = value;
    update();
    Q_EMIT progressChanged(value);
}

void ModalMotion::reset()
{
    m_timeline.stop();
    setProgress(0);
}

void ModalMotion::animate(bool open)
{
    m_timeline.stop();
    m_targetOpen = open;
    if (qEnvironmentVariableIntValue("DRAWY_REDUCED_MOTION") == 1) {
        setProgress(open ? 1 : 0);
        Q_EMIT finished(open);
        return;
    }
    m_startProgress = m_progress;
    const auto *widget = qobject_cast<QWidget *>(parent());
    const auto *screen = widget ? widget->screen() : nullptr;
    const double refresh = screen && screen->refreshRate() > 0 ? screen->refreshRate() : 60;
    m_timeline.setUpdateInterval(std::max(1, qRound(1000 / refresh)));
    m_timeline.start();
}

void ModalMotion::draw(QPainter *painter)
{
    if (m_progress <= 0) {
        return;
    }
    if (m_progress >= 1) {
        drawSource(painter);
        return;
    }
    QPoint offset;
    const QPixmap pixmap = sourcePixmap(Qt::LogicalCoordinates, &offset, NoPad);
    const QPointF center = sourceBoundingRect(Qt::LogicalCoordinates).center();
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setOpacity(painter->opacity() * m_progress);
    painter->translate(center);
    painter->scale(scale(), scale());
    painter->translate(-center);
    painter->drawPixmap(offset, pixmap);
    painter->restore();
}

#include "moc_modalmotion.cpp"
