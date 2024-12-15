#ifndef _FACESPLITTER_H_
#define _FACESPLITTER_H_

#include <array>
#include <math.h>
#include <unordered_set>
#include <vector>
#include <utility>

#ifndef PI
#define PI 3.1415926535
#endif

namespace MeshLib {
using namespace std;

template <typename M>
class CFaceSplitter {
    using V = typename M::V;
    using H = typename M::H;
    using E = typename M::E;
    using F = typename M::F;

public:
    CFaceSplitter(M* pMesh);
    ~CFaceSplitter() {};

    /*
     * �������ڴ����㣬���ش����ĵ��ָ��
     */
    V* create_vertex();
    /*
     * �ָ���he���ڵ�edge��
     * �з�֮ǰhe��va��vb���зֺ�he��va��vc��he_nxt��vc��vb��ά��������ݽṹ������vc
     */
    V* split_edge(H* he);
    /*
     * ����һ���ߴ�v0��v1�����ش�v0��v1�İ�ߣ�ͬʱ��֤ά��������ݽṹ
     * �÷���1������������������з֣��ֳ������� 2���������������ڴ���һ��������������һ���㣬ֻ�轫�������Ӧ��he��nullptr
     */
    std::pair<H*, F*> create_edge(V* v0, H* he_inv0, V* v1, H* he_inv1);
    //! �������Ľ��㣬�ڰ������ָ����λ��ͬ���ĵ��������������з֡�
    //! ע�⣺����������ά��vertex�ĳ�Ա����
    //! \return ���ĵ��ָ��
    //! \param phes_in: ������飬ָ�����ָ����ͷָ��λ�á���߶�����ͬһ���ϣ����ָ��Ķ����ʾ�ָ��
    V* slice_face(vector<H*> phes_in);
    //! ɾ�������ıߣ������������һ��
    //! ����������Ҫ�ϲ�����󣬱���phe_del���ڵ���
    //! \return �ϲ������ָ��
    //! \param phe_del: ���ָ�룬ָ����ɾ���ı�
    F* merge_face(H* phe_del);

protected:
    M* m_pMesh;

    int m_maxVid {};
    int m_maxFid {};

