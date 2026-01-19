#include "kaillera_master_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

// Known master server URLs
static const char* MASTER_SERVERS[] = {
    "http://kaillerareborn.2manygames.fr/server_list.php",
    "http://www.anti3d.com/kaillera/server_list.php",
    nullptr
};

static const char* MASTER_NAMES[] = {
    "Kaillera Reborn (Recommended)",
    "Anti3D Master (Legacy)",
    nullptr
};

KailleraMasterDialog::KailleraMasterDialog(QWidget* parent)
    : QDialog(parent)
    , networkManager(new QNetworkAccessManager(this))
{
    setupUi();
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &KailleraMasterDialog::onNetworkReply);
}

KailleraMasterDialog::~KailleraMasterDialog() = default;

void KailleraMasterDialog::setupUi() {
    setWindowTitle("Kaillera Master Server List");
    setMinimumSize(700, 500);

    auto* mainLayout = new QVBoxLayout(this);

    // Master server selection
    auto* topLayout = new QHBoxLayout();
    auto* masterLabel = new QLabel("Master Server:", this);
    masterCombo = new QComboBox(this);
    for (int i = 0; MASTER_NAMES[i]; i++) {
        masterCombo->addItem(MASTER_NAMES[i]);
    }

    refreshButton = new QPushButton("Refresh", this);
    connect(refreshButton, &QPushButton::clicked, this, &KailleraMasterDialog::onRefresh);

    topLayout->addWidget(masterLabel);
    topLayout->addWidget(masterCombo, 1);
    topLayout->addWidget(refreshButton);
    mainLayout->addLayout(topLayout);

    // Filter
    auto* filterLayout = new QHBoxLayout();
    auto* filterLabel = new QLabel("Filter:", this);
    filterEdit = new QLineEdit(this);
    filterEdit->setPlaceholderText("Filter by name...");
    connect(filterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        for (int i = 0; i < serverTable->rowCount(); i++) {
            bool match = text.isEmpty() ||
                serverTable->item(i, 0)->text().contains(text, Qt::CaseInsensitive) ||
                serverTable->item(i, 1)->text().contains(text, Qt::CaseInsensitive);
            serverTable->setRowHidden(i, !match);
        }
    });
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(filterEdit, 1);
    mainLayout->addLayout(filterLayout);

    // Server table
    serverTable = new QTableWidget(this);
    serverTable->setColumnCount(6);
    serverTable->setHorizontalHeaderLabels({"Name", "Address", "Users", "Games", "Version", "Location"});
    serverTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    serverTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    serverTable->setAlternatingRowColors(true);
    serverTable->setSortingEnabled(true);
    serverTable->horizontalHeader()->setStretchLastSection(true);
    serverTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    serverTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(serverTable, &QTableWidget::cellDoubleClicked,
            this, &KailleraMasterDialog::onServerDoubleClicked);
    mainLayout->addWidget(serverTable, 1);

    // Progress and status
    progressBar = new QProgressBar(this);
    progressBar->setVisible(false);
    mainLayout->addWidget(progressBar);

    statusLabel = new QLabel("Select a master server and click Refresh", this);
    mainLayout->addWidget(statusLabel);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    addButton = new QPushButton("Add Selected", this);
    connect(addButton, &QPushButton::clicked, this, &KailleraMasterDialog::onAddSelected);
    buttonLayout->addWidget(addButton);

    auto* closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);
}

void KailleraMasterDialog::onRefresh() {
    int idx = masterCombo->currentIndex();
    if (idx < 0 || !MASTER_SERVERS[idx]) return;

    refreshButton->setEnabled(false);
    progressBar->setVisible(true);
    progressBar->setRange(0, 0); // Indeterminate
    statusLabel->setText("Fetching server list...");

    QNetworkRequest request(QUrl(MASTER_SERVERS[idx]));
    request.setHeader(QNetworkRequest::UserAgentHeader, "N02 Kaillera Client/0.6.0");
    networkManager->get(request);
}

