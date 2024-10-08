//
// SR Spritesheet Manager
//
// file:   srsm_project.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "sr_project.hpp"

#include "Data/sr_settings.hpp"              // Settings
#include "Server/sr_live_reload_server.hpp"  // g_Server
#include "UI/sr_image_library.hpp"           // ImageLibrary
#include "sr_main_window.hpp"

#include <QDebug>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProgressDialog>

#include <cmath>  // ceil

Project::Project(MainWindow* main_window, const QString& name) :
  m_Name{name},
  m_EditUUID{},
  m_ProjectFile{nullptr},
  m_ImageLibrary{nullptr},
  m_HistoryStack{new QUndoStack(main_window)},
  m_AnimationList{},
  m_UI{*main_window},
  m_Export{},
  m_SelectedAnimation{-1},
  m_SpriteSheetImageSize{2048},
  m_SpriteSheetFrameSize{256},
  m_AtlasModified{false},
  m_IsRegeneratingAtlas{false}
{
  m_Export.atlas_data      = std::make_unique<unsigned char[]>(0);
  m_Export.atlas_data_size = 0u;
}

void Project::newAnimation(const QString& name, int frame_rate)
{
  if (!hasAnimation(name))
  {
    recordAction(tr("New Animation (%1)").arg(name), UndoActionFlag_ModifiedAnimation, [this, name, frame_rate]() {
      selectAnimation(newAnimationRaw(name, frame_rate));
    });
  }
  else
  {
    QMessageBox::warning(&m_UI, "Warning", "There already exists an animation named \'" + name + "\'");
  }
}

void Project::selectAnimation(QModelIndex index)
{
  selectAnimation(index.isValid() ? index.row() : -1);
}

void Project::selectAnimation(int index)
{
  if (m_SelectedAnimation != index)
  {
    const bool is_valid = index != -1;

    m_SelectedAnimation = index;

    Animation* const animation = is_valid ? animationAt(m_SelectedAnimation) : nullptr;

    if (animation)
    {
      m_UI.timelineFpsSpinbox().setValue(animation->frame_rate);

      m_UI.animationListView().setCurrentIndex(m_AnimationList.index(m_SelectedAnimation, 0, QModelIndex()));
    }

    m_UI.timelineFpsSpinbox().setEnabled(animation);

    emit animationSelected(animation, index);
  }
}

void Project::removeAnimation(QModelIndex index)
{
  Animation* const animation = animationAt(index);

  if (animation)
  {
    recordAction(tr("Remove Animation (%1)").arg(animation->name()), UndoActionFlag_ModifiedAnimation, [this, index]() {
      if (index.row() == m_SelectedAnimation)
      {
        selectAnimation(-1);
      }

      m_AnimationList.removeRow(index.row(), index.parent());
      m_UI.setWindowModified(true);
    });
  }
}

void Project::importImageUrls(const QList<QUrl>& urls)
{
  recordAction(tr("Import Dragged Images"), UndoActionFlag_ModifiedAtlas, [this, &urls]() {
    m_ImageLibrary->addUrls(urls);
  });
}

void Project::setAnimationName(Animation* anim, const QString& new_name)
{
  if (anim->name() != new_name)
  {
    for (int i = 0; i < numAnimations(); ++i)
    {
      if (animationAt(i)->name() == new_name)
      {
        QMessageBox::warning(&m_UI, "Warning", "There already exists an animation named \'" + new_name + "\'");
        return;
      }
    }

    recordAction(tr("Renamed Animation"), UndoActionFlag_ModifiedAnimation, [anim, &new_name]() {
      anim->setName(new_name);
    });
  }
}

void Project::notifyAnimationChanged(Animation* animation)
{
  emit animationChanged(animation);
}

void Project::markAtlasModifed()
{
  m_UI.setWindowModified(true);
  m_AtlasModified = true;
}

void Project::markAnimationsModifed()
{
  m_UI.setWindowModified(true);
  regenerateAnimationExport();
}

void Project::onUndoRedoIndexChanged(int idx)
{
  if (idx == 0)
  {
    m_UI.setWindowModified(false);
  }

  regenerateAtlasExport();
}

void Project::handleImageLibraryChange()
{
  markAtlasModifed();
  regenerateAtlasExport();
}

