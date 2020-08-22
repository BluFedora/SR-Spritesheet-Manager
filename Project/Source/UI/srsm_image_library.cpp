#include "srsm_image_library.hpp"

#include "Data/srsm_project.hpp"  // Project

#include <QCollator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>

QDataStream& operator<<(QDataStream& stream, const AnimationFrameSourcePtr& data)
{
  std::uintptr_t ptr_val = reinterpret_cast<std::uintptr_t>(data.get());

  stream << ptr_val;

  return stream;
}

QDataStream& operator>>(QDataStream& stream, AnimationFrameSourcePtr& data)
{
  std::uintptr_t ptr_val = 0x0;

  stream >> ptr_val;

  AnimationFrameSource* const ptr = reinterpret_cast<AnimationFrameSource*>(ptr_val);

  data = ptr->shared_from_this();

  return stream;
}

ImageLibrary::ImageLibrary(QWidget* parent) :
  QTreeWidget(parent),
  m_LoadedImages{},
  m_FileWatcher{},
  m_AbsToFrameSrc{}
{
  setAcceptDrops(true);

  QObject::connect(this, &QTreeWidget::customContextMenuRequested, this, &ImageLibrary::onCustomCtxMenu);
  QObject::connect(&m_FileWatcher, &QFileSystemWatcher::directoryChanged, this, &ImageLibrary::onFileWatcherDirOrFile);
  QObject::connect(&m_FileWatcher, &QFileSystemWatcher::fileChanged, this, &ImageLibrary::onFileWatcherDirOrFile);
}

QJsonObject ImageLibrary::serialize(Project& project)
{
  QJsonObject result = serializeImpl(project, invisibleRootItem());

  return result;
}

void ImageLibrary::deserialize(Project& project, const QJsonObject& data)
{
  m_LoadedImages.clear();
  m_AbsToFrameSrc.clear();
  clear();

  deserializeImpl(project, invisibleRootItem(), data);
}

void ImageLibrary::addImage(const QString& img_path, bool emit_signal)
{
  addImage(invisibleRootItem(), img_path, emit_signal);
}

void ImageLibrary::addNewFolder()
{
  QTreeWidgetItem* folder_item = new QTreeWidgetItem(this, QStringList{"New Folder"});

  folder_item->setFlags(folder_item->flags() | Qt::ItemIsEditable);

  editItem(folder_item, 0);
}

void ImageLibrary::onCustomCtxMenu(const QPoint& pos)
{
  QMenu right_click;

  QObject::connect(right_click.addAction("Create Folder"), &QAction::triggered, m_Project, &Project::onCreateNewFolder);

  right_click.exec(mapToGlobal(pos));
}

void ImageLibrary::onFileWatcherDirOrFile(const QString& path)
{
  (void)path;

  emit signalImagesChanged();
}

void ImageLibrary::dropEvent(QDropEvent* event)
{
  const QMimeData* mimedata = event->mimeData();

  if (mimedata->hasUrls())
  {
    m_Project->importImageUrls(mimedata->urls());
    event->acceptProposedAction();
  }
  else
  {
    QTreeWidget::dropEvent(event);
  }
}

void ImageLibrary::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls())
  {
    event->acceptProposedAction();
  }
  else
  {
    QTreeWidget::dragEnterEvent(event);
  }
}

void ImageLibrary::dragMoveEvent(QDragMoveEvent* event)
{
  if (event->mimeData()->hasUrls())
  {
    event->acceptProposedAction();
  }
  else
  {
    QTreeWidget::dragMoveEvent(event);
  }
}

