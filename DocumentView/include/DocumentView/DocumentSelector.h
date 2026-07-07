#pragma once

#include <QTimer>

class DocumentView;

class DocumentSelector : public QObject
{
public:
    explicit DocumentSelector(DocumentView* parent);
    ~DocumentSelector() override;

protected:
    bool eventFilter(QObject*, QEvent*) final;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
