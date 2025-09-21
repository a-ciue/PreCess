#ifndef _TOOL_MESH_H_
#define _TOOL_MESH_H_

#include <map>
#include <vector>

#include "MeshLib/Geometry/Point.h"
#include "MeshLib/Geometry/Point2.h"
#include "MeshLib/Mesh/BaseMesh.h"
#include "MeshLib/Mesh/Edge.h"
#include "MeshLib/Mesh/Face.h"
#include "MeshLib/Mesh/HalfEdge.h"
#include "MeshLib/Mesh/Vertex.h"
#include "MeshLib/Mesh/boundary.h"
#include "MeshLib/Mesh/iterators.h"
#include "MeshLib/Parser/parser.h"

#include <unordered_map>

namespace MeshLib {
class CToolVertex : public CVertex {
public:
  CToolVertex() { m_fixed = false; };
  ~CToolVertex(){};
  void _to_string();
  void _from_string();

  bool &fixed() { return m_fixed; };
  bool &is_edge() { return edge; };
  int &father() { return m_father; };
  CPoint &rgb() { return m_rgb; };
  CPoint &normal() { return m_normal; };

protected:
  bool m_fixed;
  bool edge;
  int m_father;
  CPoint m_rgb;
  CPoint m_normal;
};

inline void CToolVertex::_from_string() {
  CParser parser(m_string);

  for (std::list<CToken *>::iterator iter = parser.tokens().begin();
       iter != parser.tokens().end(); ++iter) {
    CToken *token = *iter;
    /*if (token->m_key == "uv")
    {
            double u, v;
            sscanf(token->m_value.c_str(), "(%lf %lf)", &u, &v);
            m_huv[0] = u;
            m_huv[1] = v;
    }*/
  }
}

inline void CToolVertex::_to_string() {
  std::string a;
  m_string = a;

  if (1) {
    CParser parser3(m_string);
    parser3._removeToken("rgb");
    parser3._toString(m_string);
    std::stringstream iss3;
    iss3 << "rgb=(" << m_rgb[0] << " " << m_rgb[1] << " " << m_rgb[2] << ")";
    if (m_string.length() > 0) {
      m_string += " ";
    }
    m_string += iss3.str();
  }
  if (1) {
      CParser parser3(m_string);
      parser3._removeToken("father");
      parser3._toString(m_string);
      std::stringstream iss3;
      iss3 << "father=(" << m_father << ")";
      if (m_string.length() > 0) {
          m_string += " ";
      }
      m_string += iss3.str();
  }
}

class CToolEdge : public CEdge {
public:
  CToolEdge(){};
  ~CToolEdge(){};
  void _to_string();
  void _from_string();
  void set_edge(double edge) { is_edge = edge; };
  double get_edge() { return is_edge; };

protected:
  double is_edge = false;
};

inline void CToolEdge::_to_string() {
  std::string a;
  m_string = a;

  /*	CParser parser(m_string);
          parser._removeToken("sharp");
          parser._toString(m_string);
          stringstream iss;
          iss << "sharp";
          if (m_string.length() > 0)
          {
                  m_string += " ";
          }
          if (m_sharp == true)
          {
                  m_string += iss.str();
          }*/
}

inline void CToolEdge::_from_string() {
  CParser parser(m_string);

  for (std::list<CToken *>::iterator iter = parser.tokens().begin();
       iter != parser.tokens().end(); ++iter) {
    CToken *token = *iter;
    /*if (token->m_key == "uv")
    {
    double u, v;
    sscanf(token->m_value.c_str(), "(%lf %lf)", &u, &v);
    m_huv[0] = u;
    m_huv[1] = v;
    }*/
  }
}

class CToolFace : public CFace {
public:
  CToolFace(){};
  ~CToolFace(){};
  void _to_string();
  void _from_string();
  int &get_g() { return m_g; }

private:
  int m_g;
};

inline void CToolFace::_to_string() {
    CParser parser3(m_string);
    parser3._removeToken("g");
    parser3._toString(m_string);
    std::stringstream iss3;
    iss3 << "g=(" << m_g << ")";
    if (m_string.length() > 0) {
        m_string += " ";
    }
    m_string += iss3.str();

}

inline void CToolFace::_from_string() {
  CParser parser(m_string);

  for (std::list<CToken*>::iterator iter = parser.tokens().begin();
		iter != parser.tokens().end(); ++iter) {
      CToken* token = *iter;
      if (token->m_key == "g") {
          int g {};
          
          std::stringstream ss(token->m_value);
          char _{};
          ss >> std::ws >> _ >> g;
          //sscanf(token->m_value.c_str(), "(%d)", &g);
          m_g = g;
      }
  }
}

class CToolHalfEdge : public CHalfEdge {
public:
  CToolHalfEdge(){};
  ~CToolHalfEdge(){};
  double &angle() { return m_angle; }
  bool is_in_same_face(CToolHalfEdge* he)
{
    CToolHalfEdge* cur_he = this;
    do {
        if (cur_he == he) {
            break;
        }
        cur_he = static_cast<CToolHalfEdge*>(cur_he->he_next());
    } while (cur_he != this);

    return cur_he == he;
}
  void _to_string();

protected:
  double m_angle;
};

inline void CToolHalfEdge::_to_string() {
  // std::string a;
  // m_string = a;
}

template <typename TV, typename TE, typename TF, typename TH>
class CToolMesh : public CBaseMesh<TV, TE, TF, TH> {
public:
  typedef TV V;
  using CVertex = V;
  typedef TE E;
  using CEdge = E;
  typedef TF F;
  using CFace = F;
  typedef TH H;
  using CHalfEdge = H;

