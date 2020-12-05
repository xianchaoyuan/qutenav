#pragma once

#include <QVariant>
#include <QVector2D>
#include <QOpenGLFunctions>
#include <QColor>
#include <QRectF>
#include "types.h"

namespace S57 {

class Attribute {
public:

  enum class Type: uint8_t {
    Integer,
    IntegerList,
    Real,
    None, // OgrAttr_t has RealLst here
    String,
    Any // Not in OgrAttr_t
  };

  Attribute(int v)
    : m_type(Type::Integer)
    , m_value(QVariant::fromValue(v)) {}

  Attribute(double v)
    : m_type(Type::Real)
    , m_value(QVariant::fromValue(v)) {}

  Attribute(const QVector<int>& v)
    : m_type(Type::IntegerList)
    , m_value(QVariant::fromValue(v)) {}

  Attribute(const QString& v)
    : m_type(Type::String)
    , m_value(QVariant::fromValue(v)) {}

  Attribute(Type t)
    : m_type(t)
    , m_value() {}

  Attribute() = default;

  Type type() const {return m_type;}
  const QVariant& value() const {return m_value;}

  bool matches(const Attribute& constraint) const;

private:
  Type m_type;
  QVariant m_value;
};

using AttributeMap = QMap<quint32, Attribute>;
using AttributeIterator = QMap<quint32, Attribute>::const_iterator;

struct ElementData {
  GLenum mode;
  // offset to vertex/element buffer, depending whether
  // vertices are indexed or not
  uintptr_t offset;
  size_t count;
};

using ElementDataVector = QVector<ElementData>;

namespace Geometry {

enum class Type: char {
  Point = 'P',
  Line = 'L',
  Area = 'A',
  Meta = 'M', // not really a geometry type
};

class Base {
public:

  Type type() const {return m_type;}
  const QPointF& center() const {return m_center;}

  virtual ~Base() = default;

protected:

  Base(Type t, const QPointF& c): m_type(t), m_center(c) {}
  Base(Type t): m_type(t) {}

  Type m_type;
  QPointF m_center;

};


class Meta: public Base {
public:
  Meta(): Base(Type::Meta, QPointF()) {}
};

using PointVector = QVector<double>;

class Point: public Base {
public:
  Point(const QPointF& p)
    : Base(Type::Point, p) {
    m_points.append(p.x());
    m_points.append(p.y());
    m_points.append(0.);
  }

  Point(const PointVector& ps)
    : Base(Type::Point)
    , m_points(ps) {
    QPointF s(0, 0);
    const int n = m_points.size() / 3;
    for (int i = 0; i < n; i++) {
      s.rx() += m_points[3 * i + 0];
      s.ry() += m_points[3 * i + 1];
    }
    m_center = s / n;
  }

  const PointVector& points() const {return m_points;}

private:

  PointVector m_points;
};


class Line: public Base {
public:
  Line(const ElementDataVector& elems, const QPointF& c, GLsizei vo)
    : Base(Type::Line, c)
    , m_lineElements(elems)
    , m_vertexOffset(vo) {}

  const ElementDataVector& lineElements() const {return m_lineElements;}
  GLsizei vertexOffset() const {return m_vertexOffset;}

private:

  ElementDataVector m_lineElements;
  GLsizei m_vertexOffset;

};

class Area: public Line {
public:
  Area(const ElementDataVector& lelems, const QPointF& c, const ElementDataVector& telems, GLsizei vo, bool indexed)
    : Line(lelems, c, vo)
    , m_triangleElements(telems)
    , m_indexed(indexed)
  {
    m_type = Type::Area;
  }

  const ElementDataVector& triangleElements() const {return m_triangleElements;}
  bool indexed() const {return m_indexed;}

private:

  ElementDataVector m_triangleElements;
  bool m_indexed;

};

} // namespace Geometry



using VertexVector = QVector<GLfloat>;

class PaintData {
public:

  enum class Type {
    CategoryOverride, // For a CS procedure to change the display category
    TriangleElements,
    TriangleArrays,
    SolidLineElements,
    DashedLineElements,
    SolidLineArrays,
    DashedLineArrays,
    SolidLineLocal,
    DashedLineLocal,
    TextElements,
    RasterSymbolElements,
  };

  virtual void setUniforms() const = 0;
  virtual void setVertexOffset() const = 0;
  Type type() const {return m_type;}

  virtual ~PaintData() = default;

protected:

  PaintData(Type t);

  Type m_type;

};

class TriangleData: public PaintData {
public:
  void setUniforms() const override;
  void setVertexOffset() const override;

  const ElementDataVector& elements() const {return m_elements;}

protected:

  TriangleData(Type t, const ElementDataVector& elems, GLsizei offset, const QColor& c);

  ElementDataVector m_elements;
  GLsizei m_vertexOffset;
  QColor m_color;

};

class TriangleArrayData: public TriangleData {
public:
  TriangleArrayData(const ElementDataVector& elem, GLsizei offset, const QColor& c);
};

class TriangleElemData: public TriangleData {
public:
  TriangleElemData(const ElementDataVector& elem, GLsizei offset, const QColor& c);
};

class LineData: public PaintData {
public:
  const ElementDataVector& elements() const {return m_elements;}

protected:

