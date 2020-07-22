//
// SR Spritesheet Manager
//
// file:   main.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "UI/sr_welcome_window.hpp" // WelcomeWindow
#include "mainwindow.h"             // MainWindow

#include <QApplication>  // QApplication
#include <QDir>          // For MacOS
#include <QStyleFactory> // QStyleFactory

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
  // If this was a command line app then only 'QCoreApplication'
  // would be needed rather than 'QApplication'.

  QApplication app(argc, argv);

#ifdef Q_OS_MACX
  QDir bin(QCoreApplication::applicationDirPath());
  /* Set working directory */
  bin.cdUp();    /* Fix this on Mac because of the .app folder, */
  bin.cdUp();    /* which means that the actual executable is   */
  bin.cdUp();    /* three levels deep. Grrr.                    */
  QDir::setCurrent(bin.absolutePath());
#endif

  // QApplication::setStyle(QStyleFactory::create("Fusion"));

  WelcomeWindow welcome_window;
  welcome_window.show();

  return app.exec();
}
