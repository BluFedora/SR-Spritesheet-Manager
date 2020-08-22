//
// SR Spritesheet Manager
//
// file:   srsm_welcome_window.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "UI/sr_welcome_window.hpp"

#include "ui_sr_welcome_window.h"

#include "Data/srsm_project.hpp"
#include "Data/srsm_settings.hpp"

#include "mainwindow.hpp"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPropertyAnimation>

const QString k_XButtonStyles = R"(
QPushButton {
  background-color: #313131;
  border:           3px solid #404040;
}

QPushButton:hover:!pressed {
  background-color: #EF6363;
}

QPushButton:pressed {
  background-color: #D53E3E;
}
)";

WelcomeWindow::WelcomeWindow(QWidget* parent) :
  QDialog(parent),
  ui(new Ui::WelcomeWindow),
  m_RecentFilePaths{},
  m_DragPos{0, 0},
  m_DoDrag{false}
{
  ui->setupUi(this);

  setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::FramelessWindowHint);

  auto* const x_btn      = new QPushButton(this);
  const auto  x_btn_size = QSize(30, 30);

  x_btn->setIcon(QIcon(":/Res/Images/Icons/x_button.png"));
  x_btn->setIconSize(QSize(24, 24));
  x_btn->setMinimumSize(x_btn_size);
  x_btn->setMaximumSize(x_btn_size);
  x_btn->setStyleSheet(k_XButtonStyles);
  x_btn->setContentsMargins(1, 2, 3, 4);

  ui->gridLayout->addWidget(x_btn, 0, 0, Qt::AlignRight | Qt::AlignTop);

  QObject::connect(x_btn, &QPushButton::clicked, this, &WelcomeWindow::onCloseRequested);

  // HWND(winId())

  setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
  raise();           // for MacOS
  activateWindow();  // for Windows

  QPropertyAnimation* animation = new QPropertyAnimation(this, "windowOpacity", this);

  animation->setDuration(270 / 2);
  animation->setStartValue(0.0);
  animation->setEndValue(1.0);
  animation->setEasingCurve(QEasingCurve::OutQuad);

  animation->start();

  Settings settings;
  settings.beginGroup("WelcomeWindow");

  const int num_items = settings.beginReadArray("RecentFiles");

  for (int i = 0; i < num_items; ++i)
  {
    settings.setArrayIndex(i);

    QString recent_file_name = settings.value("name").toString();
    QString recent_file_path = settings.value("path").toString();

    ui->m_RecentFiles->addItem(recent_file_name);
    m_RecentFilePaths.push_back(recent_file_path);
  }

  settings.endArray();

  settings.endGroup();
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
      main_window->postLoadInit();
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
    tryOpenProject(json_file);
  }
}

void WelcomeWindow::onCloseRequested()
{
  QPropertyAnimation* animation = new QPropertyAnimation(this, "windowOpacity", this);

  animation->setDuration(270);
  animation->setStartValue(1.0);
  animation->setEndValue(0.0);
  animation->setEasingCurve(QEasingCurve::OutQuad);

  animation->start();

  QObject::connect(animation, &QPropertyAnimation::finished, this, &WelcomeWindow::close);
}

void WelcomeWindow::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    m_DragPos = event->globalPos() - frameGeometry().topLeft();
    m_DoDrag  = true;
    event->accept();
  }
}

void WelcomeWindow::mouseMoveEvent(QMouseEvent* event)
{
  if (m_DoDrag && event->buttons() & Qt::LeftButton)
  {
    move(event->globalPos() - m_DragPos);
    event->accept();
  }
}

void WelcomeWindow::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    m_DoDrag = false;
    event->accept();
  }
}

void WelcomeWindow::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Escape)
  {
    onCloseRequested();
    event->accept();
  }
  else
  {
    QDialog::keyPressEvent(event);
  }
}

void WelcomeWindow::tryOpenProject(const QString& file_path)
{
  MainWindow* const main_window = new MainWindow("__Unnamed__");
  auto&             prj         = main_window->project();

  if (prj->open(file_path))
  {
    main_window->postLoadInit();
    main_window->show();
    accept();
  }
  else
  {
    main_window->deleteLater();
    QMessageBox::warning(this, "Warning", "Failed to open project.");
  }
}

void WelcomeWindow::on_m_RecentFiles_itemDoubleClicked(QListWidgetItem* item)
{
  tryOpenProject(m_RecentFilePaths[ui->m_RecentFiles->row(item)]);
}
