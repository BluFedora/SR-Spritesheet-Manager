#include "sr_image_library.hpp"

#include "Data/sr_project.hpp"  // Project

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
  m_FileWatcher{},
  m_AbsToFrameSrc{},
  m_LoadedImagesIndices{}
{
  setAcceptDrops(true);

  QObject::connect(this, &QTreeWidget::customContextMenuRequested, this, &ImageLibrary::onCustomCtxMenu);
  QObject::connect(&m_FileWatcher, &QFileSystemWatcher::directoryChanged, this, &ImageLibrary::onFileWatcherDirOrFile);
  QObject::connect(&m_FileWatcher, &QFileSystemWatcher::fileChanged, this, &ImageLibrary::onFileWatcherDirOrFile);
}

QJsonObject ImageLibrary::serialize(Project& project)
{
  return serializeImpl(project, invisibleRootItem());
}

void ImageLibrary::deserialize(Project& project, const QJsonObject& data)
{
  m_LoadedImagesIndices.clear();
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

  folder_item->addChildren(selectedItems());

  editItem(folder_item, 0);
}

void ImageLibrary::onCustomCtxMenu(const QPoint& pos)
{
  QMenu right_click;

  QObject::connect(right_click.addAction(tr("Create Folder")), &QAction::triggered, m_Project, &Project::onCreateNewFolder);

  auto remove_items_action = right_click.addAction(tr("Remove Selected Items"));

  remove_items_action->setEnabled(!selectedItems().isEmpty());

  QObject::connect(remove_items_action, &QAction::triggered, this, &ImageLibrary::removeSelectedItems);

  right_click.exec(mapToGlobal(pos));
}

void ImageLibrary::onFileWatcherDirOrFile(const QString& path)
{
  (void)path;

  emit signalImagesChanged();
}

bool ImageLibrary::removeSelectedItems()
{
  const auto selected_items = selectedItems();

  if (!selected_items.isEmpty())
  {
    std::vector<ItemToDeleteInfo>                    items_to_delete_info     = {};
    std::unordered_map<Animation*, std::vector<int>> anim_to_frames_to_delete = {};
    bool                                             do_removal               = true;

    items_to_delete_info.reserve(selected_items.size());

    for (auto* const item : selected_items)
    {
      checkForFramesInUse(item, items_to_delete_info, anim_to_frames_to_delete);
    }

    if (!items_to_delete_info.empty())
    {
      QString message = "The frames you are about to remove are used in these animations:\n\n";

      for (const ItemToDeleteInfo& info : items_to_delete_info)
      {
        if (!info.animations.empty())
        {
          message += info.frame_name;
          message += ":\n";

          for (const Animation* anim_info : info.animations)
          {
            message += "  ";
            message += anim_info->name();
            message += "\n";
          }
        }
      }

      message += "\nAre you sure you want to remove these images?";

      do_removal = QMessageBox::warning(this, "Frames Used By Animations", message, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Ok;
    }

    if (do_removal)
    {
      m_Project->recordAction(tr("Remove Imported Items"), UndoActionFlag_ModifiedAtlas, [&selected_items, &anim_to_frames_to_delete, this]() {
        for (const auto& anim : anim_to_frames_to_delete)
        {
          const auto& frame_indices = anim.second;

          // This reverse makes it safe to remove items in order from the animation without invalidating indices.
          std::for_each(frame_indices.rbegin(), frame_indices.rend(), [&anim](int frame_index) {
            anim.first->frames.erase(anim.first->frames.begin() + frame_index);
          });
        }

        for (auto* const item : selected_items)
        {
          const QString abs_path = item->data(0, ImageLibraryRole::AbsolutePath).toString();

          m_FileWatcher.removePath(abs_path);
          m_AbsToFrameSrc.remove(abs_path);
          delete item;
        }
      });
    }

    return true;
  }

  return false;
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
    if (removeSelectedItems())
    {
      event->accept();
    }
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

  std::sort(files.begin(), files.end(), collator);

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
  if (!m_AbsToFrameSrc.contains(img_path))
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

      frame_src_data->index = m_LoadedImagesIndices.size();

      image_item->setFlags(image_item->flags() & ~Qt::ItemIsDropEnabled);
      image_item->setData(0, ImageLibraryRole::AbsolutePath, img_path);
      image_item->setData(0, ImageLibraryRole::FrameSource, QVariant::fromValue(frame_src_data));
      image_item->setData(0, Qt::ToolTipRole, tooltip);

      m_AbsToFrameSrc.insert(img_path, frame_src_data);
      m_FileWatcher.addPath(img_path);
      m_LoadedImagesIndices.push_back(img_path);

      if (emit_signal)
      {
        emit signalImagesAdded();
      }
    }
  }
}

void ImageLibrary::checkForFramesInUse(QTreeWidgetItem* item, std::vector<ItemToDeleteInfo>& items_to_delete_info, std::unordered_map<Animation*, std::vector<int>>& anim_to_frames_to_delete)
{
  // Is Folder
  if (item->flags() & Qt::ItemIsDropEnabled)
  {
    for (int i = 0; i < item->childCount(); ++i)
    {
      checkForFramesInUse(item->child(i), items_to_delete_info, anim_to_frames_to_delete);
    }
  }
  else
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
      bool found_use         = false;

      for (int i = 0; i < m_Project->numAnimations(); ++i)
      {
        Animation* const anim       = m_Project->animationAt(i);
        const int        num_frames = anim->numFrames();
        Animation*       anim_info  = nullptr;

        for (int j = 0; j < num_frames; ++j)
        {
          AnimationFrameInstance* const frame = anim->frameAt(j);

          if (frame->source == frame_src_info)
          {
            auto& frames_to_delete = anim_to_frames_to_delete[anim];

            // Starting the search backwards is better since we loop through 'j' in order.
            if (std::find(frames_to_delete.rbegin(), frames_to_delete.rend(), j) == frames_to_delete.rend())
            {
              if (!anim_info)
              {
                anim_info = info.animations.emplace_back(anim);
              }

              frames_to_delete.push_back(j);
              found_use = true;
            }
          }
        }
      }

      if (!found_use)
      {
        items_to_delete_info.pop_back();
      }
    }
  }
}