void ImageLibrary::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
  {
    const auto selected_items = selectedItems();

    if (!selected_items.isEmpty())
    {
      struct AnimRemoveData final
      {
        Animation*       anim;
        std::vector<int> frame_indices;
      };

      struct ItemToDeleteInfo final
      {
        QString                     frame_name;
        std::vector<AnimRemoveData> animations;
      };

      const int                     num_animations       = m_Project->numAnimations();
      std::vector<ItemToDeleteInfo> items_to_delete_info = {};
      bool                          do_removal           = true;

      items_to_delete_info.reserve(selected_items.size());

      for (auto* const item : selected_items)
      {
        // There are 3 references to the shared pointer at ths point in the code.
        //   1 - local [frame_src_info]
        //   2 - Stored into the Actual item->data
        //   3 - m_AbsToFrameSrc
        static const long k_MinUseCount = 3;

        const auto frame_src_info = item->data(0, ImageLibraryRole::FrameSource).value<AnimationFrameSourcePtr>();

        if (frame_src_info.use_count() > k_MinUseCount)
        {
          ItemToDeleteInfo& info = items_to_delete_info.emplace_back();
          info.frame_name        = item->data(0, ImageLibraryRole::RelativePath).toString();

          for (int i = 0; i < num_animations; ++i)
          {
            Animation* const anim       = m_Project->animationAt(i);
            const int        num_frames = anim->numFrames();
            AnimRemoveData*  anim_info  = nullptr;

            for (int j = 0; j < num_frames; ++j)
            {
              AnimationFrameInstance* const frame = anim->frameAt(j);

              if (frame->source == frame_src_info)
              {
                if (!anim_info)
                {
                  anim_info       = &info.animations.emplace_back();
                  anim_info->anim = anim;
                }

                anim_info->frame_indices.emplace_back(j);
              }
            }

            if (anim_info)
            {
              // This reverse makes it safe to remove items in order from the animation without invalidating indices.
              std::reverse(anim_info->frame_indices.begin(), anim_info->frame_indices.end());
            }
          }
        }
      }

      if (!items_to_delete_info.empty())
      {
        QString message = "The frames you are about to remove are used in these animations:\n\n";

        for (const ItemToDeleteInfo& info : items_to_delete_info)
        {
          message += info.frame_name;
          message += ":\n";

          for (const AnimRemoveData& anim_info : info.animations)
          {
            message += "  ";
            message += anim_info.anim->name();
            message += "\n";
          }
        }

        message += "\nAre you sure you want to remove these images?";

        do_removal = QMessageBox::warning(this, "Frames Used By Animations", message, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Ok;
      }

      if (do_removal)
      {
        m_Project->recordAction(tr("Remove Imported Items"), UndoActionFlag_ModifiedAtlas, [&selected_items, &items_to_delete_info, this]() {
          if (!items_to_delete_info.empty())
          {
            for (const ItemToDeleteInfo& info : items_to_delete_info)
            {
              for (const AnimRemoveData& anim_info : info.animations)
              {
                for (int frame_index : anim_info.frame_indices)
                {
                  // TODO(SR):
                  // anim_info.anim->removeFrameAt(frame_index);
                }
              }
            }
          }

          for (auto* const item : selected_items)
          {
            const QString abs_path = item->data(0, ImageLibraryRole::AbsolutePath).toString();

            m_LoadedImages.remove(abs_path);
            m_FileWatcher.removePath(abs_path);
            m_AbsToFrameSrc.remove(abs_path);
            delete item;
          }
        });
      }
    }

    event->accept();
  }
  else
  {
    QTreeWidget::keyPressEvent(event);
  }
}

QJsonObject ImageLibrary::serializeImpl(Project& project, QTreeWidgetItem* item)
{
  QJsonObject return_value = {};

  // This is a folder
  if (item->flags() & Qt::ItemIsDropEnabled)
  {
    return_value["type"] = "folder";

    QJsonArray items = {};

    const int num_children = item->childCount();

    for (int i = 0; i < num_children; ++i)
    {
      QTreeWidgetItem* const child = item->child(i);

      items.push_back(serializeImpl(project, child));
    }

    return_value["name"]       = item->data(0, Qt::DisplayRole).toString();
    return_value["items"]      = items;
    return_value["isExpanded"] = item->isExpanded();
  }
  else
  {
    const QDir&   dir      = project.projectFolder();
    const QString abs_path = item->data(0, Qt::UserRole).toString();

    QFileInfo fi{abs_path};

    fi.makeAbsolute();

    const QString rel_path = dir.relativeFilePath(fi.filePath());

    return_value["type"]     = "image";
    return_value["rel_path"] = rel_path;
  }

  return return_value;
}

