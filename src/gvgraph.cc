#include "hpp/plot/gvgraph.h"

#include <QStringList>

namespace hpp {
  namespace plot {

    namespace {
      /// The agopen method for opening a graph
      static inline Agraph_t* _agopen(QString name, int kind)
      {
        return agopen(const_cast<char *>(qPrintable(name)), kind);
      }

      /// Add an alternative value parameter to the method for getting an object's attribute
      static inline QString _agget(void *object, QString attr, QString alt=QString())
      {
        QString str=agget(object, const_cast<char *>(qPrintable(attr)));

        if(str==QString())
          return alt;
        else
          return str;
      }

      /// Directly use agsafeset which always works, contrarily to agset
      static inline int _agset(void *object, QString attr, QString value)
      {
        return agsafeset(object, const_cast<char *>(qPrintable(attr)),
                         const_cast<char *>(qPrintable(value)),
                         const_cast<char *>(qPrintable(value)));
      }

      static inline Agsym_t* _agnodeattr(Agraph_t *object, QString attr, QString value)
      {
        return agnodeattr(object, const_cast<char *>(qPrintable(attr)),
                          const_cast<char *>(qPrintable(value)));
      }

      static inline Agsym_t* _agedgeattr(Agraph_t *object, QString attr, QString value)
      {
        return agedgeattr(object, const_cast<char *>(qPrintable(attr)),
                          const_cast<char *>(qPrintable(value)));
      }

      static inline Agnode_t* _agnode(Agraph_t *object, QString name)
      {
        return agnode(object, const_cast<char *>(qPrintable(name)));
      }

      static inline int _gvLayout (GVC_t *gvc, graph_t *g, QString engine)
      {
        return gvLayout(gvc, g, const_cast<char *>(qPrintable(engine)));
      }
    }

    /*! Dot uses a 72 DPI value for converting it's position coordinates from points to pixels
    while we display at 96 DPI on most operating systems. */
    const qreal GVGraph::DotDefaultDPI=72.0;
    const QLocale GVGraph::interpertDouble = QLocale::German;

    GVGraph::GVGraph(QString name, QFont font, qreal node_size) :
      _context(gvContext()),
      _graph(_agopen(name, AGDIGRAPHSTRICT)) // Strict directed graph, see libgraph doc
    {
      //Set graph attributes
      _agset(_graph, "overlap", "prism");
      _agset(_graph, "splines", "true");
      _agset(_graph, "pad", "0,2");
      _agset(_graph, "dpi", "96,0");
      _agset(_graph, "nodesep", "0,4");

      //Set default attributes for the future nodes
      _agnodeattr(_graph, "fixedsize", "true");
      _agnodeattr(_graph, "label", "");
      _agnodeattr(_graph, "regular", "true");

      //Divide the wanted width by the DPI to get the value in points
      QString nodePtsWidth = QString("%1").arg(
            node_size/
            interpertDouble.toDouble(_agget(_graph, "dpi", "96,0")));

      //GV uses , instead of . for the separator in floats
      _agnodeattr(_graph, "width", nodePtsWidth.replace('.', ","));

      setFont(font);
    }

    GVGraph::~GVGraph()
    {
      gvFreeLayout(_context, _graph);
      agclose(_graph);
      gvFreeContext(_context);
    }

    void GVGraph::addNode(const QString& name)
    {
      if(_nodes.contains(name))
        removeNode(name);

      _nodes.insert(name, _agnode(_graph, name));
    }

    void GVGraph::addNodes(const QStringList& names)
    {
      for(int i=0; i<names.size(); ++i)
        addNode(names.at(i));
    }

    void GVGraph::removeNode(const QString& name)
    {
      if(_nodes.contains(name))
        {
          agdelete(_graph, _nodes[name]);
          _nodes.remove(name);

          QList<QPair<QString, QString> >keys=_edges.keys();
          for(int i=0; i<keys.size(); ++i)
            if(keys.at(i).first==name || keys.at(i).second==name)
              removeEdge(keys.at(i));
        }
    }

    void GVGraph::clearNodes()
    {
      QList<QString> keys=_nodes.keys();

      for(int i=0; i<keys.size(); ++i)
        removeNode(keys.at(i));
    }

    void GVGraph::setRootNode(const QString& name)
    {
      if(_nodes.contains(name))
        _agset(_graph, "root", name);
    }

    void GVGraph::addEdge(const QString &source, const QString &target)
    {
      if(_nodes.contains(source) && _nodes.contains(target))
        {
          QPair<QString, QString> key(source, target);
          if(!_edges.contains(key))
            _edges.insert(key, agedge(_graph, _nodes[source], _nodes[target]));
        }
    }

    void GVGraph::removeEdge(const QString &source, const QString &target)
    {
      removeEdge(QPair<QString, QString>(source, target));
    }

    void GVGraph::removeEdge(const QPair<QString, QString>& key)
    {
      if(_edges.contains(key))
        {
          agdelete(_graph, _edges[key]);
          _edges.remove(key);
        }
    }