void KailleraMasterDialog::onNetworkReply(QNetworkReply* reply) {
    refreshButton->setEnabled(true);
    progressBar->setVisible(false);

    if (reply->error() != QNetworkReply::NoError) {
        statusLabel->setText(QString("Error: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    parseServerList(data);
}

void KailleraMasterDialog::parseServerList(const QByteArray& data) {
    servers.clear();
    serverTable->setRowCount(0);

    // Kaillera Reborn format (two lines per server):
    // Line 1: Server name
    // Line 2: IP:port;users/max;games;version;location
    QList<QByteArray> lines = data.split('\n');

    for (int i = 0; i + 1 < lines.size(); i += 2) {
        QString name = QString::fromUtf8(lines[i].trimmed());
        QByteArray infoLine = lines[i + 1].trimmed();

        if (name.isEmpty() || infoLine.isEmpty()) continue;

        QList<QByteArray> parts = infoLine.split(';');
        if (parts.size() >= 5) {
            ServerInfo info;
            info.name = name;
            info.address = QString::fromUtf8(parts[0]); // IP:port

            // Parse users (format: "current/max")
            QString usersStr = QString::fromUtf8(parts[1]);
            int slashIdx = usersStr.indexOf('/');
            if (slashIdx >= 0) {
                info.users = usersStr.left(slashIdx).toInt();
            } else {
                info.users = usersStr.toInt();
            }

            info.games = parts[2].toInt();
            info.version = QString::fromUtf8(parts[3]);
            info.location = QString::fromUtf8(parts[4]);

            // Skip empty entries
            if (info.address.isEmpty()) continue;

            servers.append(info);

            int row = serverTable->rowCount();
            serverTable->insertRow(row);
            serverTable->setItem(row, 0, new QTableWidgetItem(info.name));
            serverTable->setItem(row, 1, new QTableWidgetItem(info.address));

            auto* usersItem = new QTableWidgetItem();
            usersItem->setData(Qt::DisplayRole, info.users);
            serverTable->setItem(row, 2, usersItem);

            auto* gamesItem = new QTableWidgetItem();
            gamesItem->setData(Qt::DisplayRole, info.games);
            serverTable->setItem(row, 3, gamesItem);

            serverTable->setItem(row, 4, new QTableWidgetItem(info.version));
            serverTable->setItem(row, 5, new QTableWidgetItem(info.location));
        }
    }

    serverTable->resizeColumnsToContents();
    statusLabel->setText(QString("Found %1 servers").arg(servers.size()));
}

void KailleraMasterDialog::onAddSelected() {
    selectedServers.clear();

    QList<QTableWidgetItem*> items = serverTable->selectedItems();
    QSet<int> rows;
    for (auto* item : items) {
        rows.insert(item->row());
    }

    for (int row : rows) {
        ServerInfo info;
        info.name = serverTable->item(row, 0)->text();
        info.address = serverTable->item(row, 1)->text();
        info.users = serverTable->item(row, 2)->data(Qt::DisplayRole).toInt();
        info.games = serverTable->item(row, 3)->data(Qt::DisplayRole).toInt();
        info.version = serverTable->item(row, 4)->text();
        info.location = serverTable->item(row, 5)->text();
        selectedServers.append(info);
    }

    if (selectedServers.isEmpty()) {
        QMessageBox::information(this, "No Selection", "Please select at least one server.");
        return;
    }

    accept();
}

void KailleraMasterDialog::onServerDoubleClicked(int row, int column) {
    Q_UNUSED(column);

    selectedServers.clear();
    ServerInfo info;
    info.name = serverTable->item(row, 0)->text();
    info.address = serverTable->item(row, 1)->text();
    info.users = serverTable->item(row, 2)->data(Qt::DisplayRole).toInt();
    info.games = serverTable->item(row, 3)->data(Qt::DisplayRole).toInt();
    info.version = serverTable->item(row, 4)->text();
    info.location = serverTable->item(row, 5)->text();
    selectedServers.append(info);

    accept();
}

QList<KailleraMasterDialog::ServerInfo> KailleraMasterDialog::getSelectedServers() const {
    return selectedServers;
}
