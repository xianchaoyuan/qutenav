/* -*- coding: utf-8-unix -*-
 *
 * File: oesureader/src/oesureader.cpp
 *
 * Copyright (C) 2021 Jukka Sirkka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "oesureader.h"
#include "osenc.h"
#include "ocdevice.h"
#include "logging.h"
#include <QFileInfo>


class OesuDevice: public OCDevice {
public:
  OesuDevice(const QString& path, ReadMode mode):
    OCDevice(path, mode, serverEPName) {}

  static inline const char serverEPName[] = "/tmp/OCPN_PIPEX";
  static inline const char serverPath[] = "/usr/bin/oexserverd";

};

const GeoProjection* OesuReader::geoprojection() const {
  return m_proj;
}

OesuReader::OesuReader(const QString& name)
  : ChartFileReader(name)
  , m_proj(GeoProjection::CreateProjection("SimpleMercator"))
{}

OesuReader::~OesuReader() {
  delete m_proj;
}

static void read_server_status(const QString& path, QIODevice* device) {
   // a hack to skip oesenc files: server does not send status for these
  if (QFileInfo(path).suffix() == "oesenc") return;
  QDataStream stream(device);
  Buffer buffer;
  // auto status =
  read_record<OSENC_ServerStat_Record_Payload>(buffer, stream, SencRecordType::SERVER_STATUS_RECORD);

  //  qCDebug(CENC) << "Status of " << QFileInfo(path).baseName();
  //  qCDebug(CENC) << "  serverStatus        =" << status->serverStatus;
  //  qCDebug(CENC) << "  decryptStatus       =" << status->decryptStatus;
  //  qCDebug(CENC) << "  expireStatus        =" << status->expireStatus;
  //  qCDebug(CENC) << "  expireDaysRemaining =" << status->expireDaysRemaining;
  //  qCDebug(CENC) << "  graceDaysAllowed    =" << status->graceDaysAllowed;
  //  qCDebug(CENC) << "  graceDaysRemaining  =" << status->graceDaysRemaining;
}

GeoProjection* OesuReader::configuredProjection(const QString &path) const {

  OesuDevice device(path, ReadMode::ReadHeader);
  if (!device.open(OCDevice::ReadOnly)) {
    throw ChartFileError(QString("Cannot open %1 for reading").arg(path));
  }

  read_server_status(path, &device);

  Osenc senc;
  return senc.configuredProjection(&device, m_proj->className());
}

S57ChartOutline OesuReader::readOutline(const QString &path, const GeoProjection *gp) const {

  OesuDevice device(path, ReadMode::ReadHeader);
  if (!device.open(OCDevice::ReadOnly)) {
    throw ChartFileError(QString("Cannot open %1 for reading").arg(path));
  }

  read_server_status(path, &device);

  Osenc senc;
  return senc.readOutline(&device, gp);
}


void OesuReader::readChart(GL::VertexVector& vertices,
                            GL::IndexVector& indices,
                            S57::ObjectVector& objects,
                            const QString& path,
                            const GeoProjection* gp) const {
  OesuDevice device(path, ReadMode::ReadSENC);
  if (!device.open(OCDevice::ReadOnly)) {
    throw ChartFileError(QString("Cannot open %1 for reading").arg(path));
  }

  read_server_status(path, &device);

  Osenc senc;
  senc.readChart(vertices, indices, objects, &device, gp);
}


QString OesuReaderFactory::name() const {
  return "oesu";
}

QString OesuReaderFactory::displayName() const {
  return "OESU Encrypted Charts";
}

QStringList OesuReaderFactory::filters() const {
  return QStringList {"*.oesu", "*.oesenc"};
}

ChartFileReader* OesuReaderFactory::create() const {
  return new OesuReader(name());
}

class OesuServerManager: public OCServerManager {
public:
  static OesuServerManager* instance() {
    static auto manager = new OesuServerManager();
    return manager;
  }
private:
  OesuServerManager(): OCServerManager(OesuDevice::serverPath, OesuDevice::serverEPName) {}
};

void OesuReaderFactory::initialize(const QStringList&) const {
  OesuServerManager::instance()->init();
}
