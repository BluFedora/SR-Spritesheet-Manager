//
// SR Spritesheet Manager
//
// file:   srsm_animation.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "srsm_animation.hpp"

#include "Data/srsm_project.hpp"      // Project
#include "UI/srsm_image_library.hpp"  // AnimationFrameSourcePtr

#include <QDebug>

AnimationFrameInstance::AnimationFrameInstance(AnimationFrameSourcePtr anim_source, float frame_time) :
  source{anim_source},
  frame_time{frame_time}
{
}

QString AnimationFrameInstance::full_path() const
{
  return source->full_path;
}

Animation::Animation(Project* parent, const QString& name, int fps) :
  QStandardItem(name),
  parent{parent},
  frames{},
  frame_rate{fps},
  previewed_frame{0},
  previewed_frame_time{0.0f}
{
}

void Animation::setName(const QString& name)
{
  setData(name, Qt::DisplayRole);
}

void Animation::addFrame(AnimationFrameSourcePtr anim_source)
{
  frames.emplace_back(anim_source, 1.0f / float(frame_rate));
}

void Animation::addFrame(AnimationFrameInstance frame)
{
  frames.push_back(std::move(frame));
}

AnimationFrameInstance* Animation::frameAt(int index)
{
  return &frames.at(index);
}

void Animation::notifyPreviewFrameChanged()
{
  emit parent->signalPreviewFrameSelected(this);
}

void Animation::notifyChanged()
{
  parent->notifyAnimationChanged(this);
}
