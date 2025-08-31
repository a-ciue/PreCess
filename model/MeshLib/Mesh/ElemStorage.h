#include "plf_colony.h"

namespace MeshLib {

template <typename CElement>
class ElemStorage : public plf::colony<CElement> {
public:
    ElemStorage() = default;
    ~ElemStorage() = default;
    CElement* allocate() { return &*(emplace()); }
    void deallocate(CElement* e)
    {
        if (!e)
            return;
	    erase(get_iterator(e));
    }
};
}