void Project::onTimelineFpsChange(int value)
{
  Animation* const animation = animationAt(m_SelectedAnimation);

  if (animation->frame_rate != value)
  {
    animation->frame_rate = value;

    notifyAnimationChanged(animation);

    m_UI.setWindowModified(true);
  }
}

void Project::onCreateNewFolder()
{
  recordAction(tr("Created New Folder"), UndoActionFlag_ModifiedSettings, [this]() {
    m_ImageLibrary->addNewFolder();
  });
}

void Project::onImportImages()
{
  auto files = QFileDialog::getOpenFileNames(&m_UI, "Select An Image Sequence");

  if (!files.isEmpty())
  {
    recordAction("Import Images", UndoActionFlag_ModifiedAtlas, [this, &files]() {
      m_ImageLibrary->addDirectory(files);
    });
  }
}

void Project::setSpritesheetImageSize(int value)
{
  if (m_SpriteSheetImageSize != unsigned(value))
  {
    recordAction(
     tr("Image Quality Set To %1 px").arg(value),
     UndoActionFlag_ModifiedAtlas,
     [this, value]() {
       m_SpriteSheetImageSize = value;
     });
  }
}

void Project::setSpritesheetFrameSize(int value)
{
  if (m_SpriteSheetFrameSize != unsigned(value))
  {
    recordAction(
     tr("Frame Quality Set To %1").arg(value),
     UndoActionFlag_ModifiedAtlas,
     [this, value]() {
       m_SpriteSheetFrameSize = value;
     });
  }
}

void Project::setProjectName(const QString& value)
{
  if (value != m_Name)
  {
    recordAction(
     tr("Renamed Project To \"%1\"").arg(value),
     UndoActionFlag_ModifiedSettings,
     [this, value]() {
       setProjectNameRaw(value);
     });
  }
}

int Project::newAnimationRaw(const QString& name, int frame_rate)
{
  const int new_anim_index = m_AnimationList.rowCount();

  m_AnimationList.appendRow(new Animation(this, name, frame_rate));
  m_UI.setWindowModified(true);

  return new_anim_index;
}

void Project::setProjectNameRaw(const QString& new_name)
{
  if (new_name != m_Name)
  {
    m_Name = new_name;
    m_UI.setWindowModified(true);
    emit renamed(m_Name);
  }
}

void Project::setup(ImageLibrary* img_library)
{
  m_ImageLibrary            = img_library;
  m_ImageLibrary->m_Project = this;

  QObject::connect(img_library, &ImageLibrary::signalImagesAdded, this, &Project::handleImageLibraryChange);
  QObject::connect(img_library, &ImageLibrary::signalImagesChanged, this, &Project::handleImageLibraryChange);
  QObject::connect(m_HistoryStack, &QUndoStack::indexChanged, this, &Project::onUndoRedoIndexChanged);
}

bool Project::exportAtlas(const QString& dir_path)
{
  QDir    root_dir      = dir_path;
  QString image_path    = root_dir.filePath(m_Name + ".png");
  QString bytes_path    = root_dir.filePath(m_Name + ".srsm.bytes");
  QFile   bytes_file    = bytes_path;
  bool    is_successful = m_Export.image.save(image_path, "PNG", -1);

  // Only write out bytes if we were able to save the png.
  is_successful = is_successful && bytes_file.open(QFile::WriteOnly);

  if (is_successful)
  {
    bytes_file.write((const char*)m_Export.atlas_data.get(), m_Export.atlas_data_size);
    bytes_file.close();
  }

  return is_successful;
}

static bool loadJson(QString file_name, QJsonDocument& out)
{
  QFile file(file_name);

  if (file.open(QFile::ReadOnly))
  {
    out = QJsonDocument().fromJson(file.readAll());

    return true;
  }

  return false;
}

