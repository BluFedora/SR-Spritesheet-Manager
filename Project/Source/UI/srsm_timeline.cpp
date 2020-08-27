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
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
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
  active_selection.start_idx = std::min(pivot_index, item);
  active_selection.end_idx   = std::max(pivot_index, item);
}

bool TimelineSelection::isSelected(int item) const
{
  return selection.find(item) != selection.end() || active_selection.contains(item);
}

void TimelineSelection::select(int item)
{
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
  m_ScrubberTrackRect{0, 0, 0, 0},
  m_IsDraggingScrubber{false},
  m_MouseIsDown{false},
  m_UpdateTimer{}
{
  ui->setupUi(this);

  onFrameSizeChanged(100);
  setMouseTracking(true);
  setAcceptDrops(true);
  setContextMenuPolicy(Qt::CustomContextMenu);

  QObject::connect(this, &Timeline::customContextMenuRequested, this, &Timeline::onCustomCtxMenu);
  QObject::connect(&m_UpdateTimer, &QTimer::timeout, this, &Timeline::onTimerTick);

  m_UpdateTimer.start(16);
}

void Timeline::setup(QScrollArea* scroll_area)
{
  m_ParentScrollArea = scroll_area;
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
    m_Selection.clear();
    recalculateTimelineSize();
  }
}

