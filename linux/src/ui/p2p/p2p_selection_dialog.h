#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>

class P2PSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit P2PSelectionDialog(QWidget* parent = nullptr);
    ~P2PSelectionDialog() override;

private slots:
    void onHostClicked();
    void onConnectClicked();
    void onAddStoredUser();
    void onEditStoredUser();
    void onDeleteStoredUser();
    void onStoredUserSelected();
    void onTabChanged(int index);

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    void loadStoredUsers();
    void saveStoredUsers();
    void updateStoredUsersList();
    void startConnection(bool isHost);

    // Shared controls
    QTabWidget* tabWidget;
    QLineEdit* usernameEdit;
    QSpinBox* portSpinBox;

    // Host tab
    QWidget* hostTab;
    QComboBox* gameCombo;

    // Connect tab
    QWidget* connectTab;
    QLineEdit* ipEdit;
    QCheckBox* randomPortCheck;
    QListWidget* storedUsersList;
    QPushButton* addButton;
    QPushButton* editButton;
    QPushButton* deleteButton;

    // Data
    struct StoredUser {
        QString name;
        QString address;
    };
    QList<StoredUser> storedUsers;
};