bool Project::open(const QString& file_path)
{
  QJsonDocument json_doc;

  if (loadJson(file_path, json_doc))
  {
    if (json_doc.isObject())
    {
      // NOTE(SR): Must happen before the 'deserialize' call for correct file path resolution.

      m_ProjectFile = std::make_unique<QDir>(QFileInfo{file_path}.dir());

      const QJsonObject json_obj = json_doc.object();

      // Must be loaded before deserialize.
      if (json_obj.contains("m_EditUUID"))
      {
        m_EditUUID = QUuid(json_obj.value("m_EditUUID").toString("00000000-0000-0000-0000-000000000000"));
      }

      if (m_EditUUID.isNull())
      {
        m_EditUUID = QUuid::createUuid();
      }

      if (deserialize(json_obj))
      {
        const QString& json_file_path = m_ProjectFile->absoluteFilePath(m_Name + ".srsmproj.json");

        Settings::addRecentFile(name(), json_file_path);

        regenerateAtlasExport();
        m_UI.setWindowModified(false);

        return true;
      }

      m_ProjectFile.reset();
      return false;
    }
  }

  return false;
}

bool Project::save()
{
  if (!m_ProjectFile)
  {
    QString dir = QFileDialog::getExistingDirectory(&m_UI, "Where To Save the Project", "", QFileDialog::ShowDirsOnly);

    if (dir.isEmpty())
    {
      return false;
    }

    m_ProjectFile = std::make_unique<QDir>(dir);
    m_ProjectFile->makeAbsolute();
  }

  if (m_EditUUID.isNull())
  {
    m_EditUUID = QUuid::createUuid();
  }

  const QString& json_file_path = m_ProjectFile->absoluteFilePath(m_Name + ".srsmproj.json");
  QJsonObject    project_data   = serialize();

  Settings::addRecentFile(name(), json_file_path);

  // This data should not be saved to file.
  project_data["m_SelectedAnimation"] = -1;
  project_data["m_SelectedFrame"]     = -1;

  // This data is exclusive to the save file.

  project_data["m_EditUUID"] = m_EditUUID.toString(QUuid::WithoutBraces);

  const QByteArray& json_as_bytes = QJsonDocument(project_data).toJson(QJsonDocument::Indented);

  QFile jsonFile(json_file_path);

  if (jsonFile.open(QFile::WriteOnly))
  {
    jsonFile.write(json_as_bytes);
    jsonFile.close();

    m_UI.setWindowModified(false);

    return true;
  }

  return false;
}

QJsonObject Project::serialize()
{
  const QString& abs_path = m_ProjectFile ? m_ProjectFile->absolutePath() : "";
  QDir           abs_dir{abs_path};

  QJsonObject animations_data;

  for (int i = 0; i < m_AnimationList.rowCount(); ++i)
  {
    Animation*    anim = (Animation*)m_AnimationList.item(i, 0);
    const QString name = anim->data(Qt::DisplayRole).toString();

    QJsonArray frames_data;

    for (std::uint32_t j = 0; j < anim->numFrames(); ++j)
    {
      AnimationFrameInstance* anim_frame = anim->frameAt(j);

      frames_data.push_back(QJsonObject{
       {"rel_path", abs_dir.relativeFilePath(anim_frame->full_path())},
       {"frame_time", anim_frame->frame_time},
      });
    }

    animations_data[name] = QJsonObject{
     {"frames", frames_data},
     {"frame_rate", anim->frame_rate},
    };
  }

  return QJsonObject{
   {"name", m_Name},
   {"last_saved_path", abs_path},
   {"image_library", m_ImageLibrary->serialize(*this)},
   {"animations", animations_data},
   {"m_SelectedAnimation", m_SelectedAnimation},
   {"m_SpriteSheetImageSize", int(m_SpriteSheetImageSize)},
   {"m_SpriteSheetFrameSize", int(m_SpriteSheetFrameSize)},
  };
}

