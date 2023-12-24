#pragma once

#include <QAbstractItemModel>  // QAbstractItemModel

#include <QUuid>  // QUuid

#include "containers/tagged_union.hpp"

#include <QPixmap>

struct ImportedImage
{
  QPixmap pixmap;  //!< The loaded image, use for drawing.
  QString path;    //!< Path is relative to project dir.
};

struct ImportedImageItem
{
  QUuid   id;    //!<
  QString path;  //!< Path is relative to project dir.
  bool    is_folder;
};

class ImageLibraryModel : public QAbstractItemModel
{
  // Inherited via QAbstractItemModel
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int         rowCount(const QModelIndex& parent) const override;
  int         columnCount(const QModelIndex& parent) const override;
  QVariant    data(const QModelIndex& index, int role) const override;
};
