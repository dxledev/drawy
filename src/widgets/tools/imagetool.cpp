/*
 * SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "imagetool.hpp"
#include "command/commandhistory.hpp"
#include "command/insertitemcommand.hpp"
#include "context/applicationcontext.hpp"
#include "context/coordinatetransformer.hpp"
#include "context/renderingcontext.hpp"
#include "context/spatialcontext.hpp"
#include "context/uicontext.hpp"
#include "dialog/filechooser.hpp"
#include "event/event.hpp"
#include "item/factory/imagefactory.hpp"
#include "item/image.hpp"
#include "properties/widgets/propertymanager.hpp"
#include <KLocalizedString>
#include <QImageReader>
#include <QList>
#include <QTimer>


using namespace Qt::Literals::StringLiterals;

ImageTool::ImageTool(ApplicationContext *context)
    : DrawingTool(context)
{
    m_itemFactory = std::make_unique<ImageFactory>();
    m_properties = {Property::Type::Opacity};
    const QList<QByteArray> supportedImage = QImageReader::supportedImageFormats();
    for (const QByteArray &ba : supportedImage) {
        m_patterns.append("*."_L1 + QString::fromLatin1(ba));
    }
}

QString ImageTool::icon() const
{
    return u"insert-image"_s;
}

Tool::Type ImageTool::type() const
{
    return Tool::Type::Image;
}

void ImageTool::mouseReleased(ApplicationContext *context)
{
    UIContext *uiContext{context->uiContext()};

    if (uiContext->appEvent()->button() != Qt::LeftButton) {
        return;
    }

    const QPointF lastPoint = uiContext->appEvent()->pos();

    // Don't run a modal dialog from inside the mouse event handler: the canvas keeps its
    // mouse grab and the app appears frozen. Defer until the event has been fully handled.
    QTimer::singleShot(0, context->parentWidget(), [this, context, lastPoint]() {
        chooseImage(context, lastPoint);
    });
}

void ImageTool::chooseImage(ApplicationContext *context, const QPointF &lastPoint)
{
    FileChooser::choose(context->parentWidget(),
                        FileChooser::Mode::OpenFile,
                        i18nc("@title:window", "Insert Image"),
                        i18n("Image"),
                        m_patterns,
                        {},
                        context->parentWidget(),
                        [this, context, lastPoint](const QString &fileName) {
                            insertImage(context, lastPoint, fileName);
                        });
}

void ImageTool::insertImage(ApplicationContext *context, const QPointF &lastPoint, const QString &fileName)
{
    if (fileName.isEmpty()) {
        return;
    }

    UIContext *uiContext{context->uiContext()};
    auto spatialContext{context->spatialContext()};
    auto renderingContext{context->renderingContext()};
    auto commandHistory{spatialContext->commandHistory()};
    CoordinateTransformer &transformer{spatialContext->coordinateTransformer()};

    QPixmap pixmap;
    if (!pixmap.load(fileName)) {
        return;
    }

    std::shared_ptr<ImageItem> curItem = std::dynamic_pointer_cast<ImageItem>(m_itemFactory->create());
    curItem->setBox(QRectF(transformer.viewToWorld(lastPoint), transformer.viewToWorld(QSizeF(100, 100))));

    curItem->setProperty(Property::Type::Opacity, uiContext->propertyManager()->value(Property::Type::Opacity));
    curItem->setPixmap(pixmap);

    QList<std::shared_ptr<Item>> lst;
    lst << curItem;
    commandHistory->insert(std::make_shared<InsertItemCommand>(lst));

    renderingContext->markForRender();
    renderingContext->markForUpdate();
}
