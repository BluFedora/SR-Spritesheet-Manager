//
// SR Spritesheet Manager
//
// file:   srsm_timeline.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SRSM_TIMELINE_HPP
#define SRSM_TIMELINE_HPP

#include <QTimer>
#include <QWidget>

#include <set>  // set<T>

namespace Ui
{
  class Timeline;
}

struct AtlasExport;
struct Animation;
struct AnimationFrameSource;

enum class FrameDragMode
{
  None,
  Image,
  Left,
  Right,
};

struct FrameRectInfo final
{
  QRect                 image;
  QRect                 left_resize;
  QRect                 right_resize;
  float                 frame_time;
  QRect                 frame_uv_rect;
  AnimationFrameSource* frame_src;

  FrameRectInfo(
   const QRect&          image,
   const QRect&          left_resize,
   const QRect&          right_resize,
   float                 frame_time,
   const QRect&          frame_uv_rect,
   AnimationFrameSource* frame_src) :
    image{image},
    left_resize{left_resize},
    right_resize{right_resize},
    frame_time{frame_time},
    frame_uv_rect{frame_uv_rect},
    frame_src{frame_src}
  {
  }
};

struct FrameInfoAtPoint
{
  int           frame_rect_index = -1;
  QPoint        drag_offset      = {0, 0};
  FrameDragMode drag_mode        = FrameDragMode::None;
};

struct FrameDropInfoAtPoint final : public FrameInfoAtPoint
{
};

//
// Timeline Operations
// - Resize Frame timing
// - Drop frames onto timeline
// - Select Frames
// - Delete Frames
// - Reorder Frames
//

// Inclusive Range [start_idx, end_idx]
struct TimelineSelectionItem final
{
  int start_idx;
  int end_idx;

  bool isValid() const;
  bool contains(int item) const;
};

struct TimelineSelection final
{
  std::set<int>         selection        = {};
  int                   pivot_index      = 0;
  TimelineSelectionItem active_selection = {-1, -1};

  bool isEmpty() const;
  void clear();
  void pivotClick(int item, bool keep_old_seletion = false);
  void extendClick(int item);
  bool isSelected(int item) const;

  template<typename F>
  void forEachSelectedItem(F&& f) const
  {
    for (int item : selection)
    {
      f(item);
    }

    if (active_selection.isValid())
    {
      for (int item = active_selection.start_idx; item <= active_selection.end_idx; ++item)
      {
        if (selection.find(item) == selection.end())
        {
          f(item);
        }
      }
    }
  }

  int numSelected() const
  {
    int count = 0;

    forEachSelectedItem([&count](int) { ++count; });

    return count;
  }

  void select(int item);

 private:
  void toggle(int item);
};

class Timeline final : public QWidget
{
  Q_OBJECT

 private:
  Ui::Timeline*              ui;
  int                        m_FrameHeight;
  Animation*                 m_CurrentAnimation;
  AtlasExport*               m_AtlasExport;
  std::vector<FrameRectInfo> m_FrameInfos;
  std::vector<FrameRectInfo> m_DesiredFrameInfos;
  int                        m_DraggedFrameInfo;
  QPoint                     m_DraggedOffset;
  FrameDragMode              m_DragMode;
  FrameInfoAtPoint           m_DroppedFrameInfo;
  QRect                      m_HoveredRect;

  union
  {
    bool m_ResizedFrame;
  };

  TimelineSelection m_Selection;
  FrameInfoAtPoint  m_ActiveDraggedItem;
  FrameInfoAtPoint  m_HoveredDraggedItem;
  FrameInfoAtPoint  m_ReorderDropLocation;
  QPoint            m_MouseDownLocation;
  QTimer            m_UpdateTimer;

 public:
  explicit Timeline(QWidget* parent = nullptr);
  ~Timeline();

 public slots:
  void onFrameSizeChanged(int new_value);
  void onAnimationSelected(Animation* anim);
  void onAnimationChanged(Animation* anim);
  void onAtlasUpdated(AtlasExport& atlas);
  void onTimerTick();

  // QWidget interface
 protected:
  void paintEvent(QPaintEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  // QObject interface
 public:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void                 recalculateTimelineSize();
  void                 calculateDesiredLayout(bool use_selected_items);
  void                 copyOverAnimtionData();
  int                  numFrames() const;
  void                 drawFrame(const QPixmap& atlas_image, QPainter& painter, int index);
  FrameInfoAtPoint     infoAt(const QPoint& local_mouse_pos, bool allow_active_item) const;
  FrameDropInfoAtPoint dropInfoAt(const QPoint& local_mouse_pos);  // Uses 'logical' frame positioning rather than 'physical' layout
};

#endif  // SRSM_TIMELINE_HPP
