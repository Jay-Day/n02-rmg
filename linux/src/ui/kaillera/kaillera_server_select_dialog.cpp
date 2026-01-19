#include "kaillera_server_select_dialog.h"
#include "kaillera_server_dialog.h"
#include "kaillera_master_dialog.h"
#include "../../common/settings.h"
#include "../../core/kaillera/kaillera_core.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <pwd.h>
#include <unistd.h>

const char* CONNECTION_TYPES[] = {
    "",
    "LAN",
    "Excellent",
    "Good",
    "Average",
    "Low",
    "Bad"
};

KailleraServerSelectDialog::KailleraServerSelectDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings();
    loadServerList();
}

KailleraServerSelectDialog::~KailleraServerSelectDialog() {
    saveSettings();
    saveServerList();
}

void KailleraServerSelectDialog::setupUi() {
    setWindowTitle("N02 Kaillera Client");
    setMinimumSize(500, 450);

    auto* mainLayout = new QVBoxLayout(this);

    // Server list
    auto* serverGroup = new QGroupBox("Servers");
    auto* serverLayout = new QVBoxLayout(serverGroup);

    serverList = new QListWidget();
    serverList->setAlternatingRowColors(true);
    connect(serverList, &QListWidget::itemDoubleClicked,
            this, &KailleraServerSelectDialog::onServerDoubleClicked);
    connect(serverList, &QListWidget::itemClicked,
            this, [this](QListWidgetItem*) { onServerSelected(); });
    serverLayout->addWidget(serverList);

    auto* serverButtonLayout = new QHBoxLayout();
    auto* addButton = new QPushButton("Add");
    auto* editButton = new QPushButton("Edit");
    auto* deleteButton = new QPushButton("Delete");
    pingButton = new QPushButton("Ping");
    connect(addButton, &QPushButton::clicked, this, &KailleraServerSelectDialog::onAddServer);
    connect(editButton, &QPushButton::clicked, this, &KailleraServerSelectDialog::onEditServer);
    connect(deleteButton, &QPushButton::clicked, this, &KailleraServerSelectDialog::onDeleteServer);
    connect(pingButton, &QPushButton::clicked, this, &KailleraServerSelectDialog::onPingServer);
    serverButtonLayout->addWidget(addButton);
    serverButtonLayout->addWidget(editButton);
    serverButtonLayout->addWidget(deleteButton);
    serverButtonLayout->addWidget(pingButton);
    serverLayout->addLayout(serverButtonLayout);

    mainLayout->addWidget(serverGroup);

    // Settings
    auto* settingsGroup = new QGroupBox("Connection Settings");
    auto* settingsForm = new QFormLayout(settingsGroup);

    usernameEdit = new QLineEdit();
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        usernameEdit->setText(QString::fromUtf8(pw->pw_name));
    }
    settingsForm->addRow("Username:", usernameEdit);

    connectionCombo = new QComboBox();
    connectionCombo->addItem("LAN (60 packets/s)");
    connectionCombo->addItem("Excellent (30 packets/s)");
    connectionCombo->addItem("Good (20 packets/s)");
    connectionCombo->addItem("Average (15 packets/s)");
    connectionCombo->addItem("Low (12 packets/s)");
    connectionCombo->addItem("Bad (10 packets/s)");
    settingsForm->addRow("Connection:", connectionCombo);

    frameDelaySpinBox = new QSpinBox();
    frameDelaySpinBox->setRange(0, 10);
    frameDelaySpinBox->setValue(0);
    frameDelaySpinBox->setSpecialValueText("Auto");
    settingsForm->addRow("Frame Delay:", frameDelaySpinBox);

    mainLayout->addWidget(settingsGroup);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    auto* customButton = new QPushButton("Custom IP...");
    connect(customButton, &QPushButton::clicked, this, &KailleraServerSelectDialog::onCustomIpClicked);
    buttonLayout->addWidget(customButton);

    auto* masterButton = new QPushButton("Master List...");
    connect(masterButton, &QPushButton::clicked, this, &KailleraServerSelectDialog::onMasterListClicked);
    buttonLayout->addWidget(masterButton);

    buttonLayout->addStretch();

    connectButton = new QPushButton("Connect");
    connectButton->setMinimumWidth(100);
    connect(connectButton, &QPushButton::clicked, this, &KailleraServerSelectDialog::onConnectClicked);
    buttonLayout->addWidget(connectButton);

    mainLayout->addLayout(buttonLayout);
}

void KailleraServerSelectDialog::loadSettings() {
    usernameEdit->setText(Settings::getString("kaillera/username", usernameEdit->text()));
    connectionCombo->setCurrentIndex(Settings::getInt("kaillera/connection", 0));
    frameDelaySpinBox->setValue(Settings::getInt("kaillera/frameDelay", 0));
}

void KailleraServerSelectDialog::saveSettings() {
    Settings::setString("kaillera/username", usernameEdit->text());
    Settings::setInt("kaillera/connection", connectionCombo->currentIndex());
    Settings::setInt("kaillera/frameDelay", frameDelaySpinBox->value());
}

