//
// SR Spritesheet Manager
//
// file:   srsm_project.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SRSM_PROJECT_HPP
#define SRSM_PROJECT_HPP

//#include "bifrost_sprite_animation_api.h"
#include "srsm_animation.hpp"

#include <QBuffer>      //QBuffer
#include <QDir>         // QDir
#include <QJsonObject>  // QJsonObject
#include <QString>      // QString
#include <QUndoStack>   // QUndoStack

#include <memory>  // unique_ptr<T>
#include <vector>  // vector<T>

class Project;
class ImageLibrary;
class MainWindow;

struct AtlasExport final
{
  QPixmap                      pixmap;      //!< For fast drawing.
  QImage                       image;       //!< For fast manipulation.
  std::unique_ptr<QBuffer>     atlas_data;  //!< For saving.
  std::vector<QRect>           frame_rects;
  QMap<QString, std::uint32_t> abs_path_to_index;
};

using ProjectPtr = std::unique_ptr<Project>;

template<typename FRedo>
class UndoAction final : public QUndoCommand
{
 private:
  Project*    m_Project;
  QJsonObject m_SerializedState;
  bool        m_FirstTime;

 public:
  UndoAction(Project* project, FRedo&& do_action);

  void undo() override
  {
    swapStates();
  }

  void redo() override
  {
    if (!m_FirstTime)
    {
      swapStates();
    }

    m_FirstTime = false;
  }

 private:
  void swapStates();
};

class Project final : public QObject
{
  Q_OBJECT

 private:
  QString               m_Name;
  std::unique_ptr<QDir> m_ProjectFile;
  ImageLibrary*         m_ImageLibrary;
  QUndoStack*           m_HistoryStack;
  QStandardItemModel    m_AnimationList;
  MainWindow&           m_UI;
  AtlasExport           m_Export;
  int                   m_SelectedAnimation;
  int                   m_SelectedFrame;
  unsigned int          m_SpriteSheetImageSize;
  unsigned int          m_SpriteSheetFrameSize;
  bool                  m_AtlasModified;
  bool                  m_IsRegeneratingAtlas;

 public:
  explicit Project(MainWindow* main_window, const QString& name);

  template<typename FRedo>
  void recordAction(const QString& name, FRedo&& callback)
  {
    recordActionImpl(name, new UndoAction<FRedo>(this, std::forward<FRedo>(callback)));
  }

  // Undo-able Document Actions
  void newAnimation(const QString& name, int frame_rate);
  void selectAnimation(QModelIndex index);
  void removeAnimation(QModelIndex index);
  void importImageUrls(const QList<QUrl>& urls);

  //

  void notifyAnimationChanged(Animation* animation);
  void markAtlasModifed();

 signals:
  void animationChanged(Animation* anim);
  void animationSelected(Animation* anim);
  void atlasModified(AtlasExport& atlas);
  void renamed(const QString& name);

 public slots:
  void onTimelineFpsChange(int value);
  void onCreateNewFolder();
  void onImportImages();

 private slots:
  void regenExport();

 public:
  const QString&      name() const { return m_Name; }
  bool                hasPath() const { return m_ProjectFile != nullptr; }
  QDir                projectFolder() const { return m_ProjectFile ? *m_ProjectFile : QDir(""); }
  QUndoStack&         historyStack() const { return *m_HistoryStack; }
  QStandardItemModel& animations() { return m_AnimationList; }

  void setup(ImageLibrary* img_library);

  bool        open(const QString& file_path);
  bool        save(QWidget* parent);
  QJsonObject serialize();
  bool        deserialize(const QJsonObject& data);

  Animation* animationAt(int index);
  Animation* animationAt(QModelIndex index);

 private:
  // Document Actions without the Undo stack

  void newAnimationRaw(const QString& name, int frame_rate);

  // Helpers

  bool hasAnimation(const QString& name);
  void recordActionImpl(const QString& name, QUndoCommand* action);
};

template<typename FRedo>
UndoAction<FRedo>::UndoAction(Project* project, FRedo&& do_action) :
  QUndoCommand(),
  m_Project{project},
  m_SerializedState{},
  m_FirstTime{true}
{
  m_SerializedState = m_Project->serialize();
  do_action();
}

template<typename FRedo>
void UndoAction<FRedo>::swapStates()
{
  auto right_before_restore = m_Project->serialize();
  m_Project->deserialize(m_SerializedState);
  m_SerializedState = std::move(right_before_restore);
}

template<typename FRedo>
inline UndoAction<FRedo>* makeUndoAction(Project* project, const QString& name, FRedo&& callback)
{
  UndoAction<FRedo>* const action = new UndoAction<FRedo>(project, std::forward<FRedo>(callback));
  action->setText(name);
  return action;
}

#endif  // SRSM_PROJECT_HPP
