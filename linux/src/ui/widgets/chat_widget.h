#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QColor>

class ChatWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatWidget(QWidget* parent = nullptr);

    // Append messages with different styles
    void appendChat(const QString& nick, const QString& message,
                    const QColor& nickColor = QColor(0, 100, 200));
    void appendSystemMessage(const QString& message,
                             const QColor& color = QColor(128, 128, 128));
    void appendError(const QString& message);
    void appendInfo(const QString& message);

    // Clear all messages
    void clear();

    // Get input text
    QString getInputText() const;
    void clearInput();

    // Enable/disable input
    void setInputEnabled(bool enabled);

signals:
    void messageSent(const QString& message);

private slots:
    void onSendPressed();

private:
    void setupUi();

    QTextEdit* chatDisplay;
    QLineEdit* chatInput;
};
