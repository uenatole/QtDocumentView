#pragma once

#include <QObject>
#include <QPoint>
#include <QTimer>

class DocumentView;

class DocumentSelector : public QObject
{
public:
    explicit DocumentSelector(DocumentView* parent);

protected:
    bool eventFilter(QObject*, QEvent*) final;

private:
    void onPressed(QPoint);
    void onReleased(QPoint);
    void onMoved(QPoint) const;

    void handleClick(QPoint);
    void onDoubleClicked(QPoint) const;
    void onTripleClicked(QPoint) const;

    DocumentView* const m_view;

    QTimer m_clickTimer;
    int m_clickCount = 0;

    std::optional<QPointF> m_start;
};
