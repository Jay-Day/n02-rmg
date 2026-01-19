#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>

class KailleraServerSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit KailleraServerSelectDialog(QWidget* parent = nullptr);
    ~KailleraServerSelectDialog() override;

private slots:
    void onConnectClicked();
    void onCustomIpClicked();
    void onMasterListClicked();
    void onAddServer();
    void onEditServer();
    void onDeleteServer();
    void onPingServer();
    void onServerDoubleClicked(QListWidgetItem* item);
    void onServerSelected();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    void loadServerList();
    void saveServerList();
    void updateServerList();
    void connectToServer(const QString& name, const QString& host, int port);

    // UI elements
    QListWidget* serverList;
    QLineEdit* usernameEdit;
    QComboBox* connectionCombo;
    QSpinBox* frameDelaySpinBox;
    QPushButton* connectButton;
    QPushButton* pingButton;

    // Data
    struct ServerEntry {
        QString name;
        QString address;
    };
    QList<ServerEntry> servers;
};

// Connection type names
extern const char* CONNECTION_TYPES[];
