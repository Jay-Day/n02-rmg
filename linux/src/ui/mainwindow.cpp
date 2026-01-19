#include "mainwindow.h"
#include "../common/settings.h"
#include "p2p/p2p_selection_dialog.h"
#include "kaillera/kaillera_server_select_dialog.h"
#include <QApplication>
#include <QStyle>
#include <QScreen>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    loadSettings();
}

MainWindow::~MainWindow() {
    saveSettings();
}

void MainWindow::setupUi() {
    setWindowTitle("N02 Netplay Client");
    setFixedSize(300, 200);

    // Center on screen
    setGeometry(
        QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignCenter,
            size(),
            QGuiApplication::primaryScreen()->availableGeometry()
        )
    );

    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* layout = new QVBoxLayout(centralWidget);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 20, 20, 20);

    // Title
    titleLabel = new QLabel("N02 Netplay Client", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    // Version
    versionLabel = new QLabel("v0.6.0-linux", this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet("color: gray;");
    layout->addWidget(versionLabel);

    layout->addStretch();

    // Mode selector
    auto* modeLayout = new QHBoxLayout();
    auto* modeLabel = new QLabel("Mode:", this);
    modeCombo = new QComboBox(this);
    modeCombo->addItem("P2P");
    modeCombo->addItem("Kaillera Client");
    modeCombo->addItem("Playback");
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(modeCombo, 1);
    layout->addLayout(modeLayout);

    layout->addStretch();

    // Start button
    startButton = new QPushButton("Start", this);
    startButton->setMinimumHeight(35);
    layout->addWidget(startButton);

    // Connections
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);
    connect(startButton, &QPushButton::clicked,
            this, &MainWindow::onStartClicked);
}

void MainWindow::loadSettings() {
    int mode = Settings::getInt("main/mode", 0);
    modeCombo->setCurrentIndex(mode);
}

void MainWindow::saveSettings() {
    Settings::setInt("main/mode", modeCombo->currentIndex());
}

void MainWindow::onModeChanged(int index) {
    emit modeSelected(index);
}

void MainWindow::onStartClicked() {
    int mode = modeCombo->currentIndex();

    switch (mode) {
        case 0: {
            // P2P mode
            P2PSelectionDialog dialog(this);
            hide();
            dialog.exec();
            show();
            break;
        }
        case 1: {
            // Kaillera mode
            KailleraServerSelectDialog dialog(this);
            hide();
            dialog.exec();
            show();
            break;
        }
        case 2: {
            // Playback mode - TODO
            break;
        }
    }
}
