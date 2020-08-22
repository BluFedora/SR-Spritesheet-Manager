//
// SR Spritesheet Manager
//
// file:   srsm_timeline.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "srsm_timeline.hpp"

#include "ui_srsm_timeline.h"

#include "Data/srsm_project.hpp"
#include "UI/srsm_image_library.hpp"

#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <cmath>  // round

// UI Constants //

static const QColor k_BackgroundColor             = QColor(0x45, 0x45, 0x45, 255);
static const QBrush k_BackgroundBrush             = QBrush(k_BackgroundColor);
static const QBrush k_FrameTrackOutline           = QColor(255, 255, 255, 80);
static const QBrush k_FrameTrackBrush             = QBrush(QColor(0x2B, 0x2B, 0x2B, 255));
static const int    k_FontHeight                  = 16;
static const QBrush k_TextTrackBrush              = QBrush(QColor(20, 20, 20, 255));
static const int    k_FrameTrackPadding           = 2 + k_FontHeight;
static const int    k_FramePadding                = 5;
static const int    k_DblFramePadding             = k_FramePadding * 2;
static const float  k_MinFrameTime                = 1.0f / 60.0f;
static const int    k_FrameInnerPadding           = 2;
static const QBrush k_FrameInnerPaddingHightlight = QBrush(QColor(200, 200, 200, 40));
static const QBrush k_FrameInnerPaddingShading    = QBrush(QColor(20, 20, 20, 200));
static const QColor k_TickMarkColor               = QColor(20, 20, 20, 200);

// NOTE(SR):
//   Functions because QRect is stupid...
//   Just read the docs for QRect::right and QRect::bottom :(

static int trueRight(const QRect& r)
{
  return r.x() + r.width();
}

static int trueBottom(const QRect& r)
{
  return r.y() + r.height();
}

// Math Helpers

static int roundToLowerMultiple(int n, int grid_size)
{
  return (n / grid_size) * grid_size;
}

int roundToUpperMultiple(int n, int grid_size)
{
  const int remainder = grid_size == 0 ? 0 : n % grid_size;

  return (remainder == 0) ? n : n + grid_size - remainder;
}

QRect aspectRatioDrawRegion(std::uint32_t aspect_w, std::uint32_t aspect_h, std::uint32_t window_w, std::uint32_t window_h)
{
  QRect result = {};

  if (aspect_w > 0 && aspect_h > 0 && window_w > 0 && window_h > 0)
  {
    const float optimal_w = float(window_h) * (float(aspect_w) / float(aspect_h));
    const float optimal_h = float(window_w) * (float(aspect_h) / float(aspect_w));

    if (optimal_w > float(window_w))
    {
      result.setRight(int(window_w) - 1);
      result.setTop(qRound(0.5f * (window_h - optimal_h)));
      result.setBottom(int(result.top() + optimal_h) - 1);
    }
    else
    {
      result.setBottom(int(window_h) - 1);
      result.setLeft(qRound(0.5f * (window_w - optimal_w)));
      result.setRight(int(result.left() + optimal_w) - 1);
    }
  }

  return result;
}

// TimelineSelectionItem Class

bool TimelineSelectionItem::isValid() const
{
  return start_idx >= 0 && end_idx >= 0;
}

bool TimelineSelectionItem::contains(int item) const
{
  return start_idx <= item && item <= end_idx;
}

// TimelineSelection Class

bool TimelineSelection::isEmpty() const
{
  return selection.empty() && !active_selection.isValid();
}

void TimelineSelection::clear()
{
  selection.clear();
  pivot_index      = 0;
  active_selection = {-1, -1};
}

void TimelineSelection::pivotClick(int item, bool keep_old_seletion)
{
  if (keep_old_seletion)
  {
    // Commit the active selection.

    if (active_selection.isValid())
    {
      for (int i = active_selection.start_idx; i <= active_selection.end_idx; ++i)
      {
        select(i);
      }

      active_selection = {-1, -1};
    }
  }
  else
  {
    clear();
  }

  toggle(item);
  pivot_index = item;
}

void TimelineSelection::extendClick(int item)
{
  // assert(pivot_index != -1);

  active_selection.start_idx = std::min(pivot_index, item);
  active_selection.end_idx   = std::max(pivot_index, item);
}

bool TimelineSelection::isSelected(int item) const
{
  return selection.find(item) != selection.end() || active_selection.contains(item);
}

void TimelineSelection::select(int item)
{
  if (item < 0)
  {
    __debugbreak();
  }

  selection.insert(item);
}

void TimelineSelection::toggle(int item)
{
  const auto it = selection.find(item);

  if (it != selection.end())
  {
    selection.erase(it);
  }
  else
  {
    select(item);
  }
}

// Timeline Class

