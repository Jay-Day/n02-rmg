#pragma once

#include <QString>

class QSettings;

class Settings {
public:
    static void initialize(const QString& subModule = QString());
    static void terminate();

    static int getInt(const QString& key, int defaultValue = 0);
    static QString getString(const QString& key, const QString& defaultValue = QString());
    static void setInt(const QString& key, int value);
    static void setString(const QString& key, const QString& value);

    static void sync();

private:
    static QSettings* s_settings;
    static QString s_group;
};
