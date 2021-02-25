#include "osenc.h"
#include <QDataStream>
#include <functional>
#include <QDate>
#include "s52names.h"
#include "chartfilereader.h"


using Buffer = QVector<char>;
using HandlerFunc = std::function<bool (const Buffer&)>;

struct Handler {
  Handler(HandlerFunc f): func(std::move(f)) {}
  HandlerFunc func;
  ~Handler() = default;
};

using Region = S57ChartOutline::Region;
using VVec = QVector<glm::vec2>;

const uint UseCoverage = 1;
const uint UseNoCoverage = 2;

static uint checkCoverage(const Region& cov,
                          const Region& nocov,
                          const WGS84PointVector& ps,
                          const GeoProjection* gp);

static WGS84PointVector reduce(const VVec& vs);

GeoProjection* Osenc::configuredProjection(QIODevice *device, const QString &clsName) const {

  QDataStream stream(device);

  Buffer buffer;
  const int baseSize = sizeof(OSENC_Record_Base);

  buffer.resize(baseSize);
  if (stream.readRawData(buffer.data(), baseSize) < baseSize) {
    throw ChartFileError(QString("Error reading %1 bytes").arg(buffer.size()));
  }

  //  For identification purposes, the very first record must be the OSENC Version Number Record
  auto record = reinterpret_cast<OSENC_Record_Base*>(buffer.data());

  // Check Record
  if (record->record_type != SencRecordType::HEADER_SENC_VERSION){
    throw ChartFileError(QString("Not a supported senc file"));
  }

  //  This is the correct record type (OSENC Version Number Record), so read it
  buffer.resize(record->record_length - baseSize);
  if (stream.readRawData(buffer.data(), buffer.size()) < buffer.size()) {
    throw ChartFileError(QString("Error reading %1 bytes from device").arg(buffer.size()));
  }
  // auto p16 = reinterpret_cast<quint16*>(buffer.data());
  // qDebug() << "senc version =" << *p16;

  WGS84Point ref;

  const QMap<SencRecordType, Handler*> handlers {
    {SencRecordType::HEADER_CELL_NAME, new Handler([] (const Buffer& b) {
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_PUBLISHDATE, new Handler([] (const Buffer& b) {
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_UPDATEDATE, new Handler([] (const Buffer& b) {
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_EDITION, new Handler([] (const Buffer& b) {
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_UPDATE, new Handler([] (const Buffer& b) {
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_NATIVESCALE, new Handler([] (const Buffer& b) {
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_SENCCREATEDATE, new Handler([] (const Buffer& b) {
        return true;
      })
    },
    {SencRecordType::CELL_EXTENT_RECORD, new Handler([&ref] (const Buffer& b) {
        // qDebug() << "cell extent record";
        const OSENC_EXTENT_Record_Payload* p = reinterpret_cast<const OSENC_EXTENT_Record_Payload*>(b.constData());

        auto sw = WGS84Point::fromLL(p->extent_sw_lon, p->extent_sw_lat);
        auto ne = WGS84Point::fromLL(p->extent_ne_lon, p->extent_ne_lat);

        ref = WGS84Point::fromLL(.5 * (sw.lng() + ne.lng()),
                                 .5 * (sw.lat() + ne.lat()));

        return true;
      })
    },
    {SencRecordType::HEADER_CELL_SOUNDINGDATUM, new Handler([] (const Buffer& b) {
        return true;
      })
    },
  };

  bool done = false;

  while (!done) {

    buffer.resize(baseSize);
    if (stream.readRawData(buffer.data(), baseSize) < baseSize) {
      done = true;
      continue;
    }
    record = reinterpret_cast<OSENC_Record_Base*>(buffer.data());
    // copy, record_type will be overwritten in the next stream.readRawData
    SencRecordType rec_type = record->record_type;
    if (!handlers.contains(rec_type)) {
      done = true;
      continue;
    }
    buffer.resize(record->record_length - baseSize);
    if (stream.readRawData(buffer.data(), buffer.size()) < buffer.size()) {
      done = true;
      continue;
    }

    done = !(handlers[rec_type])->func(buffer);

  }
  qDeleteAll(handlers);

  auto gp = GeoProjection::CreateProjection(clsName);
  gp->setReference(ref);
  return gp;
}

S57ChartOutline Osenc::readOutline(QIODevice *device, const GeoProjection* gp) const {

  QDataStream stream(device);

  Buffer buffer;
  const int baseSize = sizeof(OSENC_Record_Base);

  buffer.resize(baseSize);
  if (stream.readRawData(buffer.data(), baseSize) < baseSize) {
    throw ChartFileError(QString("Error reading %1 bytes from device").arg(baseSize));
  }

  //  For identification purposes, the very first record must be the OSENC Version Number Record
  auto record = reinterpret_cast<OSENC_Record_Base*>(buffer.data());

  // Check Record
  if (record->record_type != SencRecordType::HEADER_SENC_VERSION){
    throw ChartFileError(QString("Not a supported senc file"));
  }

  //  This is the correct record type (OSENC Version Number Record), so read it
  buffer.resize(record->record_length - baseSize);
  if (stream.readRawData(buffer.data(), buffer.size()) < buffer.size()) {
    throw ChartFileError(QString("Error reading %1 bytes from device").arg(buffer.size()));
  }
  // auto p16 = reinterpret_cast<quint16*>(buffer.data());
  // qDebug() << "senc version =" << *p16;

  QDate pub;
  QDate mod;
  quint32 scale = 0;
  WGS84PointVector points;
  Region cov;
  Region nocov;

  const QMap<SencRecordType, Handler*> handlers {
    {SencRecordType::HEADER_CELL_NAME, new Handler([] (const Buffer& b) {
        // qDebug() << "cell name" << QString::fromUtf8(b.constData());
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_PUBLISHDATE, new Handler([&pub] (const Buffer& b) {
        auto s = QString::fromUtf8(b.constData());
        // qDebug() << "cell publishdate" << s;
        pub = QDate::fromString(s, "yyyyMMdd");
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_UPDATEDATE, new Handler([&mod] (const Buffer& b) {
        auto s = QString::fromUtf8(b.constData());
        // qDebug() << "cell modified date" << s;
        mod = QDate::fromString(s, "yyyyMMdd");
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_EDITION, new Handler([] (const Buffer& b) {
        // const quint16* p16 = reinterpret_cast<const quint16*>(b.constData());
        // qDebug() << "cell edition" << *p16;
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_UPDATE, new Handler([] (const Buffer& b) {
        // const quint16* p16 = reinterpret_cast<const quint16*>(b.constData());
        // qDebug() << "cell update" << *p16;
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_NATIVESCALE, new Handler([&scale] (const Buffer& b) {
        const quint32* p32 = reinterpret_cast<const quint32*>(b.constData());
        // qDebug() << "cell nativescale" << *p32;
        scale = *p32;
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_SENCCREATEDATE, new Handler([] (const Buffer& b) {
        // qDebug() << "cell senccreatedate" << QString::fromUtf8(b.constData());
        return true;
      })
    },
    {SencRecordType::CELL_EXTENT_RECORD, new Handler([&points] (const Buffer& b) {
        // qDebug() << "cell extent record";
        const OSENC_EXTENT_Record_Payload* p = reinterpret_cast<const OSENC_EXTENT_Record_Payload*>(b.constData());
        points << WGS84Point::fromLL(p->extent_sw_lon, p->extent_sw_lat);
        points << WGS84Point::fromLL(p->extent_se_lon, p->extent_se_lat);
        points << WGS84Point::fromLL(p->extent_ne_lon, p->extent_ne_lat);
        points << WGS84Point::fromLL(p->extent_nw_lon, p->extent_nw_lat);

        return true;
      })
    },

    {SencRecordType::HEADER_CELL_SOUNDINGDATUM, new Handler([] (const Buffer& b) {
        return true;
      })
    },

    {SencRecordType::CELL_COVR_RECORD, new Handler([&cov] (const Buffer& b) {
        // qDebug() << "cell coverage record";
        auto p = reinterpret_cast<const OSENC_POINT_ARRAY_Record_Payload*>(b.constData());
        VVec vs(p->count);
        memcpy(vs.data(), &p->array, p->count * 2 * sizeof(GLfloat));
        cov.append(reduce(vs));
        return true;
      })
    },

    {SencRecordType::CELL_NOCOVR_RECORD, new Handler([&nocov] (const Buffer& b) {
        // qDebug() << "cell nocoverage record";
        auto p = reinterpret_cast<const OSENC_POINT_ARRAY_Record_Payload*>(b.constData());
        VVec vs(p->count);
        memcpy(vs.data(), &p->array, p->count * 2 * sizeof(GLfloat));
        nocov.append(reduce(vs));
        return true;
      })
    },

  };

  bool done = false;

  while (!done) {

    buffer.resize(baseSize);
    if (stream.readRawData(buffer.data(), baseSize) < baseSize) {
      done = true;
      continue;
    }
    record = reinterpret_cast<OSENC_Record_Base*>(buffer.data());
    // copy, record_type will be overwritten in the next stream.readRawData
    SencRecordType rec_type = record->record_type;
    if (!handlers.contains(rec_type)) {
      qWarning() << "Unhandled record type" << static_cast<int>(rec_type);
      done = true;
      continue;
    }
    buffer.resize(record->record_length - baseSize);
    if (stream.readRawData(buffer.data(), buffer.size()) < buffer.size()) {
      qWarning() << "Stream read failed";
      done = true;
      continue;
    }

    done = !(handlers[rec_type])->func(buffer);
    if (done) {
      qWarning() << "Handler" << static_cast<int>(rec_type) << "failed";
    }

  }
  qDeleteAll(handlers);
  if (!pub.isValid() || !mod.isValid() || scale == 0 || points.isEmpty()) {
    throw ChartFileError(QString("Invalid osenc header"));
  }

  uint flags = checkCoverage(cov, nocov, points, gp);
  // points = sw, se, ne, nw
  return S57ChartOutline(points[0], points[2],
      (flags & UseCoverage) ? cov : Region(),
      (flags & UseNoCoverage) ? nocov : Region(),
      scale, pub, mod);
}


namespace S57 {

// Helper class to set Object's private data
class ObjectBuilder {
public:
  void osEncAddAttribute(S57::Object* obj, quint16 acode, const Attribute& a) const {
    obj->m_attributes[acode] = a;
  }
  void osEncAddString(S57::Object* obj, quint16 acode, const QString& s) const {

    if (s.isEmpty()) {
      qDebug() << "S57::AttributeType::Any";
      obj->m_attributes[acode] = S57::Attribute(S57::AttributeType::Any);
      return;
    }

    if (s == "?") {
      qDebug() << "S57::AttributeType::None";
      obj->m_attributes[acode] = S57::Attribute(S57::AttributeType::None);
      return;
    }

    if (S52::GetAttributeType(acode) == S57::Attribute::Type::IntegerList) {
      const QStringList tokens = s.split(",");
      QVariantList vs;
      for (const QString& t: tokens) {
        vs << QVariant::fromValue(t.toInt());
      }
      // qDebug() << S52::GetAttributeName(acode) << vs;
      obj->m_attributes[acode] = S57::Attribute(vs);
    } else {
      obj->m_attributes[acode] = S57::Attribute(s);
    }
  }

  bool osEncSetGeometry(S57::Object* obj, S57::Geometry::Base* g, const QRectF& bb) const {
    if (obj->m_geometry != nullptr) {
      // qDebug() << as_numeric(obj->m_geometry->type());
      return false;
    }
    obj->m_geometry = g;
    obj->m_bbox = bb;
    return true;
  }
};

}


void Osenc::readChart(GL::VertexVector& vertices,
                            GL::IndexVector& indices,
                            S57::ObjectVector& objects,
                            QIODevice* device,
                            const GeoProjection* gp) const {


  PointRefMap connected;
  RawEdgeMap edges;

  S57::Object* current = nullptr;
  ObjectWrapperVector features;

  S57::ObjectBuilder helper;

  bool hasReversed = false;

  QDataStream stream(device);


  const QMap<SencRecordType, Handler*> handlers {

    {SencRecordType::HEADER_SENC_VERSION, new Handler([&hasReversed] (const Buffer& b) {
        quint16 version = *(reinterpret_cast<const quint16*>(b.constData()));
        qDebug() << "senc version" << version;
        hasReversed = version > 200;
        return true;
      })
    },

    {SencRecordType::FEATURE_ID_RECORD, new Handler([&current, &features] (const Buffer& b) {
        // qDebug() << "feature id record";
        auto p = reinterpret_cast<const OSENC_Feature_Identification_Record_Payload*>(b.constData());
        current = new S57::Object(p->feature_ID, p->feature_type_code);
        features.append(ObjectWrapper(current));
        return true;
      })
    },

    {SencRecordType::FEATURE_ATTRIBUTE_RECORD, new Handler([&current, helper] (const Buffer& b) {
        if (!current) return false;
        auto p = reinterpret_cast<const OSENC_Attribute_Record_Payload*>(b.constData());
        auto t = as_enum<AttributeRecType>(p->attribute_value_type, AllAttrTypes);
        switch (t) {
        case AttributeRecType::Integer: {
          int v;
          memcpy(&v, &p->attribute_data, sizeof(int));
          helper.osEncAddAttribute(current,
                                   p->attribute_type_code,
                                   S57::Attribute(v));
          return true;
        }
        case AttributeRecType::Real: {
          double v;
          memcpy(&v, &p->attribute_data, sizeof(double));
          helper.osEncAddAttribute(current,
                                   p->attribute_type_code,
                                   S57::Attribute(v));
          return true;
        }
        case AttributeRecType::String: {
          const char* s = &p->attribute_data; // null terminated string
          // handles strings and integer lists
          helper.osEncAddString(current,
                                p->attribute_type_code,
                                QString::fromUtf8(s));
          return true;
        }
        default: return false;
        }
        return false;
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_POINT, new Handler([&current, &features, helper, gp] (const Buffer& b) {
        // qDebug() << "feature geometry/point record";
        if (!current) return false;
        auto p = reinterpret_cast<const OSENC_PointGeometry_Record_Payload*>(b.constData());
        auto p0 = gp->fromWGS84(WGS84Point::fromLL(p->lon, p->lat));
        features.last().geom = S57::Geometry::Type::Point;
        QRectF bb(p0 - QPointF(10, 10), QSizeF(20, 20));
        return helper.osEncSetGeometry(current, new S57::Geometry::Point(p0, gp), bb);
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_LINE, new Handler([&features, &hasReversed] (const Buffer& b) {
        // qDebug() << "feature geometry/line record";
        auto p = reinterpret_cast<const OSENC_LineGeometry_Record_Payload*>(b.constData());
        const uint stride = hasReversed ? 4 : 3;
        QVector<int> es(p->edgeVector_count * stride);
        memcpy(es.data(), &p->edge_data, p->edgeVector_count * stride * sizeof(int));
        RawEdgeRefVector refs;
        for (uint i = 0; i < p->edgeVector_count; i++) {
          RawEdgeRef ref;
          ref.begin = es[stride * i];
          ref.index = std::abs(es[stride * i + 1]);
          ref.end = es[stride * i + 2];
          if (hasReversed) {
            ref.reversed = es[stride * i + stride - 1] != 0;
          } else {
            ref.reversed = es[stride * i + 1] < 0;
          }
          refs.append(ref);
        }
        features.last().edgeRefs.append(refs);
        features.last().geom = S57::Geometry::Type::Line;
        return true;
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_AREA, new Handler([&features, &hasReversed] (const Buffer& b) {
        // qDebug() << "feature geometry/area record";
        auto p = reinterpret_cast<const OSENC_AreaGeometry_Record_Payload*>(b.constData());

        const uint8_t* ptr = &p->edge_data;

        // skip contour counts
        ptr += p->contour_count * sizeof(int);

        TrianglePatchVector ts(p->triprim_count);
        for (uint i = 0; i < p->triprim_count; i++) {
          ts[i].mode = static_cast<GLenum>(*ptr); // Note: relies on GL_TRIANGLE* to be less than 128
          ptr++;
          auto nvert = *reinterpret_cast<const uint32_t*>(ptr);
          ptr += sizeof(uint32_t);
          ptr += 4 * sizeof(double); // skip bbox
          ts[i].vertices.resize(nvert * 2);
          memcpy(ts[i].vertices.data(), ptr, nvert * 2 * sizeof(float));
          ptr += nvert * 2 * sizeof(float);
        }

        features.last().triangles.append(ts);
        features.last().geom = S57::Geometry::Type::Area;

        const uint stride = hasReversed ? 4 : 3;
        QVector<int> es(p->edgeVector_count * stride);
        memcpy(es.data(), ptr, p->edgeVector_count * stride * sizeof(int));

        RawEdgeRefVector refs;
        for (uint i = 0; i < p->edgeVector_count; i++) {
          RawEdgeRef ref;
          ref.begin = es[stride * i];
          ref.index = std::abs(es[stride * i + 1]);
          ref.end = es[stride * i + 2];
          if (hasReversed) {
            ref.reversed = es[stride * i + stride - 1] != 0;
          } else {
            ref.reversed = es[stride * i + 1] < 0;
          }
          refs.append(ref);
        }
        features.last().edgeRefs.append(refs);

        return true;
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_MULTIPOINT, new Handler([&current, &features, helper, gp] (const Buffer& b) {
        // qDebug() << "feature geometry/multipoint record";
        if (!current) return false;
        auto p = reinterpret_cast<const OSENC_MultipointGeometry_Record_Payload*>(b.constData());
        GL::VertexVector ps(p->point_count * 3);
        memcpy(ps.data(), &p->point_data, p->point_count * 3 * sizeof(GLfloat));
        features.last().geom = S57::Geometry::Type::Point;
        auto bbox = ChartFileReader::computeSoundingsBBox(ps);
        return helper.osEncSetGeometry(current, new S57::Geometry::Point(ps, gp), bbox);
      })
    },

    {SencRecordType::VECTOR_EDGE_NODE_TABLE_RECORD, new Handler([&vertices, &edges] (const Buffer& b) {
        // qDebug() << "vector edge node table record";
        const char* ptr = b.constData();

        // The Feature(Object) count
        auto cnt = *reinterpret_cast<const quint32*>(ptr);
        ptr += sizeof(quint32);

        for (uint i = 0; i < cnt; i++) {
          auto index = *reinterpret_cast<const quint32*>(ptr);
          ptr += sizeof(quint32);

          auto pcnt = *reinterpret_cast<const quint32*>(ptr);
          ptr += sizeof(quint32);

          QVector<float> points(2 * pcnt);
          memcpy(points.data(), ptr, 2 * pcnt * sizeof(float));
          ptr += 2 * pcnt * sizeof(float);

          RawEdge edge;
          edge.first = vertices.size() / 2;
          edge.count = pcnt;
          edges[index] = edge;
          vertices.append(points);
        }
        return true;
      })
    },

    {SencRecordType::VECTOR_CONNECTED_NODE_TABLE_RECORD, new Handler([&vertices, &connected] (const Buffer& b) {
        // qDebug() << "vector connected node table record";
        const char* ptr = b.constData();

        // The Feature(Object) count
        auto cnt = *reinterpret_cast<const quint32*>(ptr);

        ptr += sizeof(quint32);

        for (uint i = 0; i < cnt; i++) {
          auto index = *reinterpret_cast<const quint32*>(ptr);
          ptr += sizeof(quint32);

          auto x = *reinterpret_cast<const float*>(ptr);
          ptr += sizeof(float);
          auto y = *reinterpret_cast<const float*>(ptr);
          ptr += sizeof(float);

          connected[index] = vertices.size() / 2;
          vertices.append(x);
          vertices.append(y);
        }
        return true;
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_AREA_EXT, new Handler([] (const Buffer& b) {
        throw NotImplementedError(QStringLiteral("FEATURE_GEOMETRY_RECORD_AREA_EXT not implemented"));
        return true;
      })
    },
    {SencRecordType::VECTOR_EDGE_NODE_TABLE_EXT_RECORD, new Handler([] (const Buffer& b) {
        throw NotImplementedError(QStringLiteral("VECTOR_EDGE_NODE_TABLE_EXT_RECORD not implemented"));
        return true;
      })
    },
    {SencRecordType::VECTOR_CONNECTED_NODE_TABLE_EXT_RECORD, new Handler([] (const Buffer& b) {
        throw NotImplementedError(QStringLiteral("VECTOR_CONNECTED_NODE_TABLE_EXT_RECORD not implemented"));
        return true;
      })
    },

  };

  bool done = false;

  const int baseSize = sizeof(OSENC_Record_Base);

  while (!done) {

    Buffer buffer;

    buffer.resize(baseSize);
    if (stream.readRawData(buffer.data(), baseSize) < baseSize) {
      done = true;
      continue;
    }

    auto record = reinterpret_cast<OSENC_Record_Base*>(buffer.data());

    // copy, record_type will be overwritten in the next stream.readRawData
    SencRecordType rec_type = record->record_type;
    buffer.resize(record->record_length - baseSize);
    if (stream.readRawData(buffer.data(), buffer.size()) < buffer.size()) {
      done = true;
      qWarning() << "Cannot read base record data";
      continue;
    }
    if (!handlers.contains(rec_type)) {
      // qWarning() << "Unhandled type" << static_cast<int>(rec_type);
      continue;
    }

    done = !(handlers[rec_type])->func(buffer);
    if (done) {
      qWarning() << "Handler failed for type" << as_numeric(rec_type);
    }

  }
  qDeleteAll(handlers);


  auto triangleGeometry = [&vertices] (const TrianglePatchVector& triangles, S57::ElementDataVector& elems) {
    GLint offset = 0;
    for (const TrianglePatch& p: triangles) {
      S57::ElementData d;
      d.mode = p.mode;
      d.offset = offset;
      d.count = p.vertices.size() / 2;
      vertices.append(p.vertices);
      elems.append(d);
      offset += d.count;
    }
  };

  using Edge = ChartFileReader::Edge;
  using EdgeVector = ChartFileReader::EdgeVector;

  for (ObjectWrapper& w: features) {
    // setup meta, line and area geometries

    if (w.geom == S57::Geometry::Type::Meta) {

      helper.osEncSetGeometry(w.object, new S57::Geometry::Meta(), QRectF());

    } else if (w.geom == S57::Geometry::Type::Line || w.geom == S57::Geometry::Type::Area) {
      EdgeVector shape;
      for (const RawEdgeRef& ref: w.edgeRefs) {
        Edge e;
        e.begin = connected[ref.begin];
        e.end = connected[ref.end];
        e.first = edges[ref.index].first;
        e.count = edges[ref.index].count;
        e.reversed = ref.reversed;
        e.inner = false; // not used
        shape.append(e);
      }
      auto lines = ChartFileReader::createLineElements(indices, vertices, shape);
      auto bbox = ChartFileReader::computeBBox(lines, vertices, indices);

      if (w.geom == S57::Geometry::Type::Area) {
        GLsizei offset = vertices.size() * sizeof(GLfloat);
        S57::ElementDataVector triangles;
        triangleGeometry(w.triangles, triangles);

        auto center = computeAreaCenter(triangles, vertices, offset);

        helper.osEncSetGeometry(w.object,
                                new S57::Geometry::Area(lines,
                                                        center,
                                                        triangles,
                                                        offset,
                                                        false,
                                                        gp),
                                bbox);

      } else {
        auto center = ChartFileReader::computeLineCenter(lines, vertices, indices);
        helper.osEncSetGeometry(w.object,
                                new S57::Geometry::Line(lines, center, 0, gp),
                                bbox);
      }
    }

    objects.append(w.object);
  }
}


QPointF Osenc::computeAreaCenter(const S57::ElementDataVector &elems,
                                 const GL::VertexVector& vertices,
                                 GLsizei offset) const {
  float tot = 0;
  QPointF s(0, 0);
  for (const S57::ElementData& elem: elems) {
    if (elem.mode == GL_TRIANGLES) {
      int first = offset / sizeof(GLfloat) + 2 * elem.offset;
      for (uint i = 0; i < elem.count / 3; i++) {
        const QPointF p0(vertices[first + 6 * i + 0], vertices[first + 6 * i + 1]);
        const QPointF p1(vertices[first + 6 * i + 2], vertices[first + 6 * i + 3]);
        const QPointF p2(vertices[first + 6 * i + 4], vertices[first + 6 * i + 5]);
        const float da = std::abs((p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y()));
        tot += da;
        s.rx() += da / 3. * (p0.x() + p1.x() + p2.x());
        s.ry() += da / 3. * (p0.y() + p1.y() + p2.y());
      }
    } else if (elem.mode == GL_TRIANGLE_STRIP) {
      int first = offset / sizeof(GLfloat) + 2 * elem.offset;
      for (uint i = 0; i < elem.count - 2; i++) {
        const QPointF p0(vertices[first + 2 * i + 0], vertices[first + 2 * i + 1]);
        const QPointF p1(vertices[first + 2 * i + 2], vertices[first + 2 * i + 3]);
        const QPointF p2(vertices[first + 2 * i + 4], vertices[first + 2 * i + 5]);
        const float da = std::abs((p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y()));
        tot += da;
        s.rx() += da / 3. * (p0.x() + p1.x() + p2.x());
        s.ry() += da / 3. * (p0.y() + p1.y() + p2.y());
      }
    } else if (elem.mode == GL_TRIANGLE_FAN) {
      int first = offset / sizeof(GLfloat) + 2 * elem.offset;
      const QPointF p0(vertices[first + 0], vertices[first + 1]);
      for (uint i = 0; i < elem.count - 2; i++) {
        const QPointF p1(vertices[first + 2 * i + 2], vertices[first + 2 * i + 3]);
        const QPointF p2(vertices[first + 2 * i + 4], vertices[first + 2 * i + 5]);
        const float da = std::abs((p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y()));
        tot += da;
        s.rx() += da / 3. * (p0.x() + p1.x() + p2.x());
        s.ry() += da / 3. * (p0.y() + p1.y() + p2.y());
      }
    } else {
      Q_ASSERT_X(false, "computeAreaCenter", "Unknown primitive");
    }
  }

  return s / tot;
}


static WGS84PointVector reduce(const VVec& vs) {
  const float eps = 1.e-8;
  WGS84PointVector ps;
  const int N = vs.size();
  for (int k = 0; k < N; k++) {
    const glm::vec2 v = vs[k];
    const glm::vec2 vm = vs[(k + N - 1) % N];
    const glm::vec2 vp = vs[(k + 1) % N];
    if (std::abs(v.x - vm.x) < eps && std::abs(v.x - vp.x) < eps) continue;
    if (std::abs(v.y - vm.y) < eps && std::abs(v.y - vp.y) < eps) continue;
    ps << WGS84Point::fromLL(v.y, v.x);
  }
  return ps;
}

using PointVector = QVector<QPointF>;
using PPolygon = QVector<PointVector>;

static float area(const PointVector& ps) {
  float sum = 0;
  const int n = ps.size();
  for (int k = 0; k < n; k++) {
    const QPointF p0 = ps[k];
    const QPointF p1 = ps[(k + 1) % n];
    sum += p0.x() * p1.y() - p0.y() * p1.x();
  }
  return .5 * sum;
}

static uint checkCoverage(const Region& cov,
                          const Region& nocov,
                          const WGS84PointVector& ps,
                          const GeoProjection* gp) {
  PPolygon covp;
  for (const WGS84PointVector& vs: cov) {
    PointVector ps;
    for (auto v: vs) {
      ps << gp->fromWGS84(v);
    }
    covp.append(ps);
  }
  PPolygon nocovp;
  for (const WGS84PointVector& vs: nocov) {
    PointVector ps;
    for (auto v: vs) {
      ps << gp->fromWGS84(v);
    }
    nocovp.append(ps);
  }
  PointVector box;
  for (auto p: ps) {
    box << gp->fromWGS84(p);
  }

  const float A = area(box);

  float totcov = 0;
  for (const PointVector& ps: covp) {
    totcov += std::abs(area(ps) / A);
  }

  float totnocov = 0;
  for (const PointVector& ps: nocovp) {
    totnocov += std::abs(area(ps) / A);
  }

  uint ret = 0;
  // .5: assuming it's a bug
  if (!cov.isEmpty() && totcov < .8 && totcov != .5) ret += UseCoverage;
  if (totnocov > .2) ret += UseNoCoverage;

  return ret;
}

