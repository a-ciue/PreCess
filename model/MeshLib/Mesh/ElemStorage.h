#include "plf_colony.h"

namespace MeshLib {

template <typename CElement>
class ElemStorage : public plf::colony<CElement> {
public:
    using Base = plf::colony<CElement>;

    ElemStorage() = default;
    ~ElemStorage() = default;
    CElement* allocate() { return &*(Base::emplace()); }
    void deallocate(CElement* e)
    {
        if (!e)
            return;
	    Base::erase(Base::get_iterator(e));
    }
};
}