//
// SR Texture Packer
// Copyright (c) 2020 Shareef Aboudl-Raheem
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_mainwindow.h"

#include "Data/srsm_project.hpp"  // ProjectPtr

#include <QJsonObject>
#include <QStringListModel>

#include <memory>
#include <vector>

// clang-format off
class MainWindow : public QMainWindow, private Ui::MainWindow
// clang-format on
{
  Q_OBJECT

 private:
  QString    m_BaseTitle;
  ProjectPtr m_OpenProject;

 public:
  explicit MainWindow(const QString& name, QWidget* parent = 0);

  void postLoadInit();

  ProjectPtr& project() { return m_OpenProject; }
  QSpinBox&   timelineFpsSpinbox() const { return *m_TimelineFpsSpinbox; }
  QListView&  animationListView() const { return *m_AnimationList; }
  // FrameListView& frameListView() const { return *m_FrameList; }

 public slots:
  void onProjectRenamed(const QString& name);
  void onSaveProject();
  void onAnimationNew();
  void onAnimationRightClick(const QPoint& pos);
  void onShowWelcomeScreen();
  void onSpritesheetQualitySettingChanged();
  void onAnimationSelectionChanged(const QModelIndex& current, const QModelIndex& previous);

  // QWidget interface
 protected:
  void changeEvent(QEvent* e) override;
  void closeEvent(QCloseEvent* event) override;

 private slots:
  void on_m_ActionAboutQt_triggered();
  void on_m_ActionExportSpritesheet_triggered();

 private:
  void restoreWindowLayout();
};

#endif  // MAINWINDOW_H
