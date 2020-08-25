//
// SR Spritesheet Manager
//
// file:   main.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "main.hpp"                   // g_Server
#include "Data/srsm_settings.hpp"     // Settings
#include "UI/sr_welcome_window.hpp"   // WelcomeWindow
#include "UI/srsm_image_library.hpp"  // AnimationFrameSourcePtr

#include <QApplication>     // QApplication
#include <QDir>             // For MacOS
#include <QMessageBox>      // QMessageBox
#include <QSharedMemory  >  // QSharedMemory
#include <QStyleFactory>    // QStyleFactory

/*!
 * @brief main
 *   The main entry point of this program.
 *
 * @param argc
 *   The number of arguments passed to this program.
 *
 * @param argv
 *   The arguments passed to this program, the size in \p argc.
 *
 * @return
 *   Returns 0 when no error has occured, otherwise an error code.
 */
int main(int argc, char *argv[])
{
  qRegisterMetaTypeStreamOperators<std::shared_ptr<AnimationFrameSource>>("std::shared_ptr<AnimationFrameSource>");

  // If this was a command line app then only 'QCoreApplication'
  // would be needed rather than 'QApplication'.

  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  QApplication app(argc, argv);

#ifdef Q_OS_MACX
  QDir bin(QCoreApplication::applicationDirPath());
  bin.cdUp(); /* Fix this on Mac because of the .app folder, */
  bin.cdUp(); /* which means that the actual executable is   */
  bin.cdUp(); /* three levels deep. ;(                       */
  QDir::setCurrent(bin.absolutePath());
#endif

  QApplication::setStyle(QStyleFactory::create("Fusion"));

  QPalette palette;

  palette.setColor(QPalette::Window, QColor(53, 53, 53));
  palette.setColor(QPalette::WindowText, Qt::white);
  palette.setColor(QPalette::Base, QColor(15, 15, 15));
  palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
  palette.setColor(QPalette::ToolTipBase, Qt::white);
  palette.setColor(QPalette::ToolTipText, Qt::white);
  palette.setColor(QPalette::Text, Qt::white);
  palette.setColor(QPalette::Button, QColor(53, 53, 53));
  palette.setColor(QPalette::ButtonText, Qt::white);
  palette.setColor(QPalette::BrightText, Qt::red);
  palette.setColor(QPalette::Disabled, QPalette::Text, Qt::darkGray);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::darkGray);
  palette.setColor(QPalette::Highlight, QColor(142, 45, 197).lighter());
  palette.setColor(QPalette::HighlightedText, Qt::black);

  app.setPalette(palette);

  QSharedMemory shared_mem{"SRSM.AppCount", nullptr};

  if (!shared_mem.attach(QSharedMemory::ReadWrite))
  {
    shared_mem.create(1, QSharedMemory::ReadWrite);

    g_Server = std::make_unique<LiveReloadServer>();
  }
  else
  {
    QMessageBox::warning(nullptr, "Error", "Only one instance of this program can be opened at one.", QMessageBox::Ok, QMessageBox::Ok);
    return 0;
  }

  if (!g_Server->startServer())
  {
    g_Server = nullptr;
    QMessageBox::warning(nullptr, "Error", "Failed to create a local sevrer. Connect to engine functionality will be disabled.", QMessageBox::Ok, QMessageBox::Ok);
  }
  else
  {
    g_Server->setup();
  }

  Settings::openRecentFiles();

  WelcomeWindow welcome_window;
  welcome_window.show();

  const int app_result = app.exec();

  shared_mem.detach();
  g_Server.reset();

  Settings::saveRecentFile();

  return app_result;
}

std::unique_ptr<LiveReloadServer> g_Server = nullptr;