bool Project::deserialize(const QJsonObject& data, UndoActionFlags flags)
{
  if (data.contains("name") && data.contains("image_library") && data.contains("animations"))
  {
    if (flags & UndoActionFlag_ModifiedSettings)
    {
      setProjectNameRaw(data.value("name").toString());

      m_UI.setWindowModified(true);
    }

    if (flags & UndoActionFlag_ModifiedAtlas)
    {
      m_ImageLibrary->deserialize(*this, data.value("image_library").toObject());

      m_SpriteSheetImageSize = data.value("m_SpriteSheetImageSize").toInt(m_SpriteSheetImageSize);
      m_SpriteSheetFrameSize = data.value("m_SpriteSheetFrameSize").toInt(m_SpriteSheetFrameSize);

      markAtlasModifed();

      m_UI.setWindowModified(true);
    }

    // A modifed atlas implies a modified animation since the frame indices may change.
    if (flags & (UndoActionFlag_ModifiedAnimation | UndoActionFlag_ModifiedAtlas))
    {
      selectAnimation(QModelIndex());
      m_AnimationList.removeRows(0, m_AnimationList.rowCount());

      const QJsonObject animation_data = data["animations"].toObject();

      for (const auto& animation_data_key : animation_data.keys())
      {
        const auto   anim               = animation_data[animation_data_key];
        const int    anim_index         = newAnimationRaw(animation_data_key, anim["frame_rate"].toInt());
        Animation*   anim_obj           = (Animation*)m_AnimationList.item(anim_index, 0);
        QJsonArray   frames_data        = anim["frames"].toArray();
        const double default_frame_time = anim_obj->frameTime();

        for (const auto& frame : frames_data)
        {
          const QJsonObject frame_data = frame.toObject();
          const QString     rel_path   = frame_data["rel_path"].toString();
          const QString     abs_path   = m_ProjectFile->absoluteFilePath(rel_path);
          const auto        frame_src  = m_ImageLibrary->findFrameSource(abs_path);

#if 0
          if (rel_path.isEmpty())
          {
              // TODO(SR): Error message.
              continue;
          }
#endif

          if (!frame_src)
          {
            // TODO(SR): Error message.
            continue;
          }

          anim_obj->addFrame(AnimationFrameInstance(frame_src, (float)frame_data["frame_time"].toDouble(default_frame_time)));
        }
      }

      const int selected_anim = data.value("m_SelectedAnimation").toInt(-1);

      selectAnimation(selected_anim);
      m_UI.animationListView().setCurrentIndex(m_AnimationList.index(selected_anim, 0, QModelIndex()));

      m_UI.setWindowModified(true);
    }

    return true;
  }

  return false;
}

int Project::numAnimations() const
{
  return m_AnimationList.rowCount();
}

extern QRect aspectRatioDrawRegion(std::uint32_t aspect_w, std::uint32_t aspect_h, std::uint32_t window_w, std::uint32_t window_h);
extern int   roundToUpperMultiple(int n, int grid_size);

