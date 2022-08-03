#include "sr_live_reload_server.hpp"

#include "sprite_anim/bf_sprite_animation.hpp"

LiveReloadServer::LiveReloadServer() :
  QObject(nullptr),
  m_Server{nullptr},
  m_Clients{},
  m_NumBytesNeedSend{0},
  m_NumBytesSent{0}
{
}

void LiveReloadServer::setup()
{
  QObject::connect(&m_Server, &QTcpServer::newConnection, [this]() {
    QTcpSocket *const client_socket = m_Server.nextPendingConnection();

    // client_socket->waitForBytesWritten(-1);

    QObject::connect(client_socket, &QTcpSocket::disconnected, this, &LiveReloadServer::onClientDisconnect);
    QObject::connect(client_socket, &QTcpSocket::bytesWritten, this, &LiveReloadServer::onClientBytesSent);

    m_Clients.push_back(client_socket);
    qDebug() << "onClientConnect";
  });
}

void LiveReloadServer::sendAnimationAdded(const QUuid &spritesheet, const Animation &animation, const QMap<QString, std::uint32_t> &frame_to_index)
{
  if (!m_Clients.isEmpty())
  {
    SpriteAnim::LiveReloadPacketHeader event = {SpriteAnim::LiveReloadPacketHeader::AnimationAdded};
    std::memcpy(&event.target_spritesheet, &spritesheet, sizeof(QUuid));

    // Points into the same string as the animation data.
    event.target_animation.elements.offset = sizeof(event.target_animation) +
                                             sizeof(SpriteAnim::SpriteAnimation);
    event.target_animation.num_elements = animation.name().length();

    packetSizeAddAnimation(event, animation);

    writeEventHeader(event);
    writeAnimationData(animation, frame_to_index);
  }
}

static_assert(sizeof(QUuid) == sizeof(uuid128), "UUID Types must be binary compatible.");

void LiveReloadServer::sendAnimationRenamed(const QUuid &spritesheet, const QString &old_name, const Animation &animation, const QMap<QString, std::uint32_t> &frame_to_index)
{
  if (!m_Clients.isEmpty())
  {
    SpriteAnim::LiveReloadPacketHeader event = {SpriteAnim::LiveReloadPacketHeader::AnimationRenamed};
    std::memcpy(&event.target_spritesheet, &spritesheet, sizeof(QUuid));

    event.target_animation.elements.offset = sizeof(event.target_animation);
    event.target_animation.num_elements    = old_name.length();
    packetSizeAddString(event, old_name);
    packetSizeAddAnimation(event, animation);

    writeEventHeader(event);
    writeString(old_name);
    writeAnimationData(animation, frame_to_index);
  }
}

void LiveReloadServer::sendAnimationFramesChanged(const QUuid &spritesheet, const Animation &animation, const QMap<QString, std::uint32_t> &frame_to_index)
{
  if (!m_Clients.isEmpty())
  {
    SpriteAnim::LiveReloadPacketHeader event = {SpriteAnim::LiveReloadPacketHeader::AnimationFramesChanged};
    std::memcpy(&event.target_spritesheet, &spritesheet, sizeof(QUuid));

    // Points into the same string as the animation data.
    event.target_animation.elements.offset = sizeof(event.target_animation) +
                                             sizeof(SpriteAnim::SpriteAnimation);
    event.target_animation.num_elements = animation.name().length();

    packetSizeAddAnimation(event, animation);

    writeEventHeader(event);
    writeAnimationData(animation, frame_to_index);
  }
}

void LiveReloadServer::sendAnimationRemoved(const QUuid &spritesheet, const QString &animation_name)
{
  if (!m_Clients.isEmpty())
  {
    SpriteAnim::LiveReloadPacketHeader event = {SpriteAnim::LiveReloadPacketHeader::AnimationRemoved};
    std::memcpy(&event.target_spritesheet, &spritesheet, sizeof(QUuid));

    event.target_animation.elements.offset = sizeof(event.target_animation);
    event.target_animation.num_elements    = animation_name.length();
    packetSizeAddString(event, animation_name);

    writeEventHeader(event);
    writeString(animation_name);
  }
}

