//
// SR Texture Packer
// Copyright (c) 2020 Shareef Aboudl-Raheem
//

#include "framelistview.hpp"

#include "Data/srsm_animation.hpp"    // QStandardItemModel, Animation
#include "Data/srsm_project.hpp"      // Project
#include "UI/srsm_image_library.hpp"  // ImageLibrary

#include <QDebug>
#include <QDragEnterEvent>
#include <QMimeData>

FrameListView::FrameListView(QWidget* parent) :
  QListView(parent),
  m_CurrentAnimation{nullptr}
{
  setAcceptDrops(true);
}

void FrameListView::onSelectAnimation(Animation* animation)
{
  setModel(animation ? &animation->frame_list : nullptr);
  m_CurrentAnimation = animation;
}

void FrameListView::dropEvent(QDropEvent* event)
{
  if (event->source() == this)
  {
    m_CurrentAnimation->parent->recordAction(tr("Edit Frames (%1)").arg(m_CurrentAnimation->name()), [this, event]() {
      QListView::dropEvent(event);
    });

    m_CurrentAnimation->notifyChanged();
  }
  else
  {
    ImageLibrary* img_lib = dynamic_cast<ImageLibrary*>(event->source());

    if (!img_lib)
    {
      return;
    }

    m_CurrentAnimation->parent->recordAction(tr("Add Frames To '%1'").arg(m_CurrentAnimation->name()), [this, event]() {
      QByteArray  encoded = event->mimeData()->data("application/x-qabstractitemmodeldatalist");
      QDataStream stream(&encoded, QIODevice::ReadOnly);

      while (!stream.atEnd())
      {
        int                 row, col;
        QMap<int, QVariant> role_data_map;

        stream >> row >> col >> role_data_map;

        if (role_data_map.contains(Qt::UserRole))
        {
          const QString rel_path = role_data_map[Qt::DisplayRole].toString();
          const QString abs_path = role_data_map[Qt::UserRole].toString();

          m_CurrentAnimation->addFrame(rel_path, abs_path);
        }
      }
    });

    event->acceptProposedAction();

    m_CurrentAnimation->notifyChanged();
  }
}

void FrameListView::dragEnterEvent(QDragEnterEvent* event)
{
  if (m_CurrentAnimation)
  {
    if (event->source() == this)
    {
      QListView::dragEnterEvent(event);
    }
    else
    {
      ImageLibrary* img_lib = dynamic_cast<ImageLibrary*>(event->source());

      if (!img_lib)
      {
        return;
      }

      event->acceptProposedAction();
    }
  }
}

void FrameListView::dragMoveEvent(QDragMoveEvent* event)
{
  if (m_CurrentAnimation)
  {
    if (event->source() == this)
    {
      QListView::dragMoveEvent(event);
    }
    else
    {
      ImageLibrary* img_lib = dynamic_cast<ImageLibrary*>(event->source());

      if (!img_lib)
      {
        return;
      }

      event->acceptProposedAction();
    }
  }
}

void FrameListView::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
  {
    auto selectedItems = selectedIndexes();

    std::sort(selectedItems.begin(), selectedItems.end(), [](const QModelIndex& idx1, const QModelIndex& idx2) -> bool {
      return idx1.row() > idx2.row();
    });

    for (QModelIndex index : selectedItems)
    {
      if (index.isValid())
      {
        model()->removeRows(index.row(), 1, index.parent());
      }
    }
  }
}
