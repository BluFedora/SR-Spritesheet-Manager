//
// SR Texture Packer
// Copyright (c) 2020 Shareef Aboudl-Raheem
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_mainwindow.h"

#include "Data/srsm_project.hpp"  // ProjectPtr

// #include "bifrost_sprite_animation_api.h"

#include <QJsonObject>
#include <QStringListModel>

#include <memory>
#include <vector>

class MainWindow : public QMainWindow
  , private Ui::MainWindow
{
  Q_OBJECT

 private:
  QString    m_BaseTitle;
  ProjectPtr m_OpenProject;
  //BifrostAnimation2DCtx m_AnimationContext;

 public:
  explicit MainWindow(const QString& name, QWidget* parent = 0);

  ProjectPtr& project() { return m_OpenProject; }
  QSpinBox&   timelineFpsSpinbox() const { return *m_TimelineFpsSpinbox; }

  ~MainWindow();

 public slots:
  void onProjectRenamed(const QString& name);
  void onAnimationSelected(QModelIndex index);
  void onSaveProject();
  void onAnimationNew();
  void onAnimationRightClick(const QPoint& pos);
  void onShowWelcomeScreen();

 protected:
  void changeEvent(QEvent* e);

 private slots:
  void on_m_ActionAboutQt_triggered();
};

#endif  // MAINWINDOW_H
