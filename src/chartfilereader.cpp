#include "chartfilereader.h"
#include "osencreader.h"
#include "cm93reader.h"

QVector<ChartFileReader*> ChartFileReader::readers() {
  static QVector<ChartFileReader*> all{new OsencReader(), new CM93Reader()};
  return all;
}

ChartFileReader* ChartFileReader::reader(const QString& name) {
  for (ChartFileReader* c: readers()) {
    if (c->name() == name) return c;
  }
  return nullptr;
}

QStringList ChartFileReader::names() {
  QStringList ns;
  for (ChartFileReader* c: readers()) {
    ns << c->name();
  }
  return ns;
}

QRectF ChartFileReader::computeBBox(const S57::ElementDataVector &elems,
                                    const GL::VertexVector& vertices,
                                    const GL::IndexVector& indices) {

  QPointF ur(-1.e15, -1.e15);
  QPointF ll(1.e15, 1.e15);

  for (const S57::ElementData& elem: elems) {
    const int first = elem.offset / sizeof(GLuint);
    for (uint i = 0; i < elem.count; i++) {
      const int index = indices[first + i];
      QPointF q(vertices[2 * index], vertices[2 * index + 1]);
      ur.setX(qMax(ur.x(), q.x()));
      ur.setY(qMax(ur.y(), q.y()));
      ll.setX(qMin(ll.x(), q.x()));
      ll.setY(qMin(ll.y(), q.y()));
    }
  }
  return QRectF(ll, ur); // inverted y-axis
}

QPointF ChartFileReader::computeLineCenter(const S57::ElementDataVector &elems,
                                           const GL::VertexVector& vertices,
                                           const GL::IndexVector& indices) {
  int first = elems[0].offset / sizeof(GLuint) + 1; // account adjacency
  int last = first + elems[0].count - 3; // account adjacency
  if (elems.size() > 1 || indices[first] == indices[last]) {
    // several lines or closed loops: compute center of gravity
    QPointF s(0, 0);
    int n = 0;
    for (auto& elem: elems) {
      first = elem.offset / sizeof(GLuint) + 1; // account adjacency
      last = first + elem.count - 3; // account adjacency
      for (int i = first; i <= last; i++) {
        const int index = indices[i];
        s.rx() += vertices[2 * index + 0];
        s.ry() += vertices[2 * index + 1];
      }
      n += elem.count - 2;
    }
    return s / n;
  }
  // single open line: compute mid point of running length
  QVector<float> lengths;
  float len = 0;
  for (int i = first; i < last; i++) {
    const int i1 = indices[i + 1];
    const int i0 = indices[i];
    const QPointF d(vertices[2 * i1] - vertices[2 * i0], vertices[2 * i1 + 1] - vertices[2 * i0 + 1]);
    lengths.append(sqrt(QPointF::dotProduct(d, d)));
    len += lengths.last();
  }
  const float halfLen = len / 2;
  len = 0;
  int i = 0;
  while (i < lengths.size() && len < halfLen) {
    len += lengths[i];
    i++;
  }
  const int i1 = indices[first + i];
  const int i0 = indices[first + i - 1];
  const QPointF p1(vertices[2 * i1], vertices[2 * i1 + 1]);
  const QPointF p0(vertices[2 * i0], vertices[2 * i0 + 1]);
  return p0 + (len - halfLen) * (p1 - p0);

}


