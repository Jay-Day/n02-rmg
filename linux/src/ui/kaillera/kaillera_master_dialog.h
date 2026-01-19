#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QComboBox>

class KailleraMasterDialog : public QDialog {
    Q_OBJECT

public:
    explicit KailleraMasterDialog(QWidget* parent = nullptr);
    ~KailleraMasterDialog() override;

    struct ServerInfo {
        QString name;
        QString address;
        int users;
        int games;
        QString version;
        QString location;
    };

    QList<ServerInfo> getSelectedServers() const;

private slots:
    void onRefresh();
    void onAddSelected();
    void onNetworkReply(QNetworkReply* reply);
    void onServerDoubleClicked(int row, int column);

private:
    void setupUi();
    void parseServerList(const QByteArray& data);

    QComboBox* masterCombo;
    QTableWidget* serverTable;
    QLineEdit* filterEdit;
    QPushButton* refreshButton;
    QPushButton* addButton;
    QProgressBar* progressBar;
    QLabel* statusLabel;

    QNetworkAccessManager* networkManager;
    QList<ServerInfo> servers;
    QList<ServerInfo> selectedServers;
};