Timeline::Timeline(QWidget* parent) :
  QWidget(parent),
  ui(new Ui::Timeline),
  m_FrameHeight{0},
  m_CurrentAnimation{nullptr},
  m_AtlasExport{nullptr},
  m_FrameInfos{},
  m_DesiredFrameInfos{},
  m_DraggedFrameInfo{},
  m_DraggedOffset{0, 0},
  m_DragMode{FrameDragMode::None},
  m_DroppedFrameInfo{},
  m_HoveredRect{0, 0, 0, 0},
  m_ResizedFrame{false},
  m_Selection{},
  m_ActiveDraggedItem{},
  m_HoveredDraggedItem{},
  m_ReorderDropLocation{},
  m_MouseDownLocation{-1, -1},
  m_UpdateTimer{}
{
  ui->setupUi(this);

  installEventFilter(this);

  onFrameSizeChanged(100);
  setMouseTracking(true);
  setAcceptDrops(true);

  QObject::connect(&m_UpdateTimer, &QTimer::timeout, this, &Timeline::onTimerTick);

  m_UpdateTimer.start(16);
}

Timeline::~Timeline()
{
  delete ui;
}

void Timeline::onFrameSizeChanged(int new_value)
{
  m_FrameHeight = new_value;
  recalculateTimelineSize();
}

void Timeline::onAnimationSelected(Animation* anim)
{
  m_Selection.clear();
  m_CurrentAnimation = anim;
  recalculateTimelineSize();
}

void Timeline::onAnimationChanged(Animation* anim)
{
  if (m_CurrentAnimation == anim)
  {
    recalculateTimelineSize();
  }
}

void Timeline::onAtlasUpdated(AtlasExport& atlas)
{
  m_AtlasExport = &atlas;
  update();
}

static int lerpInt(int lhs, float t, int rhs)
{
  return int((1.0f - t) * lhs + t * rhs);
}

static QRect lerpQRect(const QRect& lhs, float t, const QRect& rhs)
{
  static const int k_CloseEnoughToRhs = 1;

  QRect result;

  result.setX(lerpInt(lhs.x(), t, rhs.x()));
  result.setY(lerpInt(lhs.y(), t, rhs.y()));
  result.setWidth(lerpInt(lhs.width(), t, rhs.width()));
  result.setHeight(lerpInt(lhs.height(), t, rhs.height()));

  if (std::abs(result.x() - rhs.x()) <= k_CloseEnoughToRhs) { result.setX(rhs.x()); }
  if (std::abs(result.y() - rhs.y()) <= k_CloseEnoughToRhs) { result.setY(rhs.y()); }
  if (std::abs(result.width() - rhs.width()) <= k_CloseEnoughToRhs) { result.setWidth(rhs.width()); }
  if (std::abs(result.height() - rhs.height()) <= k_CloseEnoughToRhs) { result.setHeight(rhs.height()); }

  return result;
}

void Timeline::onTimerTick()
{
  const float lerp_factor = 0.55f;
  const int   num_frames  = int(m_FrameInfos.size());

  for (int i = 0; i < num_frames; ++i)
  {
    auto&       src_frame = m_FrameInfos[i];
    const auto& dst_frame = m_DesiredFrameInfos[i];

    src_frame.image        = lerpQRect(src_frame.image, lerp_factor, dst_frame.image);
    src_frame.left_resize  = lerpQRect(src_frame.left_resize, lerp_factor, dst_frame.left_resize);
    src_frame.right_resize = lerpQRect(src_frame.right_resize, lerp_factor, dst_frame.right_resize);
  }

  update();
}

