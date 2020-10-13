#ifndef SRSM_IMAGELIBRARY_HPP
#define SRSM_IMAGELIBRARY_HPP

#include <QFileSystemWatcher>
#include <QTreeWidget>

#include <memory>

class Project;
struct Animation;

struct AnimationFrameSource final : public std::enable_shared_from_this<AnimationFrameSource>
{
  QString full_path;
  QString rel_path;
  int     index;

  AnimationFrameSource(const QString& full_path, const QString& rel_path) :
    full_path{full_path},
    rel_path{rel_path},
    index{-1}
  {
  }
};

using AnimationFrameSourcePtr = std::shared_ptr<AnimationFrameSource>;

QDataStream& operator<<(QDataStream& stream, const AnimationFrameSourcePtr& data);
QDataStream& operator>>(QDataStream& stream, AnimationFrameSourcePtr& data);

Q_DECLARE_METATYPE(std::shared_ptr<AnimationFrameSource>)

enum ImageLibraryRole
{
  RelativePath = Qt::DisplayRole,
  AbsolutePath = Qt::UserRole + 0,
  FrameSource  = Qt::UserRole + 1,
};

struct ItemToDeleteInfo final
{
  QString                 frame_name;
  std::vector<Animation*> animations;
};

class ImageLibrary : public QTreeWidget
{
  Q_OBJECT

  friend class Project;

 private:
  Project*                               m_Project;
  QFileSystemWatcher                     m_FileWatcher;
  QMap<QString, AnimationFrameSourcePtr> m_AbsToFrameSrc;
  QVector<QString>                       m_LoadedImagesIndices;

 public:
  ImageLibrary(QWidget* parent);

  int                     numImages() const { return m_AbsToFrameSrc.uniqueKeys().size(); }
  const QVector<QString>& loadedImageList() const { return m_LoadedImagesIndices; }

  QJsonObject             serialize(Project& project);
  void                    deserialize(Project& project, const QJsonObject& data);
  void                    addImage(const QString& img_path, bool emit_signal = true);
  void                    addNewFolder();
  void                    addDirectory(QStringList& files, bool emit_signal = true);
  void                    addUrls(const QList<QUrl>& urls);
  AnimationFrameSourcePtr findFrameSource(const QString& abs_img_path);

 signals:
  void signalImagesAdded();
  void signalImagesChanged();

 private slots:
  void onCustomCtxMenu(const QPoint& pos);
  void onFileWatcherDirOrFile(const QString& path);
  bool removeSelectedItems();

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
  void        checkForFramesInUse(QTreeWidgetItem* item, std::vector<ItemToDeleteInfo>& items_to_delete_info, std::unordered_map<Animation*, std::vector<int>>& anim_to_frames_to_delete);
};

#endif  // SRSM_IMAGELIBRARY_HPP