    int get_max_vid();
    int get_max_fid();
};

template <typename M>
CFaceSplitter<M>::CFaceSplitter(M* pMesh)
    : m_pMesh(pMesh)
    , m_maxVid(get_max_vid())
    , m_maxFid(get_max_fid())
{
}

template <typename M>
typename M::V* CFaceSplitter<M>::create_vertex()
{
    return m_pMesh->createVertex(++m_maxVid);
}

template <typename M>
typename M::V* CFaceSplitter<M>::split_edge(H* he)
{
    array<V*, 3> vs { m_pMesh->halfedgeSource(he), m_pMesh->createVertex(++m_maxVid), m_pMesh->halfedgeTarget(he) };
    array<H*, 4> hes {
        he, // he
        m_pMesh->halfedgeSym(he), // he_sym
        new H, // he_next
        nullptr
    };
    hes[3] = hes[1] ? new H : nullptr; // he_sym_prev
    array<E*, 2> es { m_pMesh->halfedgeEdge(he), new E };

    // halfedge
    hes[2]->he_next() = hes[0]->he_next(); // hes[0] -> hes[2]
    hes[0]->he_next()->he_prev() = hes[2];
    hes[2]->he_prev() = hes[0];
    hes[0]->he_next() = hes[2];
    hes[2]->edge() = es[1];
    hes[2]->face() = m_pMesh->halfedgeFace(hes[0]);
    hes[0]->vertex() = vs[1];
    hes[2]->vertex() = vs[2];
    if (hes[1]) {
        hes[3]->he_prev() = hes[1]->he_prev(); // hes[3] -> hes[1]
        hes[1]->he_prev()->he_next() = hes[3];
        hes[1]->he_prev() = hes[3];
        hes[3]->he_next() = hes[1];

        hes[3]->edge() = es[1];
        hes[3]->face() = m_pMesh->halfedgeFace(hes[1]);
        hes[1]->vertex() = vs[0];
        hes[3]->vertex() = vs[1];
    }
    // edge
    es[1]->halfedge(0) = hes[2];
    es[1]->halfedge(1) = hes[3];

    V* lv = vs[0]->id() < vs[2]->id() ? vs[0] : vs[2];
    lv->edges().remove(es[0]);

    lv = vs[0]->id() < vs[1]->id() ? vs[0] : vs[1];
    lv->edges().push_back(es[0]);
    lv = vs[1]->id() < vs[2]->id() ? vs[1] : vs[2];
    lv->edges().push_back(es[1]);
    // vertex
    vs[1]->boundary() = vs[0]->boundary() && vs[2]->boundary();
    vs[1]->halfedge() = hes[0];
    vs[1]->halfedge() = m_pMesh->vertexMostCcwInHalfEdge(vs[1]);
    vs[2]->halfedge() = hes[2];
    vs[2]->halfedge() = m_pMesh->vertexMostCcwInHalfEdge(vs[2]);
    // mesh
    m_pMesh->edges().push_back(es[1]);

    return vs[1];
}

template <typename M>
std::pair<typename M::H*, typename M::F*> CFaceSplitter<M>::create_edge(V* v0, H* he_inv0, V* v1, H* he_inv1)
{
    assert(he_inv0 || he_inv1);
    assert(!he_inv0 || !he_inv1 || he_inv0->is_in_same_face(he_inv1));

    E* e = m_pMesh->createEdge(v0, v1);
    array<F*, 2> fs {};
    array<H*, 2> e_hes {
        // e_hes[i]���v[i]
        new H, // v0->v1
        new H // v1->v0
    };

    array<array<H*, 2>, 2> v_adjhe {};
    for (int i = 0; i < 2; i++) {
        vector<H*> hes_in { he_inv0, he_inv1 };
        v_adjhe[i][0] = hes_in[i] == nullptr ? e_hes[(i + 1) % 2] : hes_in[i]; // vi_in
        v_adjhe[i][1] = hes_in[i] == nullptr ? e_hes[i] : m_pMesh->halfedgeNext(hes_in[i]); // vi_out
    }

    F* f {};
    if (he_inv0) {
        f = m_pMesh->halfedgeFace(he_inv0);
    } else {
        f = m_pMesh->halfedgeFace(he_inv1);
    }
    assert(f);

    // halfedge
    e_hes[0]->he_prev() = v_adjhe[0][0]; // v0_in
    e_hes[0]->he_next() = v_adjhe[1][1]; // v1_out
    e_hes[1]->he_prev() = v_adjhe[1][0]; // v1_in
    e_hes[1]->he_next() = v_adjhe[0][1]; // v0_out
    for (int i = 0; i < 2; i++) {
        v_adjhe[i][0]->he_next() = e_hes[i]; // vi_in -> next = hei
        v_adjhe[(i + 1) % 2][1]->he_prev() = e_hes[i]; // vi+1_out -> prev = hei
    }

    e_hes[0]->vertex() = v1;
    e_hes[1]->vertex() = v0;

    e_hes[0]->edge() = e_hes[1]->edge() = e;

    // face
    F* newFace {};
    for (int i = 0; i < 2; i++) {
        if (e_hes[i]->is_in_same_face(m_pMesh->faceHalfedge(f))) {
            fs[i] = f;
        } else {
            fs[i] = new F();
            newFace = fs[i];
            fs[i]->id() = ++m_maxFid;
            fs[i]->halfedge() = e_hes[i];
            m_pMesh->faces().push_back(fs[i]);
            m_pMesh->map_face().emplace(fs[i]->id(), fs[i]);

            H* cur_he = e_hes[i];
            do {
                cur_he->face() = fs[i];
                cur_he = m_pMesh->halfedgeNext(cur_he);
            } while (cur_he != e_hes[i]);
        }

        e_hes[i]->face() = fs[i];
    }
    // edge
    e->halfedge(0) = e_hes[0];
    e->halfedge(1) = e_hes[1];

    return { e_hes[0], newFace };
}

template <typename M>
typename M::V* CFaceSplitter<M>::slice_face(vector<H*> phes_in)
{
    // ����phes_in��˳��ɸѡͬ��İ�ߣ�����˳ʱ������
    unordered_set<H*> phes_next_set(phes_in.begin(), phes_in.end()); // ��һ����ߵ�������
    assert(phes_next_set.size() == phes_in.size()); // ����Ҳ��Ҫ�����ظ����

    // ��begin��ʼ����˳ʱ�뷽�����setԪ�أ����뵽vector��
    int n = phes_next_set.size();
    H* phe_cur = *(phes_next_set.begin());
    for (int i = 0; i < n; i++) {
        phes_in[i] = phe_cur;
        phes_next_set.erase(phe_cur);

        H* phe_next = phe_cur;
        do {
            phe_next = m_pMesh->halfedgePrev(phe_next);
        } while (phe_cur != phe_next && !phes_next_set.count(phe_next));

        // ����ǹ��������
        assert(phe_cur != phe_next || !phes_next_set.size());
        if (phe_cur == phe_next) {
            break;
        }

        phe_cur = phe_next;
    }
    assert(phes_next_set.size() == 0); // ������ͬ��İ�ߴ���ú���

    n -= phes_next_set.size();
    phes_in.resize(n);

    // ���е㡢���ϵĵ���е㴴������
    V* pv_centroid = create_vertex();
    pv_centroid->boundary() = false;
    H* phe_in_centroid = nullptr;
    for (H* phe_in : phes_in) {
        phe_in_centroid = create_edge(m_pMesh->halfedgeTarget(phe_in), phe_in, pv_centroid, phe_in_centroid);
    }
    pv_centroid->halfedge() = phe_in_centroid;

    return pv_centroid;
}

template <typename M>
typename M::F* CFaceSplitter<M>::merge_face(H* phe_del)
{
    // TODO
    // �ϲ�����
    // ��ȡ������ɾ���İ��
    H* to_be_deleted_hes[2];
    to_be_deleted_hes[0] = phe_del;
    to_be_deleted_hes[1] = m_pMesh->halfedgeSym(to_be_deleted_hes[0]);

    // ��ȡ������ɾ���ı�
    E* to_be_deleted_e = m_pMesh->halfedgeEdge(to_be_deleted_hes[0]);

    // �޸�����
    // ���ǰ������
    H* h_left_1 = m_pMesh->halfedgePrev(to_be_deleted_hes[1]);
    H* h_right_1 = m_pMesh->halfedgeNext(to_be_deleted_hes[0]);

    // �޸İ������
    h_left_1->he_next() = h_right_1;
    h_right_1->he_prev() = h_left_1;
    h_right_1->source() = h_left_1->target();

    H* h_left_2 = m_pMesh->halfedgeNext(to_be_deleted_hes[1]);
    H* h_right_2 = m_pMesh->halfedgePrev(to_be_deleted_hes[0]);

    h_left_2->he_prev() = h_right_2;
    h_right_2->he_next() = h_left_2;

    h_left_2->source() = h_right_2->target();

    // ������߹�����
    F* f = m_pMesh->halfedgeFace(h_right_2);
    f->halfedge() = h_right_2;
    h_left_1->face() = f;
    h_left_2->face() = f;

    assert(h_left_1->he_prev() == h_left_2);

    // ����vertex�İ�ߣ�ָ��һ����ȷ��ߣ�������ֿ�ָ��
    V* v_left_1 = m_pMesh->halfedgeTarget(h_left_1);

    V* v_left_2 = m_pMesh->halfedgeSource(h_left_2);

    v_left_1->halfedge() = h_left_1;
    v_left_2->halfedge() = h_right_2;

    // ɾ���桢��ߡ���
    F* f_left = m_pMesh->halfedgeFace(to_be_deleted_hes[1]);

    typename std::map<int, F*>::iterator fiter = m_pMesh->map_face().find(f_left->id());
    if (fiter != m_pMesh->map_face().end()) {
        m_pMesh->map_face().erase(fiter);
    }

    m_pMesh->faces().remove(f_left);

    // �˴�ɾȥvertex��>edges()�е�edge
    v_left_1->edges().remove(to_be_deleted_e);
    v_left_2->edges().remove(to_be_deleted_e);

    m_pMesh->edges().remove(to_be_deleted_e);

    delete to_be_deleted_e;
    to_be_deleted_e = NULL;

    for (int i = 0; i < 2; i++) {
        delete to_be_deleted_hes[i];
    }

    delete f_left;
    f_left = NULL;

    return f;
}

template <typename M>
int CFaceSplitter<M>::get_max_vid()
{
    int maxVid { 0 };
    for (typename M::MeshVertexIterator vi(m_pMesh); !vi.end(); vi++) {
        V* pCurV = vi.value();
        maxVid = max(maxVid, pCurV->id());
    }
    return maxVid;
}
template <typename M>
int CFaceSplitter<M>::get_max_fid()
{
    int maxFid { 0 };
    for (typename M::MeshFaceIterator fi(m_pMesh); !fi.end(); fi++) {
        F* pCurF = fi.value();
        maxFid = max(maxFid, pCurF->id());
    }
    return maxFid;
}

}
#endif