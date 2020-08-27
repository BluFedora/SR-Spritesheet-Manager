//
// SR Spritesheet Manager
//
// file:   srsm_animation.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SRSM_ANIMATION_HPP
#define SRSM_ANIMATION_HPP

#include <QObject>             // QObject
#include <QStandardItemModel>  // QStandardItemModel
#include <QString>             // QString
#include <QStringListModel>    // QStringListModel

#include <memory>  // unique_ptr<T>, shared_ptr<T>
#include <vector>  // vector<T>

class Project;
struct AnimationFrameSource;

using AnimationFrameSourcePtr = std::shared_ptr<AnimationFrameSource>;

struct AnimationFrameInstance final
{
  AnimationFrameSourcePtr source;
  float                   frame_time;

  explicit AnimationFrameInstance(AnimationFrameSourcePtr anim_source, float frame_time);

  QString full_path() const;

  void setFrameTime(float value)
  {
    frame_time = value;
  }

  friend bool operator==(const AnimationFrameInstance& lhs, const AnimationFrameInstance& rhs)
  {
    return lhs.source == rhs.source && lhs.frame_time == rhs.frame_time;
  }

  friend bool operator!=(const AnimationFrameInstance& lhs, const AnimationFrameInstance& rhs)
  {
    return lhs.source != rhs.source || lhs.frame_time != rhs.frame_time;
  }
};

struct Animation final : public QStandardItem
{
 public:
  Project*                            parent;                //!< The parent project object.
  std::vector<AnimationFrameInstance> frames;                //!< Contains 'AnimationFrameInstance' items.
  int                                 frame_rate;            //!< Units are in frames per second.
  int                                 previewed_frame;       //!< The current frame to show
  float                               previewed_frame_time;  //!< The amt of time that has passed for the curent frame.

 public:
  Animation(Project* parent, const QString& name, int fps);

  QString       name() const { return data(Qt::DisplayRole).toString(); }
  void          setName(const QString& name);
  std::uint32_t numFrames() const { return std::uint32_t(frames.size()); }
  float         frameTime() const { return 1.0f / float(frame_rate); }

  void                    addFrame(AnimationFrameSourcePtr anim_source);
  void                    addFrame(AnimationFrameInstance frame);
  AnimationFrameInstance* frameAt(int index);

  void notifyPreviewFrameChanged();
  void notifyChanged();
};

using AnimationPtr = std::unique_ptr<Animation>;

#endif  // SRSM_ANIMATION_HPP