void KailleraServerSelectDialog::loadServerList() {
    servers.clear();
    int count = Settings::getInt("kaillera/serverCount", 0);
    for (int i = 0; i < count; i++) {
        ServerEntry entry;
        entry.name = Settings::getString(QString("kaillera/server%1_name").arg(i), "");
        entry.address = Settings::getString(QString("kaillera/server%1_addr").arg(i), "");
        if (!entry.name.isEmpty()) {
            servers.append(entry);
        }
    }
    updateServerList();
}

void KailleraServerSelectDialog::saveServerList() {
    Settings::setInt("kaillera/serverCount", servers.size());
    for (int i = 0; i < servers.size(); i++) {
        Settings::setString(QString("kaillera/server%1_name").arg(i), servers[i].name);
        Settings::setString(QString("kaillera/server%1_addr").arg(i), servers[i].address);
    }
}

void KailleraServerSelectDialog::updateServerList() {
    serverList->clear();
    for (const auto& server : servers) {
        serverList->addItem(QString("%1 (%2)").arg(server.name, server.address));
    }
}

void KailleraServerSelectDialog::onConnectClicked() {
    int row = serverList->currentRow();
    if (row < 0 || row >= servers.size()) {
        QMessageBox::warning(this, "Error", "Please select a server.");
        return;
    }

    const auto& server = servers[row];
    QString host = server.address;
    int port = 27888;

    int colonIdx = host.indexOf(':');
    if (colonIdx >= 0) {
        port = host.mid(colonIdx + 1).toInt();
        if (port == 0) port = 27888;
        host = host.left(colonIdx);
    }

    connectToServer(server.name, host, port);
}

void KailleraServerSelectDialog::onCustomIpClicked() {
    bool ok;
    QString addr = QInputDialog::getText(this, "Custom IP",
        "Enter server address (IP:port):", QLineEdit::Normal,
        "127.0.0.1:27888", &ok);

    if (!ok || addr.isEmpty()) return;

    QString host = addr;
    int port = 27888;

    int colonIdx = addr.indexOf(':');
    if (colonIdx >= 0) {
        port = addr.mid(colonIdx + 1).toInt();
        if (port == 0) port = 27888;
        host = addr.left(colonIdx);
    }

    connectToServer(host, host, port);
}

void KailleraServerSelectDialog::onMasterListClicked() {
    KailleraMasterDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        auto selected = dialog.getSelectedServers();
        for (const auto& server : selected) {
            // Check if already exists
            bool exists = false;
            for (const auto& s : servers) {
                if (s.address == server.address) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                ServerEntry entry;
                entry.name = server.name;
                entry.address = server.address;
                servers.append(entry);
            }
        }
        updateServerList();
    }
}

void KailleraServerSelectDialog::connectToServer(const QString& name, const QString& host, int port) {
    QString username = usernameEdit->text().trimmed();
    if (username.isEmpty()) {
        username = "Player";
    }

    int connSetting = connectionCombo->currentIndex() + 1;

    KailleraServerDialog dialog(name, host, port, username, connSetting, this);
    hide();
    dialog.exec();
    show();
}

void KailleraServerSelectDialog::onAddServer() {
    bool ok;
    QString name = QInputDialog::getText(this, "Add Server",
        "Server name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    QString addr = QInputDialog::getText(this, "Add Server",
        "Server address (IP:port):", QLineEdit::Normal, "127.0.0.1:27888", &ok);
    if (!ok || addr.isEmpty()) return;

    ServerEntry entry;
    entry.name = name;
    entry.address = addr;
    servers.append(entry);
    updateServerList();
}

void KailleraServerSelectDialog::onEditServer() {
    int row = serverList->currentRow();
    if (row < 0 || row >= servers.size()) return;

    bool ok;
    QString name = QInputDialog::getText(this, "Edit Server",
        "Server name:", QLineEdit::Normal, servers[row].name, &ok);
    if (!ok || name.isEmpty()) return;

    QString addr = QInputDialog::getText(this, "Edit Server",
        "Server address:", QLineEdit::Normal, servers[row].address, &ok);
    if (!ok || addr.isEmpty()) return;

    servers[row].name = name;
    servers[row].address = addr;
    updateServerList();
}

void KailleraServerSelectDialog::onDeleteServer() {
    int row = serverList->currentRow();
    if (row < 0 || row >= servers.size()) return;

    servers.removeAt(row);
    updateServerList();
}

void KailleraServerSelectDialog::onPingServer() {
    int row = serverList->currentRow();
    if (row < 0 || row >= servers.size()) return;

    QString addr = servers[row].address;
    QString host = addr;
    int port = 27888;

    int colonIdx = addr.indexOf(':');
    if (colonIdx >= 0) {
        port = addr.mid(colonIdx + 1).toInt();
        if (port == 0) port = 27888;
        host = addr.left(colonIdx);
    }

    int ping = kaillera_ping_server(host.toUtf8().constData(), port);

    QString text = QString("%1 (%2) - %3ms")
        .arg(servers[row].name, servers[row].address)
        .arg(ping);
    serverList->item(row)->setText(text);
}

void KailleraServerSelectDialog::onServerDoubleClicked(QListWidgetItem*) {
    onConnectClicked();
}

void KailleraServerSelectDialog::onServerSelected() {
    // Could update status bar or preview
}