void Timeline::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  QFont font = painter.font();
  font.setPixelSize(k_FontHeight);
  painter.setFont(font);

  const QRect background_rect = rect();

  // Draw background

  painter.fillRect(background_rect, k_BackgroundBrush);

  // Draw Frame Track

  const int   frame_track_height = qMin(m_FrameHeight, background_rect.height() - k_FrameTrackPadding * 2);
  const QRect track_rect         = QRect(background_rect.x(), background_rect.y() + (background_rect.height() - frame_track_height) / 2, background_rect.width(), frame_track_height);
  QRect       track_rect_outline = track_rect;

  track_rect_outline.setTop(track_rect_outline.top() - 1);
  track_rect_outline.setBottom(track_rect_outline.bottom() + 1);

  painter.fillRect(track_rect_outline, k_FrameTrackOutline);
  painter.fillRect(track_rect, k_FrameTrackBrush);

  // Draw Tick Marks

  painter.setPen(k_TickMarkColor);

  for (int x = 0; x < background_rect.width(); x += m_FrameHeight)
  {
    painter.drawLine(x, 0, x, background_rect.height());
  }

  // Draw Text Tracks

  const int text_track_height      = k_FontHeight + 4;
  QRect     text_track_top         = track_rect.translated(0, -(text_track_height + 1));
  QRect     text_track_bot         = track_rect.translated(0, track_rect.height() + 1);
  QRect     text_track_top_outline = text_track_top;
  QRect     text_track_bot_outline = text_track_bot.translated(0, text_track_height);

  text_track_top.setHeight(text_track_height);
  text_track_bot.setHeight(text_track_height);
  text_track_top_outline.setHeight(1);
  text_track_bot_outline.setHeight(1);

  painter.fillRect(text_track_top, k_TextTrackBrush);
  painter.fillRect(text_track_bot, k_TextTrackBrush);
  painter.fillRect(text_track_top_outline, k_FrameTrackOutline);
  painter.fillRect(text_track_bot_outline, k_FrameTrackOutline);

  // Draw Frames

  if (m_AtlasExport && m_CurrentAnimation)
  {
    QPixmap&     atlas_image     = m_AtlasExport->pixmap;
    const int    num_frames      = numFrames();
    const QPoint local_mouse_pos = mapFromGlobal(QCursor::pos());

    for (int i = 0; i < num_frames; ++i)
    {
      if (!m_Selection.isSelected(i))
      {
        drawFrame(atlas_image, painter, i);
      }
    }

    // The selected frames needs to be drawn on top.
    m_Selection.forEachSelectedItem([this, &atlas_image, &painter](int selected_item) {
      drawFrame(atlas_image, painter, selected_item);
    });

    if (m_DragMode != FrameDragMode::None)
    {
      switch (m_DragMode)
      {
        case FrameDragMode::None:
          break;
        case FrameDragMode::Image:
        {
          // painter.fillRect(m_FrameInfos[m_DraggedFrameInfo].image, Qt::lightGray);
          break;
        }
        case FrameDragMode::Left:
          // painter.fillRect(m_FrameInfos[m_DraggedFrameInfo].left_resize, Qt::lightGray);
          break;
        case FrameDragMode::Right:
        {
          const float   base_frame_time = m_CurrentAnimation->frameTime();
          const float   frame_time      = m_DesiredFrameInfos[m_DraggedFrameInfo].frame_time;
          const float   num_frames      = frame_time / base_frame_time;
          const QString fps_str         = tr("%1 ms / ~%2 frame%3").arg(frame_time).arg(num_frames).arg(num_frames > 1.0 ? "s" : "");
          QPainterPath  text_path;

          painter.fillRect(m_FrameInfos[m_DraggedFrameInfo].right_resize, Qt::lightGray);

          text_path.addText(local_mouse_pos, painter.font(), fps_str);

          painter.strokePath(text_path, QPen(Qt::black, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
          painter.fillPath(text_path, Qt::white);
          break;
        }
      }
    }
    else if (!m_HoveredRect.isEmpty())
    {
      painter.fillRect(m_HoveredRect, QColor(200, 200, 200, 80));
    }

    // If Not Dragging
    if (m_ActiveDraggedItem.drag_mode == FrameDragMode::None)
    {
      const FrameInfoAtPoint hovered_item = infoAt(local_mouse_pos, false);

      if (hovered_item.drag_mode == FrameDragMode::Image)
      {
        const auto&   frame_info = m_DesiredFrameInfos[hovered_item.frame_rect_index];
        const QString name_str   = tr("%1").arg(frame_info.frame_src->rel_path);
        QPainterPath  text_path;

        const auto text_location = frame_info.image.bottomLeft() + QPoint(0, painter.font().pixelSize() + k_FramePadding);

        text_path.addText(text_location, painter.font(), name_str);

        const auto text_bounds  = text_path.boundingRect();
        const auto amount_extra = background_rect.width() - (text_location.x() + text_bounds.width());

        if (amount_extra < 0)
        {
          text_path.translate(amount_extra - 5, 0);
        }

        painter.strokePath(text_path, QPen(Qt::black, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.fillPath(text_path, Qt::white);
      }
    }

    if (false && m_HoveredDraggedItem.drag_mode != FrameDragMode::None)
    {
      const auto& frame_info = m_DesiredFrameInfos[m_HoveredDraggedItem.frame_rect_index];

      switch (m_HoveredDraggedItem.drag_mode)
      {
        case FrameDragMode::None:
          break;
        case FrameDragMode::Image:
        {
          painter.fillRect(frame_info.image, Qt::yellow);
          break;
        }
        case FrameDragMode::Left:
          painter.fillRect(frame_info.left_resize, Qt::yellow);
          break;
        case FrameDragMode::Right:
        {
          painter.fillRect(frame_info.right_resize, Qt::yellow);
          break;
        }
      }
    }

    if (m_DroppedFrameInfo.drag_mode != FrameDragMode::None)
    {
      const auto& frame_info = m_FrameInfos[m_DroppedFrameInfo.frame_rect_index];

      switch (m_DroppedFrameInfo.drag_mode)
      {
        case FrameDragMode::None:
          break;
        case FrameDragMode::Image:
        {
          painter.fillRect(frame_info.image, Qt::red);
          break;
        }
        case FrameDragMode::Left:
          painter.fillRect(frame_info.left_resize, Qt::red);
          break;
        case FrameDragMode::Right:
        {
          painter.fillRect(frame_info.right_resize, Qt::red);
          break;
        }
      }
    }
  }
}

void Timeline::wheelEvent(QWheelEvent* event)
{
  event->ignore();
}

bool Timeline::eventFilter(QObject* watched, QEvent* event)
{
  if (event->type() == QEvent::Wheel)
  {
    return false;
  }

  return QWidget::eventFilter(watched, event);
}

void Timeline::mousePressEvent(QMouseEvent* event)
{
  const QPoint local_mouse_pos = event->pos();

  m_ActiveDraggedItem  = infoAt(local_mouse_pos, true);
  m_HoveredDraggedItem = {};

  m_DraggedFrameInfo = m_ActiveDraggedItem.frame_rect_index;
  m_DraggedOffset    = m_ActiveDraggedItem.drag_offset;
  m_DragMode         = m_ActiveDraggedItem.drag_mode;

  // Selection Handling

  if (m_DragMode == FrameDragMode::None)
  {
    m_Selection.clear();
  }
  else
  {
    // if (key_mods & Qt::ControlModifier)
    // {
    //   m_Selection.pivotClick(m_ActiveDraggedItem.frame_rect_index, true);
    // }
    // else if (key_mods & Qt::ShiftModifier)
    // {
    //   m_Selection.extendClick(m_ActiveDraggedItem.frame_rect_index);
    // }
    // else
    // {
    // m_Selection.select(m_ActiveDraggedItem.frame_rect_index);
    // }
  }

  m_MouseDownLocation = local_mouse_pos;
  update();
}

void Timeline::mouseMoveEvent(QMouseEvent* event)
{
  const QPoint local_mouse_pos = event->pos();
  const bool   has_not_dragged = (local_mouse_pos - m_MouseDownLocation).manhattanLength() < QApplication::startDragDistance();
  QPoint       rel_pos         = local_mouse_pos + m_DraggedOffset;

  m_HoveredRect = QRect(0, 0, 0, 0);

  if (has_not_dragged)
  {
    return;
  }
  else if (m_ActiveDraggedItem.drag_mode != FrameDragMode::None)
  {
    if (m_Selection.isEmpty() || !m_Selection.isSelected(m_ActiveDraggedItem.frame_rect_index))
    {
      const auto key_mods = QGuiApplication::queryKeyboardModifiers();

      if (key_mods & Qt::ShiftModifier)
      {
        m_Selection.extendClick(m_ActiveDraggedItem.frame_rect_index);
      }
      else
      {
        m_Selection.pivotClick(m_ActiveDraggedItem.frame_rect_index, key_mods & Qt::ControlModifier);
      }
    }
  }

  switch (m_DragMode)
  {
    case FrameDragMode::None:
    {
      int frame_index = 0;
      for (auto& frame : m_FrameInfos)
      {
#if 0
        if (frame.left_resize.contains(local_mouse_pos, true))
        {
          m_HoveredRect = frame.left_resize;
          break;
        }
#endif
        if (frame.right_resize.contains(local_mouse_pos, true))
        {
          m_HoveredRect = frame.right_resize;
          break;
        }

        if (frame.image.contains(local_mouse_pos, true))
        {
          m_HoveredRect = frame.image;
          break;
        }

        ++frame_index;
      }

      break;
    }
    case FrameDragMode::Image:
    {
      m_HoveredDraggedItem = dropInfoAt(local_mouse_pos);

      bool needs_recalc = false;

      m_Selection.forEachSelectedItem([this, &rel_pos, &local_mouse_pos, &needs_recalc](int selected_item) mutable {
        m_DesiredFrameInfos[selected_item].image.moveTopLeft(rel_pos);

        int frame_index = 0;
        for (auto& frame : m_FrameInfos)
        {
          if (frame_index != selected_item)
          {
            const QRect& base_frame      = frame.image;
            const int    half_base_width = base_frame.width() / 2;
            const QRect& left_frame      = QRect(base_frame.left(), base_frame.top(), half_base_width, INT_MAX / 2);
            const QRect& right_frame     = QRect(base_frame.center().x(), base_frame.top(), half_base_width, INT_MAX / 2);
            const bool   is_to_left      = frame_index < selected_item;

            // If I am to the left of a frame that is left of me (and the same for the right side).
            if ((is_to_left && left_frame.contains(local_mouse_pos)) || (!is_to_left && right_frame.contains(local_mouse_pos)))
            {
              needs_recalc = true;
              break;
            }
          }

          ++frame_index;
        }

        rel_pos.rx() += m_DesiredFrameInfos[selected_item].image.width();

        // m_DesiredFrameInfos[selected_item] = m_FrameInfos[selected_item];
      });

      calculateDesiredLayout(m_HoveredDraggedItem.drag_mode != FrameDragMode::None);

      if (needs_recalc)
      {
        //m_CurrentAnimation->notifyChanged();
      }
      break;
    }
    case FrameDragMode::Left:
      break;
    case FrameDragMode::Right:
    {
      auto&       frame_info      = m_DesiredFrameInfos[m_ActiveDraggedItem.frame_rect_index];
      const float base_frame_time = m_CurrentAnimation->frameTime();
      const int   default_width   = m_FrameHeight;
      const int   snap_width      = default_width / 2;
      QRect&      right_resize    = frame_info.right_resize;
      QRect       frame_rect      = frame_info.image;

      right_resize.moveLeft(rel_pos.x());
      frame_rect.setRight(right_resize.right());

      int       frame_width                = frame_rect.width();
      const int lower_snap                 = roundToLowerMultiple(frame_width, snap_width);
      const int upper_snap                 = roundToUpperMultiple(frame_width, snap_width);
      const int distance_from_lower_stap   = std::abs(lower_snap - frame_width);
      const int distance_from_upper_stap   = std::abs(upper_snap - frame_width);
      const int distance_from_nearest_stap = std::min(distance_from_lower_stap, distance_from_upper_stap);

      if (distance_from_nearest_stap <= 2)
      {
        frame_width = distance_from_nearest_stap == distance_from_lower_stap ? lower_snap : upper_snap;
      }

      const float new_frame_time = qMax(base_frame_time * (float(frame_width) / float(default_width)), k_MinFrameTime);

      if (new_frame_time != frame_info.frame_time)
      {
        m_ResizedFrame = true;
      }

      const int   num_selected_items = m_Selection.numSelected();
      const float delta_time         = (new_frame_time - frame_info.frame_time) / float(num_selected_items);

      m_Selection.forEachSelectedItem([this, delta_time](int selected_item) {
        auto& frame_info = m_DesiredFrameInfos[selected_item];
        frame_info.frame_time += delta_time;
      });

      calculateDesiredLayout(true);
      break;
    }
  }

  update();
}

void Timeline::mouseReleaseEvent(QMouseEvent* event)
{
  const auto key_mods = QGuiApplication::queryKeyboardModifiers();

  switch (m_DragMode)
  {
    case FrameDragMode::None:
    case FrameDragMode::Left:
      break;
    case FrameDragMode::Image:
    {
      if (m_HoveredDraggedItem.drag_mode != FrameDragMode::None)
      {
        std::vector<AnimationFrameInstance> new_frame_data = {};

        const int     num_frames           = numFrames();
        const int     num_selected         = m_Selection.numSelected();
        const int     num_avaiable_slots   = num_frames - num_selected;
        const int     num_before_selection = std::min(m_HoveredDraggedItem.frame_rect_index, num_avaiable_slots);
        std::set<int> drawn_boxes          = {};
        int           count                = 0;

        std::unordered_map<int, int> selection_remap = {};

        selection_remap[-1] = -1;

        const auto addBox = [&](int real_index) {
          qDebug() << new_frame_data.size() << " Set to " << m_DesiredFrameInfos[real_index].frame_src->rel_path;
          new_frame_data.emplace_back(
           m_DesiredFrameInfos[real_index].frame_src->shared_from_this(),
           m_DesiredFrameInfos[real_index].frame_time);

          ++count;
        };

        for (int i = 0; count < num_before_selection; ++i)
        {
          if (!m_Selection.isSelected(i))
          {
            addBox(i);
            drawn_boxes.insert(i);
          }
        }

        m_Selection.forEachSelectedItem([&addBox, &drawn_boxes, &selection_remap, &count](int i) {
          selection_remap[i] = count;
          addBox(i);
          drawn_boxes.insert(i);
        });

        for (int i = 0; count < num_frames; ++i)
        {
          if (!m_Selection.isSelected(i) && drawn_boxes.find(i) == drawn_boxes.end())
          {
            addBox(i);
          }
        }

        // Do a diff

        for (int i = 0; i < int(new_frame_data.size()); ++i)
        {
          if (m_CurrentAnimation->frames[i] != new_frame_data[i])
          {
            // Remap Selection to new indices.

            std::set<int> new_selection = {};

            for (int old : m_Selection.selection)
            {
              new_selection.insert(selection_remap[old]);
            }

            m_Selection.selection                  = std::move(new_selection);
            m_Selection.pivot_index                = selection_remap[m_Selection.pivot_index];
            m_Selection.active_selection.start_idx = selection_remap[m_Selection.active_selection.start_idx];
            m_Selection.active_selection.end_idx   = selection_remap[m_Selection.active_selection.end_idx];

            // Copy over new frame data

            m_CurrentAnimation->parent->recordAction(
             tr("Edit Animation '%1'").arg(m_CurrentAnimation->name()),
             UndoActionFlag_ModifiedAnimation,
             [this, &new_frame_data]() {
               m_CurrentAnimation->frames = std::move(new_frame_data);
             });

            break;
          }
        }
      }

      recalculateTimelineSize();
      break;
    }
    case FrameDragMode::Right:
    {
      qDebug() << "Frame Resized: " << m_ResizedFrame;

      if (m_ResizedFrame)
      {
        int i = 0;
        for (const auto& frame : m_DesiredFrameInfos)
        {
          m_CurrentAnimation->frameAt(i)->setFrameTime(frame.frame_time);
          ++i;
        }
      }

      m_ResizedFrame = false;

      recalculateTimelineSize();
      break;
    }
  }

  const bool has_not_dragged = (event->pos() - m_MouseDownLocation).manhattanLength() < QApplication::startDragDistance();

  if (has_not_dragged)
  {
    if (m_DragMode != FrameDragMode::None)
    {
      if (key_mods & Qt::ControlModifier)
      {
        m_Selection.pivotClick(m_ActiveDraggedItem.frame_rect_index, true);
      }
      else if (key_mods & Qt::ShiftModifier)
      {
        m_Selection.extendClick(m_ActiveDraggedItem.frame_rect_index);
      }
      else
      {
        m_Selection.pivotClick(m_DraggedFrameInfo, false);
      }
    }
    else
    {
      m_Selection.clear();
    }
  }

  // Reset State

  m_ActiveDraggedItem  = {};
  m_HoveredRect        = QRect(0, 0, 0, 0);
  m_HoveredDraggedItem = {};
  m_DragMode           = FrameDragMode::None;

  calculateDesiredLayout(true);
}

void Timeline::resizeEvent(QResizeEvent* event)
{
  (void)event;

  // calculateDesiredLayout(true);
  recalculateTimelineSize();
}

void Timeline::dragEnterEvent(QDragEnterEvent* event)
{
  event->acceptProposedAction();
}

void Timeline::dragMoveEvent(QDragMoveEvent* event)
{
  const QPoint local_mouse_pos = event->pos();

  m_DroppedFrameInfo = infoAt(local_mouse_pos, false);

  if (m_DroppedFrameInfo.drag_mode != FrameDragMode::None)
  {
  }

  event->acceptProposedAction();

  update();
}

void Timeline::dragLeaveEvent(QDragLeaveEvent* event)
{
  m_DroppedFrameInfo = {};

  event->accept();
}

void Timeline::dropEvent(QDropEvent* event)
{
  if (m_DroppedFrameInfo.drag_mode != FrameDragMode::None)
  {
#if 0
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
#endif

    switch (m_DroppedFrameInfo.drag_mode)
    {
      case FrameDragMode::Left:
      {
        qDebug() << "Insert Frame before: " << m_DroppedFrameInfo.frame_rect_index;
        break;
      }
      case FrameDragMode::Image:
      {
        qDebug() << "Insert Frame On Top Of: " << m_DroppedFrameInfo.frame_rect_index;
        break;
      }
      case FrameDragMode::Right:
      {
        qDebug() << "Insert Frame After: " << m_DroppedFrameInfo.frame_rect_index;
        break;
      }
      default:
        break;
    }
  }

  m_DroppedFrameInfo = {};

  update();

  event->accept();
}

void Timeline::recalculateTimelineSize()
{
  m_FrameInfos.clear();
  m_DesiredFrameInfos.clear();

  if (m_CurrentAnimation)
  {
    const QRect background_rect    = rect();
    const int   frame_track_height = qMin(m_FrameHeight, background_rect.height() - k_FrameTrackPadding * 2);
    const QRect track_rect         = QRect(background_rect.x(), background_rect.y() + (background_rect.height() - frame_track_height) / 2, background_rect.width(), frame_track_height);
    const int   num_frames         = numFrames();
    const int   frame_top          = track_rect.top() + k_FramePadding;
    const int   frame_height       = track_rect.height() - k_DblFramePadding;
    const float base_frame_time    = m_CurrentAnimation->frameTime();
    int         current_x          = 0;

    for (int i = 0; i < num_frames; ++i)
    {
      AnimationFrameInstance* const frame = m_CurrentAnimation->frameAt(i);

      assert(frame);
      assert(frame->source);
      assert(frame->source->index < int(m_AtlasExport->image_rectangles.size()));
      assert(frame->source->index >= 0);

      const float  frame_time        = frame->frame_time;
      const QRect& frame_uv_rect     = m_AtlasExport->image_rectangles[frame->source->index];
      const float  frame_width_scale = frame_time / base_frame_time;
      const int    frame_width       = int(m_FrameHeight * frame_width_scale);
      const QRect  resize_left       = QRect(current_x, frame_top, k_FramePadding, frame_height);
      const QRect  frame_rect        = QRect(current_x + k_FramePadding, frame_top, frame_width - k_DblFramePadding, frame_height);
      const QRect  resize_right      = QRect(trueRight(frame_rect), frame_top, k_FramePadding, frame_height);

      m_FrameInfos.emplace_back(
       frame_rect,
       resize_left,
       resize_right,
       frame_time,
       frame_uv_rect,
       frame->source.get());

      current_x = trueRight(resize_right);
    }

    m_DesiredFrameInfos = m_FrameInfos;

    setMinimumSize(current_x, 0);
  }
}

void Timeline::calculateDesiredLayout(bool use_selected_items)
{
  if (!m_CurrentAnimation) { return; }

  const QRect background_rect    = rect();
  const int   frame_track_height = qMin(m_FrameHeight, background_rect.height() - k_FrameTrackPadding * 2);
  const QRect track_rect         = QRect(background_rect.x(), background_rect.y() + (background_rect.height() - frame_track_height) / 2, background_rect.width(), frame_track_height);
  const int   num_frames         = numFrames();
  const int   frame_top          = track_rect.top() + k_FramePadding;
  const int   frame_height       = track_rect.height() - k_DblFramePadding;
  const float base_frame_time    = m_CurrentAnimation->frameTime();
  int         current_x          = 0;

  const auto addBox = [&](int item) {
    auto&       frame_info        = m_DesiredFrameInfos[item];
    const float frame_time        = frame_info.frame_time;
    const float frame_width_scale = frame_time / base_frame_time;
    const int   frame_width       = int(m_FrameHeight * frame_width_scale);
    const QRect resize_left       = QRect(current_x, frame_top, k_FramePadding, frame_height);
    const QRect frame_rect        = QRect(current_x + k_FramePadding, frame_top, frame_width - k_DblFramePadding, frame_height);
    const QRect resize_right      = QRect(trueRight(frame_rect), frame_top, k_FramePadding, frame_height);

    frame_info.image        = frame_rect;
    frame_info.left_resize  = resize_left;
    frame_info.right_resize = resize_right;

    current_x = trueRight(resize_right);
  };

  if (use_selected_items && m_HoveredDraggedItem.drag_mode != FrameDragMode::None)
  {
    const int     num_selected         = m_Selection.numSelected();
    const int     num_avaiable_slots   = num_frames - num_selected;
    const int     num_before_selection = std::min(m_HoveredDraggedItem.frame_rect_index, num_avaiable_slots);
    std::set<int> drawn_boxes          = {};
    int           count                = 0;

    for (int i = 0; count < num_before_selection; ++i)
    {
      if (!m_Selection.isSelected(i))
      {
        addBox(i);
        drawn_boxes.insert(i);
        ++count;
      }
    }

    m_Selection.forEachSelectedItem([&addBox, &count, &drawn_boxes](int i) {
      addBox(i);
      drawn_boxes.insert(i);
      ++count;
    });

    for (int i = 0; count < num_frames; ++i)
    {
      if (!m_Selection.isSelected(i) && drawn_boxes.find(i) == drawn_boxes.end())
      {
        addBox(i);
        ++count;
      }
    }
  }
  else
  {
    for (int i = 0; i < num_frames; ++i)
    {
      if (use_selected_items || !m_Selection.isSelected(i))
      {
        addBox(i);
      }
    }
  }
}

void Timeline::copyOverAnimtionData()
{
}

int Timeline::numFrames() const
{
  return m_CurrentAnimation ? m_CurrentAnimation->numFrames() : 0;
}

void Timeline::drawFrame(const QPixmap& atlas_image, QPainter& painter, int index)
{
  const auto&  frame_rect_info = m_FrameInfos[index];
  const QRect& frame_rect      = frame_rect_info.image;
  const QRect& pixmap_src      = frame_rect_info.frame_uv_rect;
  const QRect  pixmap_dst      = aspectRatioDrawRegion(pixmap_src.width(), pixmap_src.height(), frame_rect.width() - 1, frame_rect.height() - 1).translated(frame_rect.topLeft());

  painter.fillRect(frame_rect, k_BackgroundBrush);

  painter.drawPixmap(
   pixmap_dst,
   atlas_image,
   pixmap_src);

  // Draw Inner Framing

  // Bottom Shade
  painter.fillRect(frame_rect.left(), trueBottom(frame_rect) - k_FrameInnerPadding, frame_rect.width(), k_FrameInnerPadding, k_FrameInnerPaddingShading);

  // Right Shade
  painter.fillRect(trueRight(frame_rect) - k_FrameInnerPadding, frame_rect.top(), k_FrameInnerPadding, frame_rect.height(), k_FrameInnerPaddingShading);

  // Top Highlight
  painter.fillRect(frame_rect.left(), frame_rect.top(), frame_rect.width(), k_FrameInnerPadding, k_FrameInnerPaddingHightlight);

  // Left Highlight
  painter.fillRect(frame_rect.left(), frame_rect.top(), k_FrameInnerPadding, frame_rect.height(), k_FrameInnerPaddingHightlight);

  if (m_Selection.isSelected(index))
  {
    painter.setPen(Qt::yellow);
    painter.drawRect(frame_rect);
  }

  // Frame Number

  const QString name_str = tr("#%1").arg(index);
  QPainterPath  text_path;

  text_path.addText(frame_rect.topLeft() - QPoint(0, k_FontHeight / 2 + 2), painter.font(), name_str);

  const auto text_bounds = text_path.boundingRect();

  text_path.translate((frame_rect.width() - text_bounds.width()) / 2, 0);

  painter.strokePath(text_path, QPen(Qt::black, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.fillPath(text_path, Qt::white);
}

FrameInfoAtPoint Timeline::infoAt(const QPoint& local_mouse_pos, bool allow_active_item) const
{
  FrameInfoAtPoint ret         = {};
  int              frame_index = 0;

  for (const auto& frame : m_FrameInfos)
  {
    if (allow_active_item || frame_index != m_ActiveDraggedItem.frame_rect_index)
    {
      if (frame.left_resize.contains(local_mouse_pos))
      {
        ret.frame_rect_index = frame_index;
        ret.drag_offset      = frame.left_resize.topLeft() - local_mouse_pos;
        ret.drag_mode        = FrameDragMode::Left;
        break;
      }

      if (frame.right_resize.contains(local_mouse_pos))
      {
        ret.frame_rect_index = frame_index;
        ret.drag_offset      = frame.right_resize.topLeft() - local_mouse_pos;
        ret.drag_mode        = FrameDragMode::Right;
        break;
      }

      if (frame.image.contains(local_mouse_pos))
      {
        ret.frame_rect_index = frame_index;
        ret.drag_offset      = frame.image.topLeft() - local_mouse_pos;
        ret.drag_mode        = FrameDragMode::Image;
        break;
      }
    }

    ++frame_index;
  }

  return ret;
}

FrameDropInfoAtPoint Timeline::dropInfoAt(const QPoint& local_mouse_pos)
{
  const QRect          background_rect    = rect();
  const int            frame_track_height = qMin(m_FrameHeight, background_rect.height() - k_FrameTrackPadding * 2);
  const QRect          track_rect         = QRect(background_rect.x(), background_rect.y() + (background_rect.height() - frame_track_height) / 2, background_rect.width(), frame_track_height);
  const int            num_frames         = numFrames();
  const int            frame_top          = track_rect.top() + k_FramePadding;
  const int            frame_height       = track_rect.height() - k_DblFramePadding;
  const float          base_frame_time    = m_CurrentAnimation->frameTime();
  int                  current_x          = 0;
  FrameDropInfoAtPoint ret                = {};

  for (int i = 0; i < num_frames; ++i)
  {
    const auto& frame_info        = m_FrameInfos[i];
    const float frame_time        = frame_info.frame_time;
    const float frame_width_scale = frame_time / base_frame_time;
    const int   frame_width       = int(m_FrameHeight * frame_width_scale);
    const QRect resize_left       = QRect(current_x, frame_top, k_FramePadding, frame_height);
    const QRect frame_rect        = QRect(current_x + k_FramePadding, frame_top, frame_width - k_DblFramePadding, frame_height);
    const QRect resize_right      = QRect(trueRight(frame_rect), frame_top, k_FramePadding, frame_height);

    ret.frame_rect_index = i;

    if (resize_left.contains(local_mouse_pos))
    {
      ret.drag_offset = resize_left.topLeft() - local_mouse_pos;
      ret.drag_mode   = FrameDragMode::Left;
      break;
    }

    if (resize_right.contains(local_mouse_pos))
    {
      ret.drag_offset = resize_right.topLeft() - local_mouse_pos;
      ret.drag_mode   = FrameDragMode::Right;
      break;
    }

    if (frame_rect.contains(local_mouse_pos))
    {
      ret.drag_offset = frame_rect.topLeft() - local_mouse_pos;
      ret.drag_mode   = FrameDragMode::Image;
      break;
    }

    current_x = trueRight(resize_right);
  }

  return ret;
}
