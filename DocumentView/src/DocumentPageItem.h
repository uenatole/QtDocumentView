#pragma once

#include "Document/API/DocumentParser.h"

#include <QGraphicsItem>

class DocumentFacade;
class DocumentLink;

class DocumentPageItem : public QGraphicsItem
{
public:
    struct Feedback
    {
        virtual ~Feedback() = default;
        virtual void linkPressed(const DocumentLink&) = 0;
    };

    DocumentPageItem(const std::shared_ptr<DocumentFacade>& document, Feedback* feedback, int number);
    ~DocumentPageItem() override;

    QRectF boundingRect() const override;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    void UpdateSelection(const DocumentSelection::Option& option, bool append = false);

    QString GetSelectedText() const;

    int Number() const;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

    void mouseMoveEvent(QGraphicsSceneMouseEvent* mouseEvent) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* mouseEvent) override;

private:
    void updateCurrentLink(const std::optional<DocumentLink>& link);
    void updateCursorShape(std::optional<QPointF> pos = std::nullopt);

    struct Private;
    std::unique_ptr<Private> d_ptr;
};
