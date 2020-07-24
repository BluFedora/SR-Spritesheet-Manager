//
// SR Texture Packer
// Copyright (c) 2020 Shareef Aboudl-Raheem
//

#include "mainwindow.h"

#include "UI/sr_welcome_window.hpp"
#include "UI/srsm_timeline.hpp"
#include "newanimation.hpp"

#include <QCollator>
#include <QDebug>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QStringListModel>
#include <iostream>

#include <cmath>

MainWindow::MainWindow(const QString& name, QWidget* parent) :
  QMainWindow(parent),
  m_BaseTitle{},
  m_OpenProject{std::make_unique<Project>(this, name)}  //,
//m_AnimationContext{}
{
  setupUi(this);

  m_BaseTitle = windowTitle();

  setWindowModified(!m_OpenProject->hasPath());
  setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
  //setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  //setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  m_UndoView->setStack(&m_OpenProject->historyStack());
  m_AnimationList->setModel(&m_OpenProject->animations());
  m_OpenProject->setup(m_ImageLibrary);

  onProjectRenamed(m_OpenProject->name());

  QObject::connect(m_TimelineFrameSizeSlider, &QSlider::valueChanged, m_TimelineFrames, &Timeline::onFrameSizeChanged);
  QObject::connect(m_TimelineFpsSpinbox, &QSpinBox::valueChanged, m_OpenProject.get(), &Project::onTimelineFpsChange);
  QObject::connect(m_OpenProject.get(), &Project::atlasModified, m_TimelineFrames, &Timeline::onAtlasUpdated);
  QObject::connect(m_OpenProject.get(), &Project::animationChanged, m_TimelineFrames, &Timeline::onAnimationChanged);
  QObject::connect(m_OpenProject.get(), &Project::animationSelected, m_TimelineFrames, &Timeline::onAnimationSelected);
  QObject::connect(m_OpenProject.get(), &Project::animationSelected, m_FrameList, &FrameListView::onSelectAnimation);
  QObject::connect(m_OpenProject.get(), &Project::animationSelected, m_GfxPreview, &AnimationPreview::onAnimationSelected);
  QObject::connect(m_ActionImageLibraryNewFolder, &QAction::triggered, m_OpenProject.get(), &Project::onCreateNewFolder);
  QObject::connect(m_ActionImportImages, &QAction::triggered, m_OpenProject.get(), &Project::onImportImages);
  QObject::connect(m_OpenProject.get(), &Project::renamed, this, &MainWindow::onProjectRenamed);
  QObject::connect(m_AnimationList, &QListView::customContextMenuRequested, this, &MainWindow::onAnimationRightClick);

  static bool s_IsPlaying = true;

  m_TimelinePlayButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));

  QObject::connect(m_TimelinePlayButton, &QToolButton::clicked, [this]() {
    s_IsPlaying = !s_IsPlaying;
    m_TimelinePlayButton->setIcon(s_IsPlaying ? style()->standardIcon(QStyle::SP_MediaPlay) : style()->standardIcon(QStyle::SP_MediaPause));
  });

#if 0
  bfAnimation2DCreateParams anim_params;
  anim_params.allocator = nullptr;

  bfAnimation2D_ctor(&m_AnimationContext, &anim_params);

  m_GfxPreview->setContext(&m_AnimationContext);
#endif
}

MainWindow::~MainWindow()
{
  //bfAnimation2D_dtor(&m_AnimationContext);
}

/*
  new_image.save(imagePath, "PNG");

  QFile jsonFile(jsonPath);
  jsonFile.open(QFile::WriteOnly);
  jsonFile.write(QJsonDocument(spritesheetData).toJson());
  jsonFile.close();
*/

void MainWindow::onProjectRenamed(const QString& name)
{
  setWindowTitle(name + "[*] - " + m_BaseTitle);
}

void MainWindow::onAnimationSelected(QModelIndex index)
{
  m_OpenProject->selectAnimation(index);
}

void MainWindow::onSaveProject()
{
  m_OpenProject->save(this);
}

void MainWindow::onAnimationNew()
{
  NewAnimation new_anim_dlog;

  if (new_anim_dlog.exec() == QDialog::Accepted)
  {
    m_OpenProject->newAnimation(new_anim_dlog.name(), new_anim_dlog.frameRate());
  }
}

void MainWindow::onAnimationRightClick(const QPoint& pos)
{
  const QModelIndexList selected_items = m_AnimationList->selectionModel()->selectedIndexes();

  if (!selected_items.isEmpty())
  {
    QMenu r_click;

    //auto*      rename           = r_click.addAction("Rename");
    //auto*      change_framerate = r_click.addAction("Change Framerate");
    auto* const remove = r_click.addAction("Remove Animation");
    const auto  result = r_click.exec(m_AnimationList->mapToGlobal(pos));

    if (result == remove)
    {
      m_OpenProject->removeAnimation(selected_items[0]);
    }

#if 0
    if (result == rename)
    {
      for (const auto& item : selected_items)
      {
        const auto name = QInputDialog::getText(this, "Animation", "NAME: ", QLineEdit::Normal, m_Document____.m_AnimationNames.data(item).toString());
        m_Document____.m_AnimationNames.setData(item, name);
        m_Document____.m_AnimationData[std::size_t(item.row())]->name = name;
        break;
      }
    }
    else if (result == change_framerate)
    {
      for (const auto& item : selected_items)
      {
        auto&      anim       = m_Document____.m_AnimationData[std::size_t(item.row())];
        const auto frame_rate = QInputDialog::getInt(this, "New Framerate", "FPS: ", (int)std::round(1.0f / anim->frame_time), 1, 60);
        anim->frame_time      = 1.0f / frame_rate;
        break;
      }
    }
#endif
  }
}

void MainWindow::onShowWelcomeScreen()
{
  WelcomeWindow* const welcome_window = new WelcomeWindow();
  welcome_window->show();
}

void MainWindow::changeEvent(QEvent* e)
{
  QMainWindow::changeEvent(e);

  switch (e->type())
  {
    case QEvent::LanguageChange:
      retranslateUi(this);
      break;
    default:
      break;
  }
}

void MainWindow::on_m_ActionAboutQt_triggered()
{
  QMessageBox::aboutQt(this, "About Qt");
}
