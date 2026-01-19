#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    void modeSelected(int mode);

private slots:
    void onModeChanged(int index);
    void onStartClicked();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    QWidget* centralWidget;
    QComboBox* modeCombo;
    QPushButton* startButton;
    QLabel* titleLabel;
    QLabel* versionLabel;
};
