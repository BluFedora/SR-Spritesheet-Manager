//
// SR Spritesheet Manager
//
// file:   srsm_animation.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef ANIMATION_HPP
#define ANIMATION_HPP

#include <QObject>             // QObject
#include <QStandardItemModel>  // QStandardItemModel
#include <QString>             // QString
#include <QStringListModel>    // QStringListModel

#include <memory>  // unique_ptr<T>
#include <vector>  // vector<T>

#if 0
class StringListModel : public QStringListModel
{
  private:
    QMap<QModelIndex, QMap<int, QVariant>> m_DataMap;

    // QAbstractItemModel interface
  public:
    QVariant data(const QModelIndex& index, int role) const override
    {
      if (role >= Qt::UserRole)
      {
        return m_DataMap[index][role];
      }

      return QStringListModel::data(index, role);
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
      if (role >= Qt::UserRole)
      {
        m_DataMap[index][role] = value;
        return true;
      }

      return QStringListModel::setData(index, value, role);
    }

    // QAbstractItemModel interface
  public:
    QMimeData* mimeData(const QModelIndexList& indexes) const override
    {
      QMimeData* mime_data = QStringListModel::mimeData(indexes);

      return mime_data;
    }
};
#endif

struct AnimationFrame final : public QStandardItem
{
  explicit AnimationFrame(const QString& rel_path, const QString& full_path, float frame_time);

  QString rel_path() const { return data(Qt::UserRole + 1).toString(); }
  QString full_path() const { return data(Qt::UserRole + 2).toString(); }
  float   frame_time() const { return data(Qt::UserRole + 3).toFloat(); }

  void setFrameTime(float value)
  {
    setData(value, Qt::UserRole + 3);
  }

  // QStandardItem interface
 public:
  QStandardItem* clone() const override;
  int            type() const override;
};

class Project;

struct Animation final : public QStandardItem
{
 public:
  Project*           parent;
  int                frame_rate;  //!< Units are in frames per second.
  QStandardItemModel frame_list;  //!< Contains 'AnimationFrame' items.

 public:
  Animation(Project* parent, const QString& name, int fps);

  QString name() const { return data(Qt::DisplayRole).toString(); }

  void            addFrame(const QString& rel_path, const QString& full_path);
  void            addFrame(AnimationFrame* frame);
  AnimationFrame* frameAt(int index) const;
  void            swapFrames(int a, int b);

  void notifyChanged();
};

using AnimationPtr = std::unique_ptr<Animation>;

#endif  // ANIMATION_HPP
