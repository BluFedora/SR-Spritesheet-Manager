//
// SR Texture Packer
// Copyright (c) 2020 Shareef Aboudl-Raheem
//

#ifndef SRSM_MAINWINDOW_H
#define SRSM_MAINWINDOW_H

#include "Data/bf_property.hpp"  // IPropChangeHandler
#include "Data/sr_project.hpp"   // ProjectPtr

#include "ui_sr_main_window.h"

#include <QProgressBar>

struct OnTimelineChange : public bf::IPropChangeListener<int>
{
  QSlider* slider;

  OnTimelineChange() :
    bf::IPropChangeListener<int>([](bf::IPropChangeListener<int>* self, const int& /* old_val */, const int& new_val) {
      OnTimelineChange* handler = (OnTimelineChange*)self;
      handler->slider->setValue(new_val);
    })
  {
  }
};

// clang-format off
class MainWindow : public QMainWindow, private Ui::MainWindow
// clang-format on
{
  Q_OBJECT

 private:
  QString          m_BaseTitle;
  ProjectPtr       m_OpenProject;
  QProgressBar     m_PacketSendingProgress;
  OnTimelineChange m_OnTimelineChange;

 public:
  explicit MainWindow(const QString& name, QWidget* parent = 0);

  void postLoadInit();

  ProjectPtr& project() { return m_OpenProject; }
  QSpinBox&   timelineFpsSpinbox() const { return *m_TimelineFpsSpinbox; }
  QListView&  animationListView() const { return *m_AnimationList; }

 public slots:
  void onProjectRenamed(const QString& name);
  void onSaveProject();
  void onAnimationNew();
  void onAnimationRightClick(const QPoint& pos);
  void onShowWelcomeScreen();
  void onSpritesheetQualitySettingChanged();
  void onAnimationSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
  void onAnimChanged(Animation* anim);

  // QWidget interface
 protected:
  void changeEvent(QEvent* e) override;
  void closeEvent(QCloseEvent* event) override;

 private slots:
  void on_m_ActionAboutQt_triggered();
  void on_m_ActionExportSpritesheet_triggered();
  void on_m_ActionProjectRename_triggered();
  void on_m_MiniScrubber_sliderMoved(int position);

 private:
  void restoreWindowLayout();
};

#endif  // SRSM_MAINWINDOW_H
