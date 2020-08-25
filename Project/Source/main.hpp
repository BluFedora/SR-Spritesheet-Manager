#ifndef MAIN_HPP
#define MAIN_HPP

#include "bf/Animation2D.h"  // k_bfSRSMServerPort
#include "bf/anim2D/bf_anim2D_network.h"

#include <QBuffer>
#include <QImage>
#include <QObject>     // QObject
#include <QTcpServer>  // QTcpServer
#include <QTcpSocket>  // QTcpSocket

class LiveReloadServer final : public QObject
{
  Q_OBJECT

 private:
  QTcpServer            m_Server;
  QVector<QTcpSocket *> m_Clients;

 public:
  LiveReloadServer() :
    QObject(nullptr),
    m_Server{nullptr},
    m_Clients{}
  {
  }

  bool startServer()
  {
    return m_Server.listen(QHostAddress::LocalHost, k_bfSRSMServerPort);
  }

  void setup()
  {
    QObject::connect(&m_Server, &QTcpServer::newConnection, [this]() {
      QTcpSocket *const client_socket = m_Server.nextPendingConnection();

      qDebug() << "onClientConnect";

      //client_socket->waitForBytesWritten(-1);

      QObject::connect(client_socket, &QTcpSocket::disconnected, this, &LiveReloadServer::onClientDisconnect);

      m_Clients.push_back(client_socket);
    });
  }

  void sendTextureChangedPacket(const char *guid, const QImage &atlas_image)
  {
    if (m_Clients.isEmpty())
    {
      return;
    }

    bfAnim2DPacketHeader header;

    QByteArray image_bytes;
    QBuffer    buffer(&image_bytes);
    buffer.open(QIODevice::WriteOnly);
    atlas_image.save(&buffer, "png");

    const uint32_t image_bytes_size = image_bytes.size();

    header.packet_size = uint32_t(k_bfAnim2DHeaderSize + k_bfAnim2DGUIDSize + sizeof(image_bytes_size) + image_bytes_size);
    header.packet_type = bfAnim2DPacketType_TextureChanged;

    for (QTcpSocket *client : m_Clients)
    {
      client->write((const char *)&header, k_bfAnim2DHeaderSize);
      client->write((const char *)guid, k_bfAnim2DGUIDSize);
      client->write((const char *)&image_bytes_size, sizeof(image_bytes_size));
      client->write(image_bytes.data(), image_bytes_size);
    }
  }

  void sendAnimChangedPacket(const char *guid, const QBuffer &srsm_byte_buffer)
  {
    if (m_Clients.isEmpty())
    {
      return;
    }

    const uint32_t srsm_byte_buffer_size = srsm_byte_buffer.size();

    bfAnim2DPacketHeader header;

    header.packet_size = uint32_t(k_bfAnim2DHeaderSize + k_bfAnim2DGUIDSize + sizeof(srsm_byte_buffer_size) + srsm_byte_buffer_size);
    header.packet_type = bfAnim2DPacketType_SpritesheetChanged;

    for (QTcpSocket *client : m_Clients)
    {
      client->write((const char *)&header, k_bfAnim2DHeaderSize);
      client->write((const char *)guid, k_bfAnim2DGUIDSize);
      client->write((const char *)&srsm_byte_buffer_size, sizeof(srsm_byte_buffer_size));
      client->write(srsm_byte_buffer.data(), srsm_byte_buffer_size);
    }
  }

  ~LiveReloadServer()
  {
    for (QTcpSocket *client : m_Clients)
    {
      client->close();
      client->deleteLater();
    }
  }

 private slots:
  void onClientDisconnect()
  {
    qDebug() << "onClientDisconnect";
    QTcpSocket *const client = static_cast<QTcpSocket *>(QObject::sender());
    m_Clients.removeOne(client);
  }
};

extern std::unique_ptr<LiveReloadServer> g_Server;

#endif  // MAIN_HPP
