//
// SR Spritesheet Manager
//
// file:   srsm_project.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "srsm_project.hpp"

#include "UI/srsm_image_library.hpp"  // ImageLibrary
#include "mainwindow.h"

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
  m_ProjectFile{nullptr},
  m_ImageLibrary{nullptr},
  m_HistoryStack{new QUndoStack(main_window)},
  m_AnimationList{},
  m_UI{*main_window},
  m_Export{},
  m_SelectedAnimation{-1},
  m_SelectedFrame{-1},
  m_SpriteSheetImageSize{2048},
  m_SpriteSheetFrameSize{256},
  m_AtlasModified{false},
  m_IsRegeneratingAtlas{false}
{
  QObject::connect(m_HistoryStack, &QUndoStack::indexChanged, [this](int idx) {
    if (m_AtlasModified)
    {
      qDebug() << "HS set index " << idx;
      m_AtlasModified = false;
    }
  });
}

void Project::newAnimation(const QString& name, int frame_rate)
{
  if (!hasAnimation(name))
  {
    recordAction(tr("New Animation (%1)").arg(name), [this, name, frame_rate]() {
      newAnimationRaw(name, frame_rate);
    });
  }
  else
  {
    QMessageBox::warning(&m_UI, "Warning", "You already have an animation named \'" + name + "\'");
  }
}

void Project::selectAnimation(QModelIndex index)
{
  const bool is_valid = index.isValid();

  m_SelectedAnimation = is_valid ? index.row() : -1;

  Animation* const animation = is_valid ? animationAt(m_SelectedAnimation) : nullptr;

  if (is_valid)
  {
    m_UI.timelineFpsSpinbox().setValue(animation->frame_rate);
  }

  m_UI.timelineFpsSpinbox().setEnabled(is_valid);

  emit animationSelected(is_valid ? animationAt(m_SelectedAnimation) : nullptr);
}

void Project::removeAnimation(QModelIndex index)
{
  Animation* animation = animationAt(index);

  if (animation)
  {
    recordAction(tr("Remove Animation (%1)").arg(animation->name()), [this, index]() {
      m_AnimationList.removeRow(index.row(), index.parent());
      m_UI.setWindowModified(true);
    });
  }
}

void Project::importImageUrls(const QList<QUrl>& urls)
{
  recordAction(tr("Import Dragged Images"), [this, &urls]() {
    m_ImageLibrary->addUrls(urls);
  });
}

void Project::notifyAnimationChanged(Animation* animation)
{
  emit animationChanged(animation);
}

void Project::markAtlasModifed()
{
  m_AtlasModified = true;
}

void Project::onTimelineFpsChange(int value)
{
  Animation* const animation = animationAt(m_SelectedAnimation);

  animation->frame_rate = value;

  emit animationChanged(animation);

  m_UI.setWindowModified(true);
}

void Project::onCreateNewFolder()
{
  recordAction(tr("Created New Folder"), [this]() {
    m_ImageLibrary->addNewFolder();
    m_UI.setWindowModified(true);
  });
}

void Project::onImportImages()
{
  auto files = QFileDialog::getOpenFileNames(&m_UI, "Select An Image Sequence");

  if (!files.isEmpty())
  {
    recordAction("Import Images", [this, &files]() {
      m_ImageLibrary->addDirectory(files);
      m_UI.setWindowModified(true);
    });
  }
}

void Project::newAnimationRaw(const QString& name, int frame_rate)
{
  m_AnimationList.appendRow(new Animation(this, name, frame_rate));
  m_UI.setWindowModified(true);
}

void Project::setup(ImageLibrary* img_library)
{
  m_ImageLibrary            = img_library;
  m_ImageLibrary->m_Project = this;

  QObject::connect(img_library, &ImageLibrary::signalImagesAdded, this, &Project::regenExport);
  QObject::connect(img_library, &ImageLibrary::signalImagesChanged, this, &Project::regenExport);
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

      if (deserialize(json_doc.object()))
      {
        m_UI.setWindowModified(false);

        return true;
      }

      m_ProjectFile.reset();
      return false;
    }
  }

  return false;
}

