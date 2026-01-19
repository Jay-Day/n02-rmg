#include "p2p_selection_dialog.h"
#include "p2p_connection_dialog.h"
#include "../../common/settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <pwd.h>
#include <unistd.h>

P2PSelectionDialog::P2PSelectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings();
    loadStoredUsers();
}

P2PSelectionDialog::~P2PSelectionDialog() {
    saveSettings();
    saveStoredUsers();
}

void P2PSelectionDialog::setupUi() {
    setWindowTitle("N02 P2P");
    setMinimumSize(450, 400);

    auto* mainLayout = new QVBoxLayout(this);

    // Tab widget
    tabWidget = new QTabWidget(this);

    // ===== Host Tab =====
    hostTab = new QWidget();
    auto* hostLayout = new QVBoxLayout(hostTab);

    auto* hostForm = new QFormLayout();
    gameCombo = new QComboBox();
    gameCombo->setEditable(true);
    hostForm->addRow("Game:", gameCombo);
    hostLayout->addLayout(hostForm);

    hostLayout->addStretch();

    auto* hostButton = new QPushButton("Host Game");
    hostButton->setMinimumHeight(35);
    connect(hostButton, &QPushButton::clicked, this, &P2PSelectionDialog::onHostClicked);
    hostLayout->addWidget(hostButton);

    tabWidget->addTab(hostTab, "Host");

    // ===== Connect Tab =====
    connectTab = new QWidget();
    auto* connectLayout = new QVBoxLayout(connectTab);

    // IP input
    auto* ipForm = new QFormLayout();
    ipEdit = new QLineEdit();
    ipEdit->setPlaceholderText("127.0.0.1:27886");
    ipForm->addRow("Address:", ipEdit);
    connectLayout->addLayout(ipForm);

    // Stored users
    auto* storedGroup = new QGroupBox("Stored Addresses");
    auto* storedLayout = new QVBoxLayout(storedGroup);

    storedUsersList = new QListWidget();
    storedUsersList->setAlternatingRowColors(true);
    connect(storedUsersList, &QListWidget::itemClicked,
            this, &P2PSelectionDialog::onStoredUserSelected);
    connect(storedUsersList, &QListWidget::itemDoubleClicked,
            this, &P2PSelectionDialog::onConnectClicked);
    storedLayout->addWidget(storedUsersList);

    auto* storedButtonLayout = new QHBoxLayout();
    addButton = new QPushButton("Add");
    editButton = new QPushButton("Edit");
    deleteButton = new QPushButton("Delete");
    connect(addButton, &QPushButton::clicked, this, &P2PSelectionDialog::onAddStoredUser);
    connect(editButton, &QPushButton::clicked, this, &P2PSelectionDialog::onEditStoredUser);
    connect(deleteButton, &QPushButton::clicked, this, &P2PSelectionDialog::onDeleteStoredUser);
    storedButtonLayout->addWidget(addButton);
    storedButtonLayout->addWidget(editButton);
    storedButtonLayout->addWidget(deleteButton);
    storedLayout->addLayout(storedButtonLayout);

    connectLayout->addWidget(storedGroup);

    auto* connectButton = new QPushButton("Connect");
    connectButton->setMinimumHeight(35);
    connect(connectButton, &QPushButton::clicked, this, &P2PSelectionDialog::onConnectClicked);
    connectLayout->addWidget(connectButton);

    tabWidget->addTab(connectTab, "Connect");

    mainLayout->addWidget(tabWidget);

    // ===== Common Settings =====
    auto* commonGroup = new QGroupBox("Settings");
    auto* commonForm = new QFormLayout(commonGroup);

    usernameEdit = new QLineEdit();
    // Get system username as default
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        usernameEdit->setText(QString::fromUtf8(pw->pw_name));
    }
    commonForm->addRow("Username:", usernameEdit);

    auto* portLayout = new QHBoxLayout();
    portSpinBox = new QSpinBox();
    portSpinBox->setRange(1024, 65535);
    portSpinBox->setValue(27886);
    portLayout->addWidget(portSpinBox);

    randomPortCheck = new QCheckBox("Random (client)");
    portLayout->addWidget(randomPortCheck);
    commonForm->addRow("Port:", portLayout);

    mainLayout->addWidget(commonGroup);

    // Load tab setting
    connect(tabWidget, &QTabWidget::currentChanged, this, &P2PSelectionDialog::onTabChanged);
}

