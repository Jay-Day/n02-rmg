#include "settings.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

QSettings* Settings::s_settings = nullptr;
QString Settings::s_group;

void Settings::initialize(const QString& subModule) {
    if (s_settings) {
        delete s_settings;
    }

    // Store settings in ~/.config/n02/n02.ini or next to executable
    QString configPath = QDir::homePath() + "/.config/n02";
    QDir().mkpath(configPath);

    s_settings = new QSettings(configPath + "/n02.ini", QSettings::IniFormat);
    s_group = subModule;

    if (!s_group.isEmpty()) {
        s_settings->beginGroup(s_group);
    }
}

void Settings::terminate() {
    if (s_settings) {
        if (!s_group.isEmpty()) {
            s_settings->endGroup();
        }
        s_settings->sync();
        delete s_settings;
        s_settings = nullptr;
    }
}

int Settings::getInt(const QString& key, int defaultValue) {
    if (!s_settings) {
        return defaultValue;
    }
    return s_settings->value(key, defaultValue).toInt();
}

QString Settings::getString(const QString& key, const QString& defaultValue) {
    if (!s_settings) {
        return defaultValue;
    }
    return s_settings->value(key, defaultValue).toString();
}

void Settings::setInt(const QString& key, int value) {
    if (s_settings) {
        s_settings->setValue(key, value);
    }
}

void Settings::setString(const QString& key, const QString& value) {
    if (s_settings) {
        s_settings->setValue(key, value);
    }
}

void Settings::sync() {
    if (s_settings) {
        s_settings->sync();
    }
}
