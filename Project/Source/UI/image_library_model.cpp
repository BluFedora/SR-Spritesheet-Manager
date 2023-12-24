#include "image_library_model.hpp"

QModelIndex ImageLibraryModel::index(int row, int column, const QModelIndex& parent) const
{
  return QModelIndex();
}

QModelIndex ImageLibraryModel::parent(const QModelIndex& child) const
{
  return QModelIndex();
}

Q_INVOKABLE int ImageLibraryModel::rowCount(const QModelIndex& parent) const
{
  return int();
}

Q_INVOKABLE int ImageLibraryModel::columnCount(const QModelIndex& parent) const
{
  return int();
}

Q_INVOKABLE QVariant ImageLibraryModel::data(const QModelIndex& index, int role) const
{
  return QVariant();
}