void Timeline::onAtlasUpdated(AtlasExport& atlas)
{
  m_AtlasExport = &atlas;
  m_Selection.clear();
  recalculateTimelineSize();
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
  const float lerp_factor     = 0.55f;
  const int   num_frames      = int(m_FrameInfos.size());
  const auto  local_mouse_pos = mapFromGlobal(QCursor::pos());

  for (int i = 0; i < num_frames; ++i)
  {
    auto&       src_frame = m_FrameInfos[i];
    const auto& dst_frame = m_DesiredFrameInfos[i];

    src_frame.image        = lerpQRect(src_frame.image, lerp_factor, dst_frame.image);
    src_frame.left_resize  = lerpQRect(src_frame.left_resize, lerp_factor, dst_frame.left_resize);
    src_frame.right_resize = lerpQRect(src_frame.right_resize, lerp_factor, dst_frame.right_resize);
  }

  {
    if (m_IsDraggingScrubber && !m_FrameInfos.empty())
    {
      bool found_frame = false;

      for (int i = 0; i < int(m_FrameInfos.size()); ++i)
      {
        const auto& frame_info     = m_FrameInfos[i];
        auto        inf_image_rect = frame_info.image;

        inf_image_rect.setLeft(frame_info.left_resize.left());
        inf_image_rect.setRight(frame_info.right_resize.right());
        inf_image_rect.setY(-INT_MAX / 4);
        inf_image_rect.setHeight(INT_MAX / 2);

        if (inf_image_rect.contains(local_mouse_pos))
        {
          const float frame_width     = frame_info.image.width();
          const float amt_across_rect = float(local_mouse_pos.x() - frame_info.image.left());

          if (m_CurrentAnimation->previewed_frame != i)
          {
            m_CurrentAnimation->previewed_frame = i;

            m_CurrentAnimation->notifyPreviewFrameChanged();
          }

          m_CurrentAnimation->previewed_frame_time = std::clamp(amt_across_rect / frame_width, 0.0f, 1.0f) * frame_info.frame_time;

          found_frame = true;
          break;
        }
      }

      if (!found_frame)
      {
        const int new_frame = local_mouse_pos.x() <= k_FramePadding ? 0 : m_CurrentAnimation->numFrames() - 1;

        if (m_CurrentAnimation->previewed_frame != new_frame)
        {
          m_CurrentAnimation->previewed_frame = new_frame;

          m_CurrentAnimation->notifyPreviewFrameChanged();
        }

        if (local_mouse_pos.x() <= k_FramePadding)
        {
          m_CurrentAnimation->previewed_frame_time = 0.0f;
        }
        else
        {
          m_CurrentAnimation->previewed_frame_time = m_FrameInfos[new_frame].frame_time;
        }
      }
    }
  }

  // Hover Rect Calcs
  {
    m_HoveredRect = QRect(0, 0, 0, 0);

    int frame_index = 0;
    for (const auto& frame : m_FrameInfos)
    {
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
  }

  // This should be the only call to update in this class.
  update();
}

void Timeline::onCustomCtxMenu(const QPoint& pos)
{
  QMenu right_click;

  auto rm_selected_frames_action = right_click.addAction(tr("Removed Selected Frames"));

  rm_selected_frames_action->setEnabled(!m_Selection.isEmpty());

  QObject::connect(rm_selected_frames_action, &QAction::triggered, this, &Timeline::removeSelectedFrames);

  right_click.exec(mapToGlobal(pos));
}

void Timeline::paintEvent(QPaintEvent* event)
{
  const QPoint local_mouse_pos = mapFromGlobal(QCursor::pos());

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

  m_ScrubberTrackRect = text_track_top;

  // Hover Fx
  if (m_IsDraggingScrubber)
  {
    auto alpha_yellow = QColor(Qt::yellow);

    alpha_yellow.setAlpha(64);

    painter.fillRect(m_ScrubberTrackRect, alpha_yellow);
  }
  else if (m_ScrubberTrackRect.contains(local_mouse_pos))
  {
    painter.fillRect(m_ScrubberTrackRect, k_FrameInnerPaddingHightlight);
  }

  // Draw Frames

  if (m_AtlasExport && m_CurrentAnimation)
  {
    QPixmap&  atlas_image = m_AtlasExport->pixmap;
    const int num_frames  = numFrames();

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
        case FrameDragMode::Image:
        case FrameDragMode::Left:
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
    else
    {
      if (!m_HoveredRect.isEmpty())
      {
        painter.fillRect(m_HoveredRect, QColor(200, 200, 200, 80));
      }

      if (!m_IsDraggingScrubber && m_MouseIsDown)
      {
        const auto selection = selectionRect();

        painter.fillRect(selection, QColor(255, 255, 255, 50));
        painter.setPen(QColor(128, 128, 128, 100));
        painter.drawRect(selection);
      }
    }

    // If Not Dragging
    if (m_ActiveDraggedItem.drag_mode == FrameDragMode::None)
    {
      const FrameInfoAtPoint hovered_item = infoAt(local_mouse_pos, false);

      if (hovered_item.drag_mode == FrameDragMode::Image)
      {
        const int     gutter        = 5;
        const auto    viewport_rect = event->rect();
        const auto&   frame_info    = m_DesiredFrameInfos[hovered_item.frame_rect_index];
        const QString name_str      = tr("%1").arg(frame_info.frame_src->rel_path);
        QPainterPath  text_path;

        const auto base_text_location = frame_info.image.bottomLeft() + QPoint(0, painter.font().pixelSize() + k_FramePadding);

        text_path.addText(base_text_location, painter.font(), name_str);

        const auto text_bounds   = text_path.boundingRect();
        auto       text_location = base_text_location - QPoint((text_bounds.width() - frame_info.image.width()) / 2, 0);
        const auto left_bounds   = -x() + gutter;
        const auto right_bounds  = -x() + viewport_rect.width() - gutter - int(text_bounds.width());

        text_location.rx() = std::min(std::max(text_location.x(), left_bounds), right_bounds);

        const auto delta_pos = (text_location - base_text_location);

        text_path.translate(delta_pos.x(), delta_pos.y());

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

    // Draw Previewed Frame Scrubber

    if (m_CurrentAnimation->numFrames())
    {
      const int   previewed_frame = m_CurrentAnimation->previewed_frame;
      const auto& frame_info      = m_DesiredFrameInfos[previewed_frame];

      const int    base_x                     = frame_info.image.left();
      const float  amt_across_frame           = m_CurrentAnimation->previewed_frame_time / m_CurrentAnimation->frameAt(previewed_frame)->frame_time;
      const int    x                          = base_x + int(amt_across_frame * frame_info.image.width());
      const int    scrubber_grabber_size      = 10;
      const int    scrubber_grabber_half_size = scrubber_grabber_size / 2;
      const int    scrubber_y                 = text_track_top.top() + (text_track_top.height() - scrubber_grabber_size) / 2;
      const QRect  scrubber_box               = QRect(x - scrubber_grabber_half_size, scrubber_y, scrubber_grabber_size, scrubber_grabber_size);
      const QColor scrubber_color             = scrubber_box.contains(local_mouse_pos) || m_IsDraggingScrubber ? Qt::yellow : Qt::white;

      painter.setPen(QPen(QBrush(scrubber_color), 2.0, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));
      painter.drawLine(x, 0, x, background_rect.height());
      painter.fillRect(scrubber_box, scrubber_color);
    }
  }
}

void Timeline::wheelEvent(QWheelEvent* event)
{
  const auto   key_mods  = QGuiApplication::queryKeyboardModifiers();
  const QPoint num_steps = event->angleDelta() / 15;

  if (key_mods & Qt::ControlModifier)
  {
    m_FrameHeight = std::clamp(m_FrameHeight + num_steps.y(), k_DblFramePadding * 2, 400);

    recalculateTimelineSize();
  }
  else
  {
    QScrollBar* const h_scroll = m_ParentScrollArea->horizontalScrollBar();

    h_scroll->setValue(h_scroll->value() + -num_steps.y() * 2);
  }

  event->accept();
}

void Timeline::mousePressEvent(QMouseEvent* event)
{
  if (event->button() != Qt::LeftButton)
  {
    return;
  }

  const QPoint local_mouse_pos = event->pos();

  m_ActiveDraggedItem  = infoAt(local_mouse_pos, true);
  m_MouseDownLocation  = local_mouse_pos;
  m_HoveredDraggedItem = {};

  m_DraggedFrameInfo = m_ActiveDraggedItem.frame_rect_index;
  m_DraggedOffset    = m_ActiveDraggedItem.drag_offset;
  m_DragMode         = m_ActiveDraggedItem.drag_mode;

  // Selection Handling

  if (m_DragMode == FrameDragMode::None)
  {
    //m_Selection.clear();

    if ((m_IsDraggingScrubber = m_ScrubberTrackRect.contains(m_MouseDownLocation)))
    {
      grabMouse();
    }
  }

  m_MouseIsDown = true;
  event->accept();
}

void Timeline::mouseMoveEvent(QMouseEvent* event)
{
  if (event->buttons() != Qt::LeftButton)
  {
    return;
  }

  const QPoint local_mouse_pos = event->pos();
  const bool   has_not_dragged = (local_mouse_pos - m_MouseDownLocation).manhattanLength() < QApplication::startDragDistance();
  QPoint       rel_pos         = local_mouse_pos + m_DraggedOffset;

  if (m_ActiveDraggedItem.drag_mode != FrameDragMode::None)
  {
    if (has_not_dragged)
    {
      return;
    }

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
      break;
    }
    case FrameDragMode::Left:
    {
      break;
    }
    case FrameDragMode::Image:
    {
      m_HoveredDraggedItem = dropInfoAt(local_mouse_pos);

      m_Selection.forEachSelectedItem([this, &rel_pos](int selected_item) {
        m_DesiredFrameInfos[selected_item].image.moveTopLeft(rel_pos);
        rel_pos.rx() += m_DesiredFrameInfos[selected_item].image.width() + 2;
      });

      calculateDesiredLayout(m_HoveredDraggedItem.drag_mode != FrameDragMode::None);
      break;
    }
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
        m_DesiredFrameInfos[selected_item].frame_time += delta_time;
      });

      calculateDesiredLayout(true);
      break;
    }
  }
}

