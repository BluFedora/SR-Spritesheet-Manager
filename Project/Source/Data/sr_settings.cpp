//
// SR Spritesheet Manager
//
// file:   srsm_settings.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "sr_settings.hpp"

struct RecentFileEntry final
{
  QString name;
  QString path;
};

static constexpr const char* const k_OrganizationName = "BluFedora";
static constexpr const char* const k_ApplicationName  = "SR-Spritesheet Manager";

static std::vector<RecentFileEntry> s_RecentFiles = {};

void Settings::openRecentFiles()
{
  Settings settings;
  settings.beginGroup("WelcomeWindow");

  const int num_items = settings.beginReadArray("RecentFiles");

  for (int i = 0; i < num_items; ++i)
  {
    settings.setArrayIndex(i);

    QString recent_file_name = settings.value("name").toString();
    QString recent_file_path = settings.value("path").toString();

    s_RecentFiles.push_back(RecentFileEntry{recent_file_name, recent_file_path});
  }

  settings.endArray();

  settings.endGroup();
}

void Settings::addRecentFile(const QString& name, const QString& path)
{
  for (auto& rf : s_RecentFiles)
  {
    if (rf.name == name && rf.path == path)
    {
      return;
    }
  }

  s_RecentFiles.push_back(RecentFileEntry{name, path});
}

void Settings::saveRecentFile()
{
  Settings settings;
  settings.beginGroup("WelcomeWindow");

  const int num_items = int(s_RecentFiles.size());

  settings.beginWriteArray("RecentFiles", num_items);

  for (int i = 0; i < num_items; ++i)
  {
    settings.setArrayIndex(i);

    settings.setValue("name", s_RecentFiles[i].name);
    settings.setValue("path", s_RecentFiles[i].path);
  }

  settings.endArray();

  settings.endGroup();
}

Settings::Settings() :
  QSettings(k_OrganizationName, k_ApplicationName)
{
}
