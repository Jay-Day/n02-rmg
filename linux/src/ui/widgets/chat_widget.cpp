#include "chat_widget.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QDateTime>

ChatWidget::ChatWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ChatWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Chat display area
    chatDisplay = new QTextEdit();
    chatDisplay->setReadOnly(true);
    chatDisplay->setMinimumHeight(150);
    layout->addWidget(chatDisplay, 1);

    // Input area
    auto* inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(0, 0, 0, 0);

    chatInput = new QLineEdit();
    chatInput->setPlaceholderText("Type message and press Enter...");
    connect(chatInput, &QLineEdit::returnPressed, this, &ChatWidget::onSendPressed);
    inputLayout->addWidget(chatInput);

    auto* sendButton = new QPushButton("Send");
    connect(sendButton, &QPushButton::clicked, this, &ChatWidget::onSendPressed);
    inputLayout->addWidget(sendButton);

    layout->addLayout(inputLayout);
}

void ChatWidget::appendChat(const QString& nick, const QString& message, const QColor& nickColor) {
    QString html = QString("<span style='color: %1; font-weight: bold;'>&lt;%2&gt;</span> %3<br>")
        .arg(nickColor.name(), nick.toHtmlEscaped(), message.toHtmlEscaped());
    chatDisplay->insertHtml(html);
    chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
}

void ChatWidget::appendSystemMessage(const QString& message, const QColor& color) {
    QString html = QString("<span style='color: %1;'>%2</span><br>")
        .arg(color.name(), message.toHtmlEscaped());
    chatDisplay->insertHtml(html);
    chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
}

void ChatWidget::appendError(const QString& message) {
    appendSystemMessage("ERROR: " + message, QColor(200, 0, 0));
}

void ChatWidget::appendInfo(const QString& message) {
    appendSystemMessage(message, QColor(0, 128, 0));
}

void ChatWidget::clear() {
    chatDisplay->clear();
}

QString ChatWidget::getInputText() const {
    return chatInput->text();
}

void ChatWidget::clearInput() {
    chatInput->clear();
}

void ChatWidget::setInputEnabled(bool enabled) {
    chatInput->setEnabled(enabled);
}

void ChatWidget::onSendPressed() {
    QString text = chatInput->text().trimmed();
    if (!text.isEmpty()) {
        emit messageSent(text);
        chatInput->clear();
    }
}