void ImageLibrary::deserializeImpl(Project& project, QTreeWidgetItem* parent, const QJsonObject& data)
{
  if (data["type"] == "folder")
  {
    QJsonArray items = data["items"].toArray();

    parent->setExpanded(data["isExpanded"].toBool(false));

    for (const auto& item : items)
    {
      const auto item_obj = item.toObject();

      if (item_obj["type"] == "folder")
      {
        QTreeWidgetItem* folder_item = new QTreeWidgetItem(parent, QStringList{item_obj["name"].toString()});

        folder_item->setFlags(folder_item->flags() | Qt::ItemIsEditable);

        deserializeImpl(project, folder_item, item_obj);
      }
      else
      {
        const QDir& dir = project.projectFolder();
        QFileInfo   file_info(dir.filePath(item_obj["rel_path"].toString()));

        addImage(parent, file_info.canonicalFilePath(), false);
      }
    }
  }
  else
  {
    const QDir& dir = project.projectFolder();
    QFileInfo   file_info(dir.filePath(data["rel_path"].toString()));

    addImage(parent, file_info.canonicalFilePath(), false);
  }
}

void ImageLibrary::addDirectory(QStringList& files, bool emit_signal)
{
  QCollator collator;
  collator.setNumericMode(true);

  std::sort(files.begin(), files.end(), [&collator](const QString& file1, const QString& file2) -> bool {
    return collator.compare(file1, file2) < 0;
  });

  for (const auto& file : files)
  {
    addImage(file, false);
  }

  if (emit_signal)
  {
    emit signalImagesAdded();
  }
}

void ImageLibrary::addUrls(const QList<QUrl>& urls)
{
  if (!urls.isEmpty())
  {
    for (const QUrl& url : urls)
    {
      const QFileInfo file_info(url.toLocalFile());

      if (file_info.isDir())
      {
        QDir directory = file_info.absoluteFilePath();

        if (directory.exists())
        {
          QStringList files = directory.entryList(QDir::Files | QDir::Readable);

          for (QString& file : files)
          {
            file = directory.filePath(file);
          }

          addDirectory(files, false);
        }
      }
      else
      {
        addImage(file_info.filePath(), false);
      }
    }

    emit signalImagesAdded();
  }
}

AnimationFrameSourcePtr ImageLibrary::findFrameSource(const QString& abs_img_path)
{
  return m_AbsToFrameSrc.value(abs_img_path);
}

void ImageLibrary::addImage(QTreeWidgetItem* parent, const QString& img_path, bool emit_signal)
{
  if (!m_LoadedImages.contains(img_path))
  {
    const QImageReader reader(img_path);

    if (!reader.format().isEmpty())
    {
      const QFileInfo        finfo(img_path);
      const QDir             dir            = finfo.dir();
      const QString          frame_name     = finfo.fileName();
      const auto             frame_src_data = std::make_shared<AnimationFrameSource>(img_path, frame_name);
      const QString          tooltip        = QString("<img src=\"%1\" width=256 height=256> <p style=\"text-aling:center\">%2</p>").arg(img_path).arg(img_path);
      QTreeWidgetItem* const image_item     = new QTreeWidgetItem(parent, QStringList{frame_name});

      image_item->setFlags(image_item->flags() & ~Qt::ItemIsDropEnabled);
      image_item->setData(0, ImageLibraryRole::AbsolutePath, img_path);
      image_item->setData(0, ImageLibraryRole::FrameSource, QVariant::fromValue(frame_src_data));
      image_item->setData(0, Qt::ToolTipRole, tooltip);

      m_AbsToFrameSrc.insert(img_path, frame_src_data);
      m_LoadedImages.insert(img_path, frame_name);
      m_FileWatcher.addPath(img_path);

      if (emit_signal)
      {
        emit signalImagesAdded();
      }
    }
  }
}