void P2PSelectionDialog::loadSettings() {
    int tab = Settings::getInt("p2p/tab", 0);
    tabWidget->setCurrentIndex(tab);

    usernameEdit->setText(Settings::getString("p2p/username", usernameEdit->text()));
    portSpinBox->setValue(Settings::getInt("p2p/port", 27886));
    ipEdit->setText(Settings::getString("p2p/ip", "127.0.0.1:27886"));
    randomPortCheck->setChecked(Settings::getInt("p2p/randomPort", 1) != 0);

    QString lastGame = Settings::getString("p2p/game", "");
    if (!lastGame.isEmpty()) {
        gameCombo->setCurrentText(lastGame);
    }
}

void P2PSelectionDialog::saveSettings() {
    Settings::setInt("p2p/tab", tabWidget->currentIndex());
    Settings::setString("p2p/username", usernameEdit->text());
    Settings::setInt("p2p/port", portSpinBox->value());
    Settings::setString("p2p/ip", ipEdit->text());
    Settings::setInt("p2p/randomPort", randomPortCheck->isChecked() ? 1 : 0);
    Settings::setString("p2p/game", gameCombo->currentText());
}

void P2PSelectionDialog::loadStoredUsers() {
    storedUsers.clear();
    int count = Settings::getInt("p2p/storedCount", 0);
    for (int i = 0; i < count; i++) {
        StoredUser user;
        user.name = Settings::getString(QString("p2p/stored%1_name").arg(i), "");
        user.address = Settings::getString(QString("p2p/stored%1_addr").arg(i), "");
        if (!user.name.isEmpty()) {
            storedUsers.append(user);
        }
    }
    updateStoredUsersList();
}

void P2PSelectionDialog::saveStoredUsers() {
    Settings::setInt("p2p/storedCount", storedUsers.size());
    for (int i = 0; i < storedUsers.size(); i++) {
        Settings::setString(QString("p2p/stored%1_name").arg(i), storedUsers[i].name);
        Settings::setString(QString("p2p/stored%1_addr").arg(i), storedUsers[i].address);
    }
}

void P2PSelectionDialog::updateStoredUsersList() {
    storedUsersList->clear();
    for (const auto& user : storedUsers) {
        storedUsersList->addItem(QString("%1 (%2)").arg(user.name, user.address));
    }
}

void P2PSelectionDialog::onHostClicked() {
    QString game = gameCombo->currentText().trimmed();
    if (game.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a game name.");
        return;
    }
    startConnection(true);
}

void P2PSelectionDialog::onConnectClicked() {
    QString ip = ipEdit->text().trimmed();
    if (ip.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter an IP address.");
        return;
    }
    startConnection(false);
}

void P2PSelectionDialog::startConnection(bool isHost) {
    QString username = usernameEdit->text().trimmed();
    if (username.isEmpty()) {
        username = "Player";
    }

    int port = portSpinBox->value();
    if (!isHost && randomPortCheck->isChecked()) {
        port = 0;  // Let system assign port
    }

    QString game = isHost ? gameCombo->currentText() : "";
    QString ip = isHost ? "" : ipEdit->text();

    P2PConnectionDialog dialog(isHost, game, ip, username, port, this);
    hide();
    dialog.exec();
    show();
}

void P2PSelectionDialog::onAddStoredUser() {
    bool ok;
    QString name = QInputDialog::getText(this, "Add Address",
        "Name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    QString addr = QInputDialog::getText(this, "Add Address",
        "Address (IP:port):", QLineEdit::Normal, "127.0.0.1:27886", &ok);
    if (!ok || addr.isEmpty()) return;

    StoredUser user;
    user.name = name;
    user.address = addr;
    storedUsers.append(user);
    updateStoredUsersList();
}

void P2PSelectionDialog::onEditStoredUser() {
    int row = storedUsersList->currentRow();
    if (row < 0 || row >= storedUsers.size()) return;

    bool ok;
    QString name = QInputDialog::getText(this, "Edit Address",
        "Name:", QLineEdit::Normal, storedUsers[row].name, &ok);
    if (!ok || name.isEmpty()) return;

    QString addr = QInputDialog::getText(this, "Edit Address",
        "Address (IP:port):", QLineEdit::Normal, storedUsers[row].address, &ok);
    if (!ok || addr.isEmpty()) return;

    storedUsers[row].name = name;
    storedUsers[row].address = addr;
    updateStoredUsersList();
}

void P2PSelectionDialog::onDeleteStoredUser() {
    int row = storedUsersList->currentRow();
    if (row < 0 || row >= storedUsers.size()) return;

    storedUsers.removeAt(row);
    updateStoredUsersList();
}

void P2PSelectionDialog::onStoredUserSelected() {
    int row = storedUsersList->currentRow();
    if (row >= 0 && row < storedUsers.size()) {
        ipEdit->setText(storedUsers[row].address);
    }
}

void P2PSelectionDialog::onTabChanged(int index) {
    (void)index;
    // Could update UI based on tab
}
