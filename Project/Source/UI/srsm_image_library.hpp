#ifndef IMAGELIBRARY_HPP
#define IMAGELIBRARY_HPP

#include <QFileSystemWatcher>
#include <QTreeWidget>

class Project;
struct Animation;

class ImageItem final : public QTreeWidgetItem
{
 public:
  using QTreeWidgetItem::QTreeWidgetItem;

  QJsonObject serialize(Project& project, QMap<QString, QString>& loaded_images);

  bool isFolder() const { return flags() & Qt::ItemIsDropEnabled; }
  bool isImage() const { return !isFolder(); }
};

class ImageLibrary : public QTreeWidget
{
  Q_OBJECT

  friend class Project;

 private:
  Project*               m_Project;
  QMap<QString, QString> m_LoadedImages;
  QFileSystemWatcher     m_FileWatcher;

 public:
  ImageLibrary(QWidget* parent);

  int                           numImages() const { return m_LoadedImages.uniqueKeys().size(); }
  const QMap<QString, QString>& loadedImages() const { return m_LoadedImages; }

  QJsonObject serialize(Project& project);
  void        deserialize(Project& project, const QJsonObject& data);
  void        addImage(const QString& img_path, bool emit_signal = true);
  void        addNewFolder();
  void        addDirectory(QStringList& files);
  void        addUrls(const QList<QUrl>& urls);

 signals:
  void signalImagesAdded();
  void signalImagesChanged();

 private slots:
  void onCustomCtxMenu(const QPoint& pos);
  void onFileWatcherDirOrFile(const QString& path);

  // QWidget interface
 protected:
  void dropEvent(QDropEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  QJsonObject serializeImpl(Project& project, QTreeWidgetItem* item);
  void        deserializeImpl(Project& project, QTreeWidgetItem* parent, const QJsonObject& data);
  void        addImage(QTreeWidgetItem* parent, const QString& img_path, bool emit_signal = true);
};

#endif  // IMAGELIBRARY_HPP
