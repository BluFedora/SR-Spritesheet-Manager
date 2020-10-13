//
// SR Spritesheet Manager
//
// file:   srsm_settings.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <QSettings>

class Settings : public QSettings
{
 public:
  static void openRecentFiles();
  static void addRecentFile(const QString& name, const QString& path);
  static void saveRecentFile();

 public:
  Settings();
};

#endif  // SETTINGS_HPP
