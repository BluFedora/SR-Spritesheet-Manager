//
// SR Spritesheet Manager
//
// file:   srsm_animation.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "srsm_animation.hpp"

#include "Data/srsm_project.hpp"  // Project

AnimationFrame::AnimationFrame(const QString& rel_path, const QString& full_path, float frame_time) :
  QStandardItem(rel_path)
{
  setFlags(flags() & ~(Qt::ItemIsDropEnabled | Qt::ItemIsEditable));
  setData(rel_path, Qt::UserRole + 1);
  setData(full_path, Qt::UserRole + 2);
  setData(frame_time, Qt::UserRole + 3);
}

QStandardItem* AnimationFrame::clone() const
{
  return new AnimationFrame(rel_path(), full_path(), frame_time());
}

int AnimationFrame::type() const
{
  return UserType;
}

Animation::Animation(Project* parent, const QString& name, int fps) :
  QStandardItem(name),
  parent{parent},
  frame_rate{fps},
  frame_list{}
{
  frame_list.setItemPrototype(new AnimationFrame("", "", 0.0f));
}

void Animation::addFrame(const QString& rel_path, const QString& full_path)
{
  addFrame(new AnimationFrame(rel_path, full_path, 1.0f / float(frame_rate)));
}

void Animation::addFrame(AnimationFrame* frame)
{
  frame_list.appendRow(frame);
}

AnimationFrame* Animation::frameAt(int index) const
{
  return dynamic_cast<AnimationFrame*>(frame_list.item(index, 0));
}

void Animation::swapFrames(int a, int b)
{
  Q_ASSERT(a != b);

  if (a > b)
  {
    std::swap(a, b);
  }

  auto item_a = frame_list.takeRow(a)[0];
  auto item_b = frame_list.takeRow(b - 1)[0];

  frame_list.insertRow(a, item_b);
  frame_list.insertRow(b, item_a);
}

void Animation::notifyChanged()
{
  parent->notifyAnimationChanged(this);
}
