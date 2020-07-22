#include "srsm_animated_sprite.hpp"

AnimatedSprite::AnimatedSprite(Project& project) :
  m_Project{&project},
  m_Size{256.0f, 256.0f},
  m_UvRect{0.0f, 0.0f, 1.0f, 1.0f},
  m_Flags{State::None}
{
  setFlags(QGraphicsItem::ItemIsSelectable  | QGraphicsItem::ItemIsMovable);
  setCacheMode(NoCache);
}
