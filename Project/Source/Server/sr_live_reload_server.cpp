#include "sr_live_reload_server.hpp"

#include "bf/anim2D/bf_anim2D_network.h"  // bfAnim2DPacketHeader

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

    //client_socket->waitForBytesWritten(-1);

    QObject::connect(client_socket, &QTcpSocket::disconnected, this, &LiveReloadServer::onClientDisconnect);
    QObject::connect(client_socket, &QTcpSocket::bytesWritten, this, &LiveReloadServer::onClientBytesSent);

    m_Clients.push_back(client_socket);
    qDebug() << "onClientConnect";
  });
}

void LiveReloadServer::sendTextureChangedPacket(const char *guid, const QImage &atlas_image)
{
  if (!m_Clients.isEmpty())
  {
    QByteArray image_bytes;
    QBuffer    buffer(&image_bytes);
    buffer.open(QIODevice::WriteOnly);
    atlas_image.save(&buffer, "png");
    buffer.close();

    bfAnim2DPacketHeader header;
    const uint32_t       image_bytes_size = image_bytes.size();

    header.packet_size = uint32_t(k_bfAnim2DTotalHeaderSize + sizeof(image_bytes_size) + image_bytes_size);
    header.packet_type = bfAnim2DPacketType_TextureChanged;

    m_NumBytesNeedSend += header.packet_size * m_Clients.size();

    for (QTcpSocket *client : m_Clients)
    {
      client->write((const char *)&header, k_bfAnim2DHeaderSize);
      client->write((const char *)guid, k_bfAnim2DGUIDSize);
      client->write((const char *)&image_bytes_size, sizeof(image_bytes_size));
      client->write(image_bytes.data(), image_bytes_size);
    }
  }
}

void LiveReloadServer::sendAnimChangedPacket(const char *guid, const QBuffer &srsm_byte_buffer)
{
  if (!m_Clients.isEmpty())
  {
    bfAnim2DPacketHeader header;
    const uint32_t       srsm_byte_buffer_size = srsm_byte_buffer.size();

    header.packet_size = uint32_t(k_bfAnim2DTotalHeaderSize + sizeof(srsm_byte_buffer_size) + srsm_byte_buffer_size);
    header.packet_type = bfAnim2DPacketType_SpritesheetChanged;

    m_NumBytesNeedSend += header.packet_size * m_Clients.size();

    for (QTcpSocket *client : m_Clients)
    {
      client->write((const char *)&header, k_bfAnim2DHeaderSize);
      client->write((const char *)guid, k_bfAnim2DGUIDSize);
      client->write((const char *)&srsm_byte_buffer_size, sizeof(srsm_byte_buffer_size));
      client->write(srsm_byte_buffer.data(), srsm_byte_buffer_size);
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

std::unique_ptr<LiveReloadServer> g_Server;