void Timeline::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() != Qt::LeftButton)
  {
    return;
  }

  const auto key_mods = QGuiApplication::queryKeyboardModifiers();

  switch (m_DragMode)
  {
    case FrameDragMode::None:
    {
      if (!m_IsDraggingScrubber)
      {
        const auto selection = selectionRect();

        if (!key_mods.testFlag(Qt::ShiftModifier))
        {
          m_Selection.clear();
        }

        int frame_index = 0;
        for (const auto& frame : m_FrameInfos)
        {
          if (frame.left_resize.intersects(selection) || frame.right_resize.intersects(selection) || frame.image.intersects(selection))
          {
            m_Selection.select(frame_index);
          }

          ++frame_index;
        }
      }

      break;
    }
    case FrameDragMode::Left:
      break;
    case FrameDragMode::Image:
    {
      if (m_HoveredDraggedItem.drag_mode != FrameDragMode::None)
      {
        const int                           num_frames           = numFrames();
        const int                           num_selected         = m_Selection.numSelected();
        const int                           num_avaiable_slots   = num_frames - num_selected;
        const int                           num_before_selection = std::min(m_HoveredDraggedItem.frame_rect_index, num_avaiable_slots);
        std::set<int>                       drawn_boxes          = {};
        int                                 count                = 0;
        std::vector<AnimationFrameInstance> new_frames           = {};
        std::unordered_map<int, int>        selection_remap      = {};
        auto&                               old_frames           = m_CurrentAnimation->frames;

        new_frames.reserve(old_frames.size());

        selection_remap[-1] = -1;

        const auto addBox = [&](int real_index) {
          const auto& frame_info = m_DesiredFrameInfos[real_index];

          new_frames.emplace_back(frame_info.frame_src->shared_from_this(), frame_info.frame_time);

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

        const auto first_diff = std::mismatch(new_frames.begin(), new_frames.end(), old_frames.begin());

        if (first_diff.first != new_frames.end())
        {
          // Remap Selection to new indices.

          auto&         old_selection = m_Selection.selection;
          std::set<int> new_selection = {};

          std::transform(
           old_selection.begin(),
           old_selection.end(),
           std::inserter(new_selection, new_selection.begin()),
           [&selection_remap](int old_item) -> int {
             return selection_remap[old_item];
           });

          m_Selection.selection                  = std::move(new_selection);
          m_Selection.pivot_index                = selection_remap[m_Selection.pivot_index];
          m_Selection.active_selection.start_idx = selection_remap[m_Selection.active_selection.start_idx];
          m_Selection.active_selection.end_idx   = selection_remap[m_Selection.active_selection.end_idx];

          // Copy over new frame data

          m_CurrentAnimation->parent->recordAction(
           tr("Edit Frame Order '%1'").arg(m_CurrentAnimation->name()),
           UndoActionFlag_ModifiedAnimation,
           [&first_diff, &new_frames]() {
             std::copy(first_diff.first, new_frames.end(), first_diff.second);
           });

          recalculateTimelineSize();
        }
      }

      break;
    }
    case FrameDragMode::Right:
    {
      if (m_ResizedFrame)
      {
        m_CurrentAnimation->parent->recordAction(
         tr("Edit Frame Time '%1'").arg(m_CurrentAnimation->name()),
         UndoActionFlag_ModifiedAnimation,
         [this]() {
           m_Selection.forEachSelectedItem([this](int item) {
             m_CurrentAnimation->frameAt(item)->setFrameTime(m_DesiredFrameInfos[item].frame_time);
           });
         });
      }

      m_ResizedFrame = false;
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

  m_MouseIsDown        = false;
  m_IsDraggingScrubber = false;
  m_ActiveDraggedItem  = {};
  m_HoveredRect        = QRect(0, 0, 0, 0);
  m_HoveredDraggedItem = {};
  m_DragMode           = FrameDragMode::None;
  releaseMouse();

  calculateDesiredLayout(true);
}

void Timeline::resizeEvent(QResizeEvent* event)
{
  (void)event;

  calculateDesiredLayout(true);
  m_FrameInfos = m_DesiredFrameInfos;  // Instantly snapping movement is the desired behavior in the case of resizing.
}

void Timeline::dragEnterEvent(QDragEnterEvent* event)
{
  if (m_CurrentAnimation && event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
  {
    event->acceptProposedAction();
  }
}

void Timeline::dragMoveEvent(QDragMoveEvent* event)
{
  if (m_CurrentAnimation && event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
  {
    const QPoint local_mouse_pos = event->pos();

    m_DroppedFrameInfo = infoAt(local_mouse_pos, false);

    if (m_DroppedFrameInfo.drag_mode != FrameDragMode::None)
    {
      event->acceptProposedAction();
    }
  }
}

void Timeline::dragLeaveEvent(QDragLeaveEvent* event)
{
  m_DroppedFrameInfo = {};

  event->accept();
}

static typename std::vector<AnimationFrameInstance>::iterator iteratorFromFrameInfo(const FrameInfoAtPoint& info, std::vector<AnimationFrameInstance>& frames)
{
  switch (info.drag_mode)
  {
    case FrameDragMode::Left:
    {
      return frames.begin() + info.frame_rect_index;  // Insert frame before 'frame_rect_index'
    }
    case FrameDragMode::Image:
    {
      return frames.erase(frames.begin() + info.frame_rect_index);  // Insert frame on top of 'frame_rect_index'
    }
    case FrameDragMode::Right:
    {
      return frames.begin() + info.frame_rect_index + 1;  // Insert frame after 'frame_rect_index'
    }
    default:
    case FrameDragMode::None:
    {
      return frames.begin();  // The frames are empty
    }
  }
};

void Timeline::dropEvent(QDropEvent* event)
{
  if ((m_DroppedFrameInfo.drag_mode != FrameDragMode::None || m_CurrentAnimation->frames.empty()) && event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
  {
    QByteArray                          encoded = event->mimeData()->data("application/x-qabstractitemmodeldatalist");
    QDataStream                         stream(&encoded, QIODevice::ReadOnly);
    const float                         frame_time       = m_CurrentAnimation->frameTime();
    std::vector<AnimationFrameInstance> frames_to_insert = {};

    while (!stream.atEnd())
    {
      int                 row, col;
      QMap<int, QVariant> role_data_map;

      stream >> row >> col >> role_data_map;

      if (role_data_map.contains(ImageLibraryRole::FrameSource))
      {
        frames_to_insert.emplace_back(role_data_map[ImageLibraryRole::FrameSource].value<AnimationFrameSourcePtr>(), frame_time);
      }
    }

    if (frames_to_insert.empty())
    {
      return;
    }

    m_CurrentAnimation->parent->recordAction(
     tr("Add Frames To %1").arg(m_CurrentAnimation->name()),
     UndoActionFlag_ModifiedAnimation,
     [this, &frames_to_insert]() {
       std::vector<AnimationFrameInstance>& frames = m_CurrentAnimation->frames;
       const auto                           it     = iteratorFromFrameInfo(m_DroppedFrameInfo, frames);

       frames.insert(it, frames_to_insert.cbegin(), frames_to_insert.cend());
     });

    m_DroppedFrameInfo = {};
    recalculateTimelineSize();

    event->accept();
  }
}

void Timeline::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Delete)
  {
    if (removeSelectedFrames())
    {
      event->accept();
    }
  }
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
      assert(frame->source->index >= 0 && frame->source->index < m_AtlasExport->image_rectangles.size());

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

FrameInfoAtPoint Timeline::dropInfoAt(const QPoint& local_mouse_pos)
{
  const QRect      background_rect    = rect();
  const int        frame_track_height = qMin(m_FrameHeight, background_rect.height() - k_FrameTrackPadding * 2);
  const QRect      track_rect         = QRect(background_rect.x(), background_rect.y() + (background_rect.height() - frame_track_height) / 2, background_rect.width(), frame_track_height);
  const int        num_frames         = numFrames();
  const int        frame_top          = track_rect.top() + k_FramePadding;
  const int        frame_height       = track_rect.height() - k_DblFramePadding;
  const float      base_frame_time    = m_CurrentAnimation->frameTime();
  int              current_x          = 0;
  FrameInfoAtPoint ret                = {};

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

bool Timeline::removeSelectedFrames()
{
  if (!m_Selection.isEmpty())
  {
    m_CurrentAnimation->parent->recordAction("Delete Frames", UndoActionFlag_ModifiedAnimation, [this]() {
      m_Selection.forEachSelectedItem<true>([this](int item) {
        m_CurrentAnimation->frames.erase(m_CurrentAnimation->frames.begin() + item);
      });
    });

    m_Selection.clear();
    recalculateTimelineSize();

    return true;
  }

  return false;
}

QRect Timeline::selectionRect() const
{
  const QPoint local_mouse_pos = mapFromGlobal(QCursor::pos());

  return QRect(
   std::min(local_mouse_pos.x(), m_MouseDownLocation.x()),
   std::min(local_mouse_pos.y(), m_MouseDownLocation.y()),
   std::abs(local_mouse_pos.x() - m_MouseDownLocation.x()),
   std::abs(local_mouse_pos.y() - m_MouseDownLocation.y()));
}