void LiveReloadServer::sendAtlasTextureChanged(const QUuid &spritesheet, const QImage &atlas_image)
{
  if (!m_Clients.isEmpty())
  {
    SpriteAnim::LiveReloadPacketHeader event = {SpriteAnim::LiveReloadPacketHeader::AtlasTextureChanged};
    std::memcpy(&event.target_spritesheet, &spritesheet, sizeof(QUuid));

    const auto image_bytes_size = atlas_image.sizeInBytes();

    event.packet_size = sizeof(SpriteAnim::LiveReloadPacketAtlasTexture);
    event.packet_size += image_bytes_size;

    const std::uint16_t atlas_width  = atlas_image.width();
    const std::uint16_t atlas_height = atlas_image.height();

    writeEventHeader(event);

    assetio::rel_array32<char> texture_data = {};
    texture_data.elements.offset            = sizeof(texture_data) +
                                   sizeof(std::uint16_t) +
                                   sizeof(std::uint16_t);
    texture_data.num_elements = image_bytes_size;

    for (QTcpSocket *const client : m_Clients)
    {
      client->write((const char *)&texture_data, sizeof(texture_data));
      client->write((const char *)&atlas_width, sizeof(atlas_width));
      client->write((const char *)&atlas_height, sizeof(atlas_height));
      client->write((const char *)atlas_image.constBits(), image_bytes_size);
    }
  }
}

LiveReloadServer::~LiveReloadServer()
{
  for (QTcpSocket *client : m_Clients)
  {
    client->close();
    client->deleteLater();
  }
}

void LiveReloadServer::onClientBytesSent(qint64 num_bytes)
{
  m_NumBytesSent += num_bytes;

  emit bytesSent();

  // if (m_NumBytesSent == m_NumBytesNeedSend)
  // {
  //   m_NumBytesSent     = 0;
  //   m_NumBytesNeedSend = 0;
  // }
}

void LiveReloadServer::onClientDisconnect()
{
  qDebug() << "onClientDisconnect";
  QTcpSocket *const client = static_cast<QTcpSocket *>(QObject::sender());
  m_Clients.removeOne(client);
}

void LiveReloadServer::writeEventHeader(const SpriteAnim::LiveReloadPacketHeader &event)
{
  for (QTcpSocket *const client : m_Clients)
  {
    client->write((const char *)&event, sizeof(event));
  }
}

void LiveReloadServer::writeString(const QString &str)
{
  const QByteArray utf8_str = str.toUtf8();

  for (QTcpSocket *const client : m_Clients)
  {
    client->write(utf8_str.data(), utf8_str.size());
  }
}

void LiveReloadServer::writeAnimationData(const Animation &animation, const QMap<QString, std::uint32_t> &frame_to_index)
{
  SpriteAnim::SpriteAnimation animation_data;
  animation_data.name.elements.offset   = sizeof(animation_data);
  animation_data.name.num_elements      = animation.name().length();
  animation_data.frames.elements.offset = animation_data.name.elements.offset + animation_data.name.num_elements * sizeof(char);
  animation_data.frames.num_elements    = uint32_t(animation.frames.size());

  for (QTcpSocket *const client : m_Clients)
  {
    client->write((const char *)&animation_data, sizeof(animation_data));
  }

  writeString(animation.name());

  for (QTcpSocket *const client : m_Clients)
  {
    for (const auto &frame : animation.frames)
    {
      SpriteAnim::SpriteAnimationFrame frame_data;
      frame_data.frame_index = frame_to_index[frame.full_path()];
      frame_data.frame_time  = frame.frame_time;

      client->write((const char *)&frame_data, sizeof(frame_data));
    }
  }
}

void LiveReloadServer::packetSizeAddString(SpriteAnim::LiveReloadPacketHeader &event, const QString &str)
{
  const auto utf8_str = str.toUtf8();

  event.packet_size += utf8_str.size() * sizeof(char);
}

void LiveReloadServer::packetSizeAddAnimation(SpriteAnim::LiveReloadPacketHeader &event, const Animation &animation)
{
  event.packet_size += sizeof(SpriteAnim::SpriteAnimation);
  packetSizeAddString(event, animation.name());
  event.packet_size += animation.frames.size() * sizeof(SpriteAnim::SpriteAnimationFrame);
}

std::unique_ptr<LiveReloadServer> g_Server;