  typedef CBoundary<V, E, F, H> CBoundary;
  typedef CLoop<V, E, F, H> CLoop;

  typedef MeshVertexIterator<V, E, F, H> MeshVertexIterator;
  typedef MeshEdgeIterator<V, E, F, H> MeshEdgeIterator;
  typedef VertexVertexIterator<V, E, F, H> VertexVertexIterator;
  typedef VertexEdgeIterator<V, E, F, H> VertexEdgeIterator;
  typedef MeshFaceIterator<V, E, F, H> MeshFaceIterator;
  typedef FaceVertexIterator<V, E, F, H> FaceVertexIterator;
  typedef VertexFaceIterator<V, E, F, H> VertexFaceIterator;
  typedef FaceHalfedgeIterator<V, E, F, H> FaceHalfedgeIterator;
  typedef VertexOutHalfedgeIterator<V, E, F, H> VertexOutHalfedgeIterator;
  typedef VertexInHalfedgeIterator<V, E, F, H> VertexInHalfedgeIterator;
  typedef FaceEdgeIterator<V, E, F, H> FaceEdgeIterator;
	
  std::unordered_map<int, F*>& map_face()
  {
      return this->m_map_face;
  }
  std::unordered_map<int, V*>& map_vert()
  {
      return this->m_map_vert;
  }
  void Patch_Write(int id, const std::string& path_prefix);
};


typedef CToolMesh<CToolVertex, CToolEdge, CToolFace, CToolHalfEdge> CTMesh;



template<typename V, typename E, typename F, typename H>
inline void CToolMesh<V, E, F, H>::Patch_Write(int id, const std::string &path_prefix)
{
	for (MeshVertexIterator viter(this); !viter.end(); viter++)
	{
		CVertex* v = *viter;
		v->father() = 0;
	}
	for (MeshFaceIterator fiter(this); !fiter.end(); fiter++)
	{
		CFace* f = *fiter;
		if(f->get_g() == id)
		{
			for (FaceVertexIterator fviter(f); !fviter.end(); fviter++)
			{
				CVertex* v = *fviter;
				v->father() = v->id();
			}
		}
	}
    std::unordered_map<int, int> father_to_id;

    // file writer
    std::string file_name = path_prefix + "_" + std::to_string(id) + ".m";
	std::ofstream out(file_name);

    int vid = 1;
    for (MeshVertexIterator viter(this); !viter.end(); viter++)
    {
		if ((*viter)->father() == 0) continue;
        CVertex* v = *viter;
        father_to_id[v->father()] = vid;
        out << "Vertex " << vid << " " << v->point()[0] << " " << v->point()[1] << " " << v->point()[2] <<"{ father = ( "<<v->father()<<" )}" << std::endl;
        vid++;
    }
	vid = 1;
	for (MeshFaceIterator fiter(this); !fiter.end(); fiter++)
	{
		if ((*fiter)->get_g() != id) continue;
		CFace* f = *fiter;
		if (f->get_g() == id)
		{
			out << "Face " << vid++;
			for (FaceVertexIterator fviter(f); !fviter.end(); fviter++)
			{
				CVertex* v = *fviter;
				out << " " << father_to_id[v->father()];
			}
			out << " {g=(" << f->get_g() << ")}"<<std::endl;
		}
	}
	out.close();
}

} // namespace MeshLib

#endif