    void GVGraph::setFont(QFont font)
    {
      _font=font;

      _agset(_graph, "fontname", font.family());
      _agset(_graph, "fontsize", QString("%1").arg(font.pointSizeF()));

      _agnodeattr(_graph, "fontname", font.family());
      _agnodeattr(_graph, "fontsize", QString("%1").arg(font.pointSizeF()));

      _agedgeattr(_graph, "fontname", font.family());
      _agedgeattr(_graph, "fontsize", QString("%1").arg(font.pointSizeF()));
    }

    void GVGraph::applyLayout()
    {
      gvFreeLayout(_context, _graph);
      _gvLayout(_context, _graph, "dot");
    }

    QRectF GVGraph::boundingRect() const
    {
      qreal dpi = interpertDouble.toDouble(_agget(_graph, "dpi", "96,0"));
      return QRectF(_graph->u.bb.LL.x*(dpi/DotDefaultDPI), _graph->u.bb.LL.y*(dpi/DotDefaultDPI),
                    _graph->u.bb.UR.x*(dpi/DotDefaultDPI), _graph->u.bb.UR.y*(dpi/DotDefaultDPI));
    }

    QList<GVNode> GVGraph::nodes() const
    {
      QList<GVNode> list;
      qreal dpi = interpertDouble.toDouble(_agget(_graph, "dpi", "96,0"));

      for(QMap<QString, Agnode_t*>::const_iterator it=_nodes.begin(); it!=_nodes.end();++it)
        {
          Agnode_t *node=it.value();
          GVNode object;

          //Set the name of the node
          object.name=node->name;

          //Fetch the X coordinate, apply the DPI conversion rate (actual DPI / 72, used by dot)
          qreal x=node->u.coord.x*(dpi/DotDefaultDPI);

          //Translate the Y coordinate from bottom-left to top-left corner
          qreal y=(_graph->u.bb.UR.y - node->u.coord.y)*(dpi/DotDefaultDPI);
          object.centerPos=QPoint(x, y);

          //Transform the width and height from inches to pixels
          object.height=node->u.height*dpi;
          object.width=node->u.width*dpi;

          list << object;
        }

      return list;
    }

    QList<GVEdge> GVGraph::edges() const
    {
      QList<GVEdge> list;
      qreal dpi=_agget(_graph, "dpi", "96,0").toDouble();

      for(QMap<QPair<QString, QString>, Agedge_t*>::const_iterator it=_edges.begin();
          it!=_edges.end();++it)
        {
          Agedge_t *edge=it.value();
          GVEdge object;

          //Fill the source and target node names
          object.source=edge->tail->name;
          object.target=edge->head->name;

          //Calculate the path from the spline (only one spline, as the graph is strict. If it
          //wasn't, we would have to iterate over the first list too)
          //Calculate the path from the spline (only one as the graph is strict)
          if((edge->u.spl->list!=0) && (edge->u.spl->list->size%3 == 1))
            {
              //If there is a starting point, draw a line from it to the first curve point
              if(edge->u.spl->list->sflag)
                {
                  object.path.moveTo(edge->u.spl->list->sp.x*(dpi/DotDefaultDPI),
                                     (_graph->u.bb.UR.y - edge->u.spl->list->sp.y)*(dpi/DotDefaultDPI));
                  object.path.lineTo(edge->u.spl->list->list[0].x*(dpi/DotDefaultDPI),
                                     (_graph->u.bb.UR.y - edge->u.spl->list->list[0].y)*(dpi/DotDefaultDPI));
                }
              else
                object.path.moveTo(edge->u.spl->list->list[0].x*(dpi/DotDefaultDPI),
                                   (_graph->u.bb.UR.y - edge->u.spl->list->list[0].y)*(dpi/DotDefaultDPI));

              //Loop over the curve points
              for(int i=1; i<edge->u.spl->list->size; i+=3)
                object.path.cubicTo(edge->u.spl->list->list[i].x*(dpi/DotDefaultDPI),
                                    (_graph->u.bb.UR.y - edge->u.spl->list->list[i].y)*(dpi/DotDefaultDPI),
                                    edge->u.spl->list->list[i+1].x*(dpi/DotDefaultDPI),
                                    (_graph->u.bb.UR.y - edge->u.spl->list->list[i+1].y)*(dpi/DotDefaultDPI),
                                    edge->u.spl->list->list[i+2].x*(dpi/DotDefaultDPI),
                                    (_graph->u.bb.UR.y - edge->u.spl->list->list[i+2].y)*(dpi/DotDefaultDPI));

              //If there is an ending point, draw a line to it
              if(edge->u.spl->list->eflag)
                object.path.lineTo(edge->u.spl->list->ep.x*(dpi/DotDefaultDPI),
                                   (_graph->u.bb.UR.y - edge->u.spl->list->ep.y)*(dpi/DotDefaultDPI));
            }

          list << object;
        }

      return list;
    }
  }
}
