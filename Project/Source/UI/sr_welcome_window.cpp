#include "UI/sr_welcome_window.hpp"

#include "ui_sr_welcome_window.h"

#include "Data/srsm_project.hpp"
#include "mainwindow.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

WelcomeWindow::WelcomeWindow(QWidget* parent) :
  QDialog(parent),
  ui(new Ui::WelcomeWindow)
{
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  // HWND(winId())
}

WelcomeWindow::~WelcomeWindow()
{
  delete ui;
}

void WelcomeWindow::on_m_NewProjectBtn_clicked()
{
  bool    ok;
  QString text = QInputDialog::getText(this, "New Project", "Project Name", QLineEdit::Normal, "New Spritesheet", &ok, Qt::Dialog, Qt::ImhNone);

  if (ok)
  {
    if (!text.isEmpty())
    {
      MainWindow* const main_window = new MainWindow(text);
      main_window->show();
      accept();
    }
    else
    {
      QMessageBox::warning(this, "Warning", "A Project must have a non empty name.");
    }
  }
}

void WelcomeWindow::on_m_OpenProjectBtn_clicked()
{
  const QString json_file = QFileDialog::getOpenFileName(this, "Select A Spritesheet", QString(), "Spritesheet Project (*.srsmproj.json)");

  if (!json_file.isEmpty())
  {
    MainWindow* const main_window = new MainWindow("__Unnamed__");
    auto&             prj         = main_window->project();

    if (prj->open(json_file))
    {
      main_window->show();
      accept();
    }
    else
    {
      main_window->deleteLater();
      QMessageBox::warning(this, "Warning", "Failed to open project.");
    }
  }
}