void Project::regenerateAtlasExport()
{
  static constexpr int k_FramePadding = 0;

  if (!m_AtlasModified)
  {
    return;
  }

  m_AtlasModified = false;

  if (m_IsRegeneratingAtlas)
  {
    return;
  }

  const int num_images = m_ImageLibrary->numImages();

  if (num_images)
  {
    m_IsRegeneratingAtlas = true;

    const auto&                  loaded_images_keys = m_ImageLibrary->loadedImageList();
    QMap<QString, std::uint32_t> frame_to_index     = {};
    const unsigned int           atlas_width        = roundToUpperMultiple(m_SpriteSheetImageSize, m_SpriteSheetFrameSize);  // TODO(SR): This policy is probably stupid and makes 'm_SpriteSheetImageSize' nearly useless from the user's perspectiv.e
    const unsigned int           num_frame_cols     = atlas_width / m_SpriteSheetFrameSize;
    const unsigned int           num_frame_rows     = std::ceil(static_cast<float>(num_images) / static_cast<float>(num_frame_cols));
    const unsigned int           atlas_height       = num_frame_rows * m_SpriteSheetFrameSize;
    unsigned int                 current_x          = 0;
    unsigned int                 current_y          = 0;
    std::uint32_t                current_frame      = 0;
    std::vector<QRect>           frame_rects        = {};
    auto&                        image_rects        = m_Export.image_rectangles;

    QProgressDialog progress("Generating Spritesheet", "Cancel", 0, num_images + 1, &m_UI);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);

    // TODO(Shareef): THE MAX SIZE OF QPixmap is '32767'x'32767'. This is buggy....
    // [https://doc.qt.io/qt-6/qpainter.html#limitations]

    QImage atlas_image((int)atlas_width, (int)atlas_height, QImage::Format_ARGB32);
    atlas_image.fill(0x00000000);

    QPainter painter(&atlas_image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    image_rects.clear();
    frame_rects.reserve(loaded_images_keys.size());

    for (const QString& abs_image_path : loaded_images_keys)
    {
#define OPTIMIZE_USE_SLOW_SCALING 0
#define OPTIMIZE_USE_PIXMAP 1

#if OPTIMIZE_USE_PIXMAP
      const QPixmap image(abs_image_path);
#else
      const QImage image(abs_image_path);
#endif
      if (!image.isNull())
      {
        const auto  frame_src   = m_ImageLibrary->findFrameSource(abs_image_path);
        const QSize scaled_size = QSize(m_SpriteSheetFrameSize - k_FramePadding, m_SpriteSheetFrameSize - k_FramePadding);

#if OPTIMIZE_USE_SLOW_SCALING
#if OPTIMIZE_USE_PIXMAP
        const QPixmap scaled_image = image.scaled(scaled_size.width(), scaled_size.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
#else
        const QImage scaled_image = image.scaled(scaled_size.width(), scaled_size.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
#endif
        const auto offset_x = (m_SpriteSheetFrameSize - scaled_image.width()) / 2;
        const auto offset_y = (m_SpriteSheetFrameSize - scaled_image.height()) / 2;
#else
        const QRect  scaled_image = aspectRatioDrawRegion(image.width(), image.height(), scaled_size.width(), scaled_size.height());
        const auto   offset_x     = scaled_image.x();
        const auto   offset_y     = scaled_image.y();
#endif

#if OPTIMIZE_USE_SLOW_SCALING
        const auto draw_point = QPoint(current_x + offset_x, current_y + offset_y);

#if OPTIMIZE_USE_PIXMAP
        painter.drawPixmap(draw_point, scaled_image);
#else
        painter.drawImage(draw_point, scaled_image);
#endif
#else
        const QPoint target_loc   = QPoint(current_x + offset_x, current_y + offset_y);
        const QRect  target_rect  = QRect(target_loc, scaled_image.size());
        const QRect  source_rect  = QRect(0, 0, image.width(), image.height());

#if OPTIMIZE_USE_PIXMAP
        painter.drawPixmap(target_rect, image, source_rect);
#else
        painter.drawImage(target_rect, image, source_rect, Qt::AutoColor);
#endif
#endif
        const std::uint32_t image_drawn_x = current_x;
        const std::uint32_t image_drawn_y = current_y;
        const std::uint32_t image_drawn_w = m_SpriteSheetFrameSize;
        const std::uint32_t image_drawn_h = m_SpriteSheetFrameSize;

        image_rects.emplace_back(image_drawn_x, image_drawn_y, image_drawn_w, image_drawn_h);

        const QRect frame_rect = QRect(image_drawn_x + offset_x, image_drawn_y + offset_y, scaled_image.width(), scaled_image.height());

        frame_rects.emplace_back(frame_rect);

        current_x += m_SpriteSheetFrameSize;

        if (current_x >= atlas_width)
        {
          current_x = 0;
          current_y += m_SpriteSheetFrameSize;
        }

        frame_to_index[abs_image_path] = current_frame;
        ++current_frame;
      }
      else
      {
        QMessageBox::warning(&m_UI, "Error", "Failed to load image: \'" + abs_image_path + "\'");
      }

      progress.setValue(current_frame);
    }

    painter.end();

    progress.setValue(progress.value() + 1);

    m_Export.frame_to_index = std::move(frame_to_index);
    m_Export.image          = atlas_image;
    m_Export.pixmap         = QPixmap::fromImage(m_Export.image);

    m_AtlasModified = false;

    if (g_Server)
    {
      if (m_EditUUID.isNull())
      {
        m_EditUUID = QUuid::createUuid();
      }

      const QString    guid_str  = m_EditUUID.toString(QUuid::WithoutBraces);
      const QByteArray guid_cstr = guid_str.toLocal8Bit();

      g_Server->sendAtlasTextureChanged(m_EditUUID, m_Export.image);
    }

    // An atlas regen implies an animation regen
    regenerateAnimationExport();

    emit atlasModified(m_Export);

    m_IsRegeneratingAtlas = false;
  }
}

void Project::regenerateAnimationExport()
{
  const auto&         image_rects    = m_Export.image_rectangles;
  const std::uint32_t num_animations = std::uint32_t(m_AnimationList.rowCount());
  const std::uint32_t num_uv_frames  = std::uint32_t(image_rects.size());

  if (m_EditUUID.isNull())
  {
    m_EditUUID = QUuid::createUuid();
  }

  SpriteAnim::SpritesheetDataSizeCalculator data_size = {};

  for (std::uint32_t i = 0; i < num_animations; ++i)
  {
    Animation* const    animation      = animationAt(i);
    const QString       animation_name = animation->name();
    const auto          anim_name_8bit = animation_name.toLocal8Bit();
    const std::uint32_t name_length    = std::uint32_t(animation_name.length());

    data_size.addAnimation(name_length, animation->numFrames());
  }

  for (std::uint32_t i = 0; i < num_uv_frames; ++i)
  {
    data_size.addUVFrame();
  }

  const std::uint64_t spritesheet_chunk_size = SpriteAnim::Spritesheet::calcTotalSize(data_size);

  m_Export.atlas_data      = std::make_unique<unsigned char[]>(spritesheet_chunk_size);
  m_Export.atlas_data_size = spritesheet_chunk_size;

  static_assert(sizeof(m_EditUUID) == sizeof(uuid128), "");

  SpriteAnim::SpritesheetBuilder spritesheet_builder =
   {
    m_Export.atlas_data.get(),
    *(const uuid128*)&m_EditUUID,
    std::uint16_t(m_Export.image.width()),
    std::uint16_t(m_Export.image.height()),
    data_size,
   };

  for (std::uint32_t i = 0; i < num_animations; ++i)
  {
    Animation* const    animation      = animationAt(i);
    const QByteArray    animation_name = animation->name().toLocal8Bit();
    const std::uint32_t num_frames     = animation->numFrames();

    auto animation_builder = spritesheet_builder.addAnimation(
     string_range{animation_name.data(), std::size_t(animation_name.size())},
     num_frames);

    for (std::uint32_t j = 0; j < num_frames; ++j)
    {
      AnimationFrameInstance* const src_frame = animation->frameAt(j);

      animation_builder.addFrame(
       m_Export.frame_to_index[src_frame->full_path()],
       src_frame->frame_time);
    }
  }

  const float32 image_width_f  = float32(m_Export.image.width());
  const float32 image_height_f = float32(m_Export.image.height());

  for (std::uint32_t i = 0; i < num_uv_frames; ++i)
  {
    const QRect& img_rect = image_rects[i];

    const uint32_t x = img_rect.x();
    const uint32_t y = img_rect.y();
    const uint32_t w = img_rect.width();
    const uint32_t h = img_rect.height();

    region2Df dst_uv_frame;
    dst_uv_frame.min.x = float32(x + 0.0f) / image_width_f;
    dst_uv_frame.min.y = float32(y + 0.0f) / image_height_f;
    dst_uv_frame.max.x = float32(x + 1.0f) / image_width_f;
    dst_uv_frame.max.y = float32(y + 1.0f) / image_height_f;

    spritesheet_builder.addUVFrames(&dst_uv_frame, 1u);
  }

  SpriteAnim::Spritesheet* const spritesheet = spritesheet_builder.end();

  if (g_Server && m_SelectedAnimation != -1)
  {
    g_Server->sendAnimationFramesChanged(m_EditUUID, *animationAt(m_SelectedAnimation), m_Export.frame_to_index);
  }

  notifyAnimationChanged(m_SelectedAnimation != -1 ? animationAt(m_SelectedAnimation) : nullptr);
}

bool Project::hasAnimation(const QString& name)
{
  const int num_anims = m_AnimationList.rowCount();

  for (int i = 0; i < num_anims; ++i)
  {
    const auto index_at_i = m_AnimationList.index(i, 0);
    const auto anim_name  = m_AnimationList.data(index_at_i, Qt::DisplayRole);

    if (anim_name.toString() == name)
    {
      return true;
    }
  }

  return false;
}

void Project::recordActionImpl(const QString& name, QUndoCommand* action)
{
  action->setText(name);
  historyStack().push(action);
  m_UI.setWindowModified(true);
}

Animation* Project::animationAt(int index) const
{
  return animationAt(m_AnimationList.index(index, 0));
}

Animation* Project::animationAt(QModelIndex index) const
{
  return index.isValid() ? (Animation*)m_AnimationList.itemFromIndex(index) : nullptr;
}