  LineData(Type t, const ElementDataVector& elems);

  ElementDataVector m_elements;
};

class SolidLineData: public LineData {
public:
  void setUniforms() const override;
  void setVertexOffset() const override;


protected:

  SolidLineData(Type t, const ElementDataVector& elems, GLsizei offset, const QColor& c, GLfloat width);

  GLsizei m_vertexOffset;
  QColor m_color;
  GLfloat m_lineWidth;

};

class DashedLineData: public LineData {
public:
  void setUniforms() const override;
  void setVertexOffset() const override;


protected:

  DashedLineData(Type t, const ElementDataVector& elems, GLsizei offset, const QColor& c, GLfloat width, uint patt);

  GLsizei m_vertexOffset;
  QColor m_color;
  GLfloat m_lineWidth;
  GLuint m_pattern;
};

class SolidLineElemData: public SolidLineData {
public:
  SolidLineElemData(const ElementDataVector& elem, GLsizei offset, const QColor& c, GLfloat width);
};


class SolidLineArrayData: public SolidLineData {
public:
  SolidLineArrayData(const ElementDataVector& elem, GLsizei offset, const QColor& c, GLfloat width);
};

class DashedLineElemData: public DashedLineData {
public:
  DashedLineElemData(const ElementDataVector& elem, GLsizei offset, const QColor& c, GLfloat width, uint pattern);
};

class DashedLineArrayData: public DashedLineData {
public:
  DashedLineArrayData(const ElementDataVector& elem, GLsizei offset, const QColor& c, GLfloat width, uint pattern);
};

class SolidLineLocalData: public SolidLineData {
public:
  SolidLineLocalData(const VertexVector& vertices, const ElementDataVector& elem, const QColor& c, GLfloat width);
  SolidLineArrayData* createArrayData(GLsizei offset) const;
  const VertexVector& vertices() const {return m_vertices;}
private:
  VertexVector m_vertices;
};

class DashedLineLocalData: public DashedLineData {
public:
  DashedLineLocalData(const VertexVector& vertices, const ElementDataVector& elem, const QColor& c, GLfloat width, uint pattern);
  DashedLineArrayData* createArrayData(GLsizei offset) const;
  const VertexVector& vertices() const {return m_vertices;}
private:
  VertexVector m_vertices;
};


class TextElemData: public PaintData {
public:
  void setUniforms() const override;
  void setVertexOffset() const override;

  TextElemData(const QPointF& pivot,
               const QPointF& pivotOffset,
               const QPointF& bboxOffset,
               float boxScale,
               GLsizei vertexOffset,
               const ElementData& elems,
               const QColor& c);

  const ElementData& elements() const {return m_elements;}

protected:


  ElementData m_elements;
  GLsizei m_vertexOffset;
  QColor m_color;
  GLfloat m_scaleMM;
  QPointF m_pivot;
  QPointF m_shiftMM;

};


class RasterSymbolElemData: public PaintData {
public:
  void setUniforms() const override;
  void setVertexOffset() const override;

  RasterSymbolElemData(const QPointF& pivot,
                       const QPoint& pivotOffset,
                       const ElementData& elems,
                       quint32 index,
                       S52::SymbolType type);

  const ElementData& elements() const {return m_elements;}

protected:

  ElementData m_elements;
  QPointF m_pivot;
  QPoint m_pivotOffset;
  quint32 m_index;
  S52::SymbolType m_type;
};


using PaintDataMap = QMultiMap<PaintData::Type, PaintData*>;
using PaintIterator = QMultiMap<PaintData::Type, PaintData*>::const_iterator;
using PaintMutIterator = QMultiMap<PaintData::Type, PaintData*>::iterator;

class ObjectBuilder;

class Object {

  friend class ObjectBuilder;

public:

  Object(quint32 fid, quint32 ftype)
    : m_feature_id(fid)
    , m_feature_type_code(ftype)
    , m_geometry(nullptr) {}

  ~Object();

  QString name() const;

  quint32 classCode() const {return m_feature_type_code;}
  quint32 identifier() const {return m_feature_id;}
  const Geometry::Base* geometry() const {return m_geometry;}
  const AttributeMap& attributes() const {return m_attributes;}
  QVariant attributeValue(quint32 attr) const;
  const QRectF& boundingBox() const {return m_bbox;}

  bool canPaint(const QRectF& viewArea, quint32 scale, const QDate& today) const;
  const QVector<Object*>& others() const {return m_others;}

private:

  // shortcuts to find SCAMIN and date attribute values. From s57attributes.csv.
  static const int scaminIndex = 133;
  static const int datstaIndex = 86;
  static const int datendIndex = 85;
  static const int perstaIndex = 119;
  static const int perendIndex = 118;

  const quint32 m_feature_id;
  const quint32 m_feature_type_code;
  AttributeMap m_attributes;
  Geometry::Base* m_geometry;
  QRectF m_bbox;
  QVector<Object*> m_others;

};

using ObjectVector = QVector<Object*>;

}

