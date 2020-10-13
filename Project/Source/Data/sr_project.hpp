//
// SR Spritesheet Manager
//
// file:   srsm_project.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SRSM_PROJECT_HPP
#define SRSM_PROJECT_HPP

#include "sr_animation.hpp"  // Animation

#include <QBuffer>      // QBuffer
#include <QDir>         // QDir
#include <QJsonObject>  // QJsonObject
#include <QString>      // QString
#include <QUndoStack>   // QUndoStack
#include <QUuid>        // QUuid

#include <memory>  // unique_ptr<T>
#include <vector>  // vector<T>

class Project;
class ImageLibrary;
class MainWindow;

struct AtlasExport final
{
  QPixmap                      pixmap;            //!< For fast drawing.
  QImage                       image;             //!< For fast manipulation.
  std::unique_ptr<QBuffer>     atlas_data;        //!< For saving.
  std::vector<QRect>           image_rectangles;  //!< For regenerating the the atlas.
  QMap<QString, std::uint32_t> frame_to_index;    //!<
};

using ProjectPtr = std::unique_ptr<Project>;

enum UndoActionFlag
{
  UndoActionFlag_ModifiedSettings  = (1u << 0),
  UndoActionFlag_ModifiedAnimation = (1u << 1),
  UndoActionFlag_ModifiedAtlas     = (1u << 2),
  UndoActionFlag_ModifiedAll       = UndoActionFlag_ModifiedSettings | UndoActionFlag_ModifiedAtlas | UndoActionFlag_ModifiedAnimation,
  UndoActionFlag_FirstTime         = (1u << 3),
};

Q_DECLARE_FLAGS(UndoActionFlags, UndoActionFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(UndoActionFlags)

template<typename FRedo>
class UndoAction final : public QUndoCommand
{
 private:
  Project*        m_Project;
  QJsonObject     m_SerializedState;
  UndoActionFlags m_Flags;

 public:
  UndoAction(Project* project, UndoActionFlags flags, FRedo&& do_action);

  void undo() override final { swapStates(); }
  void redo() override final;

 private:
  void swapStates();
};

class Project final : public QObject
{
  Q_OBJECT

 private:
  QString               m_Name;
  QUuid                 m_EditUUID;
  std::unique_ptr<QDir> m_ProjectFile;
  ImageLibrary*         m_ImageLibrary;
  QUndoStack*           m_HistoryStack;
  QStandardItemModel    m_AnimationList;
  MainWindow&           m_UI;
  AtlasExport           m_Export;
  int                   m_SelectedAnimation;
  unsigned int          m_SpriteSheetImageSize;
  unsigned int          m_SpriteSheetFrameSize;
  bool                  m_AtlasModified;
  bool                  m_IsRegeneratingAtlas;

 public:
  explicit Project(MainWindow* main_window, const QString& name);

  // Accessors

  const QString&      name() const { return m_Name; }
  bool                hasPath() const { return m_ProjectFile != nullptr; }
  QDir                projectFolder() const { return m_ProjectFile ? *m_ProjectFile : QDir(""); }
  QUndoStack&         historyStack() const { return *m_HistoryStack; }
  QStandardItemModel& animations() { return m_AnimationList; }
  unsigned int        spritesheetImageSize() const { return m_SpriteSheetImageSize; }
  unsigned int        spritesheetFrameSize() const { return m_SpriteSheetFrameSize; }
  Animation*          selectedAnimation() const { return m_SelectedAnimation == -1 ? nullptr : animationAt(m_SelectedAnimation); }

  // Undo-able Document Action API

  template<typename FRedo>
  void recordAction(const QString& name, UndoActionFlags flags, FRedo&& callback)
  {
    recordActionImpl(name, new UndoAction<FRedo>(this, flags | UndoActionFlag_FirstTime, std::forward<FRedo>(callback)));
  }

  void newAnimation(const QString& name, int frame_rate);
  void selectAnimation(QModelIndex index);
  void selectAnimation(int index);
  void removeAnimation(QModelIndex index);
  void importImageUrls(const QList<QUrl>& urls);
  void setAnimationName(Animation* anim, const QString& new_name);

  //

  void notifyAnimationChanged(Animation* animation);

 signals:
  void animationChanged(Animation* anim);
  void animationSelected(Animation* anim);
  void atlasModified(AtlasExport& atlas);
  void renamed(const QString& name);
  void signalPreviewFrameSelected(Animation* anim);

 public slots:
  void onTimelineFpsChange(int value);
  void onCreateNewFolder();
  void onImportImages();
  void setSpritesheetImageSize(int value);
  void setSpritesheetFrameSize(int value);
  void setProjectName(const QString& value);
  void regenerateAtlasExport();
  void regenerateAnimationExport();

 private slots:
  void markAnimationsModifed();
  void onUndoRedoIndexChanged(int idx);
  void handleImageLibraryChange();

 public:
  void markAtlasModifed();
  void setup(ImageLibrary* img_library);

  bool        exportAtlas(const QString& dir_path);
  bool        open(const QString& file_path);
  bool        save();
  QJsonObject serialize();
  bool        deserialize(const QJsonObject& data, UndoActionFlags flags = UndoActionFlag_ModifiedAll);

  int        numAnimations() const;
  Animation* animationAt(int index) const;
  Animation* animationAt(QModelIndex index) const;

 private:
  // Document Actions without the Undo stack

  int  newAnimationRaw(const QString& name, int frame_rate);
  void setProjectNameRaw(const QString& new_name);

  // Helpers

  bool hasAnimation(const QString& name);
  void recordActionImpl(const QString& name, QUndoCommand* action);
};

template<typename FRedo>
UndoAction<FRedo>::UndoAction(Project* project, UndoActionFlags flags, FRedo&& do_action) :
  QUndoCommand(),
  m_Project{project},
  m_SerializedState{},
  m_Flags{flags}
{
  m_SerializedState = m_Project->serialize();
  do_action();
}

template<typename FRedo>
void UndoAction<FRedo>::redo()
{
  if (m_Flags.testFlag(UndoActionFlag_FirstTime))
  {
    m_Flags.setFlag(UndoActionFlag_FirstTime, false);

    if (m_Flags & UndoActionFlag_ModifiedAtlas)
    {
      m_Project->markAtlasModifed();
      m_Project->regenerateAtlasExport();
    }

    if (m_Flags & UndoActionFlag_ModifiedAnimation)
    {
      m_Project->regenerateAnimationExport();
    }

    return;
  }

  swapStates();
}

template<typename FRedo>
void UndoAction<FRedo>::swapStates()
{
  auto right_before_restore = m_Project->serialize();
  m_Project->deserialize(m_SerializedState, m_Flags);
  m_SerializedState = std::move(right_before_restore);

  if (m_Flags & UndoActionFlag_ModifiedAtlas)
  {
    m_Project->markAtlasModifed();
  }
}

#endif  // SRSM_PROJECT_HPP
