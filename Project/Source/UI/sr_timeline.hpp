//
// SR Spritesheet Manager
//
// file:   srsm_timeline.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SRSM_TIMELINE_HPP
#define SRSM_TIMELINE_HPP

#include <QScrollArea>
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
  std::set<int>         selection             = {};
  int                   pivot_index           = 0;
  TimelineSelectionItem active_selection      = {-1, -1};
  mutable std::set<int> fully_ordered_indices = {};  // A small cache for 'forEachSelectedItem<bool, F>'.

  bool isEmpty() const;
  void clear();
  void pivotClick(int item, bool keep_old_seletion = false);
  void extendClick(int item);
  bool isSelected(int item) const;

  //
  // Goes from least to greatest order unless reversed_iteration is set to true.
  //
  template<bool reversed_iteration = false, typename F>
  void forEachSelectedItem(F&& f) const
  {
    updateFullyOrderedIndices();

    if constexpr (reversed_iteration)
    {
      std::for_each(fully_ordered_indices.rbegin(), fully_ordered_indices.rend(), std::forward<F>(f));
    }
    else
    {
      std::for_each(fully_ordered_indices.begin(), fully_ordered_indices.end(), std::forward<F>(f));
    }
  }

  int numSelected() const
  {
    updateFullyOrderedIndices();
    return int(fully_ordered_indices.size());
  }

  void select(int item);

 private:
  void toggle(int item);
  void updateFullyOrderedIndices() const;
};

class Timeline final : public QWidget
{
  Q_OBJECT

 private:
  // UI

  Ui::Timeline* ui;
  QScrollArea*  m_ParentScrollArea;
  int           m_FrameHeight;

  // Frame Drawing

  Animation*                 m_CurrentAnimation;
  AtlasExport*               m_AtlasExport;
  std::vector<FrameRectInfo> m_FrameInfos;
  std::vector<FrameRectInfo> m_DesiredFrameInfos;

  // Old Mouse Interaction

  int              m_DraggedFrameInfo;
  QPoint           m_DraggedOffset;
  FrameDragMode    m_DragMode;
  FrameInfoAtPoint m_DroppedFrameInfo;
  QRect            m_HoveredRect;
  bool             m_ResizedFrame;

  // Mouse Interaction

  TimelineSelection m_Selection;
  FrameInfoAtPoint  m_ActiveDraggedItem;
  FrameInfoAtPoint  m_HoveredDraggedItem;
  FrameInfoAtPoint  m_ReorderDropLocation;
  QPoint            m_MouseDownLocation;
  QRect             m_ScrubberTrackRect;
  bool              m_IsDraggingScrubber;
  bool              m_MouseIsDown;
  QTimer            m_UpdateTimer;

 public:
  explicit Timeline(QWidget* parent = nullptr);

  void setup(QScrollArea* scroll_area);

  ~Timeline();

 public slots:
  void onFrameSizeChanged(int new_value);
  void onAnimationSelected(Animation* anim);
  void onAnimationChanged(Animation* anim);
  void onAtlasUpdated(AtlasExport& atlas);
  void onTimerTick();

 private slots:
  void onCustomCtxMenu(const QPoint& pos);

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
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void             recalculateTimelineSize(bool new_anim = false);
  void             calculateDesiredLayout(bool use_selected_items);
  int              numFrames() const;
  void             drawFrame(const QPixmap& atlas_image, QPainter& painter, int index);
  FrameInfoAtPoint infoAt(const QPoint& local_mouse_pos, bool allow_active_item, bool allow_last_frame_right_ext = false) const;
  FrameInfoAtPoint dropInfoAt(const QPoint& local_mouse_pos);  // Uses 'logical' frame positioning rather than 'physical' layout
  bool             removeSelectedFrames();
  QRect            selectionRect() const;
};

#endif  // SRSM_TIMELINE_HPP
