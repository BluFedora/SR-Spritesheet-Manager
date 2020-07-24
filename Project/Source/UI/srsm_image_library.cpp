#include "srsm_image_library.hpp"

#include "Data/srsm_project.hpp"  // Project

#include <QCollator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QMimeData>

ImageLibrary::ImageLibrary(QWidget* parent) :
  QTreeWidget(parent),
  m_LoadedImages{},
  m_FileWatcher{}
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
  clear();

  deserializeImpl(project, invisibleRootItem(), data);
}

void ImageLibrary::addImage(const QString& img_path, bool emit_signal)
{
  addImage(invisibleRootItem(), img_path, emit_signal);
}

void ImageLibrary::addNewFolder()
{
  ImageItem* folder_item = new ImageItem(this, QStringList{"New Folder"});

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
      auto& history_stack = m_Project->historyStack();

      history_stack.push(makeUndoAction(m_Project, tr("Remove Images / Folders"), [&selected_items]() {
        for (auto* const item : selected_items)
        {
          delete item;
        }
      }));
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

      items.push_back(
       serializeImpl(project, child));
    }

    return_value["name"]  = item->data(0, Qt::DisplayRole).toString();
    return_value["items"] = items;
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

    for (const auto& item : items)
    {
      const auto item_obj = item.toObject();

      if (item_obj["type"] == "folder")
      {
        ImageItem* folder_item = new ImageItem(parent, QStringList{item_obj["name"].toString()});

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

void ImageLibrary::addImage(QTreeWidgetItem* parent, const QString& img_path, bool emit_signal)
{
  if (!m_LoadedImages.contains(img_path))
  {
    const QImageReader reader(img_path);

    if (!reader.format().isEmpty())
    {
      const QFileInfo finfo(img_path);
      const QDir      dir        = finfo.dir();
      const QString   frame_name = dir.dirName() + "/" + finfo.fileName();

      ImageItem* const image_item = new ImageItem(parent, QStringList{frame_name});

      image_item->setFlags(image_item->flags() & ~Qt::ItemIsDropEnabled);
      image_item->setData(0, Qt::UserRole, img_path);
      image_item->setData(0, Qt::ToolTipRole, img_path);

      m_LoadedImages.insert(img_path, frame_name);
      m_FileWatcher.addPath(img_path);

      if (emit_signal)
      {
        emit signalImagesAdded();
      }
    }
  }
}

QJsonObject ImageItem::serialize(Project& project, QMap<QString, QString>& loaded_images)
{
  QJsonObject return_value = {};

  if (isFolder())
  {
    return_value["type"] = "folder";

    QJsonArray items = {};

    const int num_children = childCount();

    for (int i = 0; i < num_children; ++i)
    {
      ImageItem* const child = (ImageItem*)this->child(i);

      items.push_back(child->serialize(project, loaded_images));
    }

    return_value["name"]  = this->data(0, Qt::DisplayRole).toString();
    return_value["items"] = items;
  }
  else
  {
    const QDir&   dir      = project.projectFolder();
    const QString abs_path = data(0, Qt::UserRole).toString();

    QFileInfo fi{abs_path};

    fi.makeAbsolute();

    const QString rel_path = dir.relativeFilePath(fi.filePath());

    return_value["type"]     = "image";
    return_value["rel_path"] = rel_path;
  }

  return return_value;
}