bool Project::save(QWidget* parent)
{
  if (!m_ProjectFile)
  {
    QString dir = QFileDialog::getExistingDirectory(parent, "Where To Save the Project", "", QFileDialog::ShowDirsOnly);

    if (dir.isEmpty())
    {
      return false;
    }

    m_ProjectFile = std::make_unique<QDir>(dir);
    m_ProjectFile->makeAbsolute();
  }

  const QString& json_file_path = m_ProjectFile->absoluteFilePath(m_Name + ".srsmproj.json");
  QJsonObject    project_data   = serialize();

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

  QJsonObject animations_data;

  for (int i = 0; i < m_AnimationList.rowCount(); ++i)
  {
    Animation*    anim = (Animation*)m_AnimationList.item(i, 0);
    const QString name = anim->data(Qt::DisplayRole).toString();

    QJsonArray frames_data;

    for (int j = 0; j < anim->frame_list.rowCount(); ++j)
    {
      AnimationFrame* anim_frame = (AnimationFrame*)anim->frame_list.item(j, 0);

      frames_data.push_back(QJsonObject{
       {"rel_path", anim_frame->rel_path()},
       {"full_path", anim_frame->full_path()},
       {"frame_time", anim_frame->frame_time()},
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
  };
}

bool Project::deserialize(const QJsonObject& data)
{
  if (data.contains("name") && data.contains("image_library") && data.contains("animations"))
  {
    selectAnimation(QModelIndex());
    m_AnimationList.removeRows(0, m_AnimationList.rowCount());

    const QString new_name = data.value("name").toString();

    if (new_name != m_Name)
    {
      m_Name = new_name;
      emit renamed(m_Name);
    }

    m_ImageLibrary->deserialize(*this, data.value("image_library").toObject());

    const QJsonObject animation_data = data["animations"].toObject();

    int anim_index = 0;
    for (const auto& animation_data_key : animation_data.keys())
    {
      const auto anim = animation_data[animation_data_key];

      newAnimationRaw(animation_data_key, anim["frame_rate"].toInt());

      Animation* anim_obj = (Animation*)m_AnimationList.item(anim_index, 0);

      QJsonArray frames_data = anim["frames"].toArray();

      for (const auto& frame : frames_data)
      {
        const auto frame_data = frame.toObject();

        anim_obj->addFrame(
         new AnimationFrame(
          frame_data["rel_path"].toString(),
          frame_data["full_path"].toString(),
          (float)frame_data["frame_time"].toDouble(1.0 / (double)anim_obj->frame_rate)));
      }

      ++anim_index;
    }

    m_UI.setWindowModified(true);
    regenExport();

    return true;
  }

  return false;
}

void Project::regenExport()
{
  static constexpr int k_FramePadding = 5;

  if (m_IsRegeneratingAtlas)
  {
    return;
  }

  const int num_images = m_ImageLibrary->numImages();

  if (num_images)
  {
    m_IsRegeneratingAtlas = true;

    const QMap<QString, QString>& loaded_images  = m_ImageLibrary->loadedImages();
    QMap<QString, std::uint32_t>& frame_to_index = m_Export.abs_path_to_index;
    const unsigned int            num_frame_cols = m_SpriteSheetImageSize / m_SpriteSheetFrameSize;
    const unsigned int            num_frame_rows = std::ceil(static_cast<float>(num_images) / static_cast<float>(num_frame_cols));
    const unsigned int            atlas_height   = num_frame_rows * m_SpriteSheetFrameSize;
    unsigned int                  current_x      = 0;
    unsigned int                  current_y      = 0;
    std::uint32_t                 current_frame  = 0;

    QProgressDialog progress("Generating Spritesheet", "Cancel", 0, num_images + 1, &m_UI);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);

    QImage atlas_image((int)m_SpriteSheetImageSize, (int)atlas_height, QImage::Format_ARGB32);
    atlas_image.fill(0x00000000);

    QPainter painter(&atlas_image);

    m_Export.atlas_data = std::make_unique<QBuffer>();

    QBuffer& byte_buffer = *m_Export.atlas_data;

    byte_buffer.open(QIODevice::WriteOnly);

    std::uint16_t data_offset       = 7;
    std::uint8_t  header_version    = 0;
    std::uint8_t  header_num_chunks = 3;

    byte_buffer.write("SRSM");
    byte_buffer.write((const char*)&data_offset, sizeof(data_offset));
    byte_buffer.write((const char*)&header_version, sizeof(header_version));
    byte_buffer.write((const char*)&header_num_chunks, sizeof(header_num_chunks));

    // Write "FRME" Chunk
    {
      const std::uint32_t frfm_chunk_num_frames = std::uint32_t(num_images);
      const std::uint32_t frfm_chunk_size       = sizeof(std::uint32_t) + frfm_chunk_num_frames * (sizeof(std::uint32_t) * 4);

      byte_buffer.write("FRFM");
      byte_buffer.write((const char*)&frfm_chunk_size, sizeof(frfm_chunk_size));
      byte_buffer.write((const char*)&frfm_chunk_num_frames, sizeof(frfm_chunk_num_frames));

      m_Export.frame_rects.clear();
      frame_to_index.clear();

      for (const QString& abs_image_path : loaded_images.keys())
      {
        const QImage image(abs_image_path);

        if (!image.isNull())
        {
          const QImage scaled_image = image.scaled(m_SpriteSheetFrameSize - k_FramePadding, m_SpriteSheetFrameSize - k_FramePadding, Qt::KeepAspectRatio, Qt::SmoothTransformation);
          const auto   offset_x     = (m_SpriteSheetFrameSize - scaled_image.width()) / 2;
          const auto   offset_y     = (m_SpriteSheetFrameSize - scaled_image.height()) / 2;

          painter.drawImage(QPoint(current_x + offset_x, current_y + offset_y), scaled_image);

          const std::uint32_t image_drawn_x = current_x;
          const std::uint32_t image_drawn_y = current_y;
          const std::uint32_t image_drawn_w = m_SpriteSheetFrameSize;
          const std::uint32_t image_drawn_h = m_SpriteSheetFrameSize;

          byte_buffer.write((const char*)&image_drawn_x, sizeof(image_drawn_x));
          byte_buffer.write((const char*)&image_drawn_y, sizeof(image_drawn_y));
          byte_buffer.write((const char*)&image_drawn_w, sizeof(image_drawn_w));
          byte_buffer.write((const char*)&image_drawn_h, sizeof(image_drawn_h));

          m_Export.frame_rects.emplace_back(
           image_drawn_x + offset_x,
           image_drawn_y + offset_y,
           scaled_image.width(),
           scaled_image.height());

          current_x += m_SpriteSheetFrameSize;

          if (current_x >= m_SpriteSheetImageSize)
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
    }

    painter.end();

    // Write "ANIM" Chunk
    {
      const std::uint32_t num_animations  = std::uint32_t(m_AnimationList.rowCount());
      const std::uint32_t anim_chunk_size = sizeof(std::uint32_t) + num_animations * (sizeof(std::uint32_t) + sizeof(float));

      byte_buffer.write("ANIM");
      byte_buffer.write((const char*)&anim_chunk_size, sizeof(anim_chunk_size));
      byte_buffer.write((const char*)&num_animations, sizeof(num_animations));

      for (std::uint32_t i = 0; i < num_animations; ++i)
      {
        Animation* const animation      = animationAt(i);
        const QString    animation_name = animation->name();
        const auto       anim_name_8bit = animation_name.toLocal8Bit();

        const std::uint32_t name_length = std::uint32_t(animation_name.length());

        byte_buffer.write((const char*)&name_length, sizeof(name_length));

        byte_buffer.write(anim_name_8bit.data(), name_length);
        byte_buffer.write("\0", 1);

        const std::uint32_t num_frames = std::uint32_t(animation->frame_list.rowCount());

        byte_buffer.write((const char*)&num_frames, sizeof(num_frames));

        for (std::uint32_t j = 0; j < num_frames; ++j)
        {
          AnimationFrame* const frame       = animation->frameAt(j);
          const std::uint32_t   frame_index = frame_to_index[frame->full_path()];
          const float           frame_time  = frame->frame_time();

          byte_buffer.write((const char*)&frame_index, sizeof(frame_index));
          byte_buffer.write((const char*)&frame_time, sizeof(frame_time));
        }
      }
    }

    // Write "FOOT" chunk
    {
      const std::uint32_t foot_chunk_size = 0;

      byte_buffer.write("FOOT");
      byte_buffer.write((const char*)&foot_chunk_size, sizeof(foot_chunk_size));
    }

    progress.setValue(progress.value() + 1);

    m_Export.image  = std::move(atlas_image);
    m_Export.pixmap = QPixmap::fromImage(m_Export.image);

    m_AtlasModified = false;

    emit atlasModified(m_Export);
    m_IsRegeneratingAtlas = false;
  }
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

Animation* Project::animationAt(int index)
{
  return animationAt(m_AnimationList.index(index, 0));
}

Animation* Project::animationAt(QModelIndex index)
{
  return index.isValid() ? (Animation*)m_AnimationList.itemFromIndex(index) : nullptr;
}
