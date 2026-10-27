/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QGraphicsEffect>
#include <QTimeLine>

/**
 * Fade and slight scale applied to a modal card while it opens and closes.
 */
class ModalMotion : public QGraphicsEffect
{
    Q_OBJECT
public:
    static constexpr int duration = 120;

    explicit ModalMotion(QObject *parent = nullptr);

    [[nodiscard]] qreal progress() const
    {
        return m_progress;
    }
    [[nodiscard]] qreal scale() const
    {
        return .98 + .02 * m_progress;
    }

    void animate(bool open);
    void reset();

Q_SIGNALS:
    void progressChanged(qreal progress);
    void finished(bool open);

protected:
    void draw(QPainter *painter) override;

private:
    void setProgress(qreal value);

    QTimeLine m_timeline;
    qreal m_startProgress = 0;
    qreal m_progress = 0;
    bool m_targetOpen = false;
};
