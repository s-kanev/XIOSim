#ifndef __CORE_SET__
#define __CORE_SET__

#include "assert.h"
#include <set>

class CoreSet : public std::set<int>
{
public:
    int getPrevCore(int coreId) {
        auto curr = this->find(coreId);
        assert(curr != this->end());
        curr--;
        // This was the first element, start over
        if (curr == this->end())
            curr--;
        return *curr;
    }

    int getNextCore(int coreId) {
        auto curr = this->find(coreId);
        assert(curr != this->end());
        curr++;
        // This was the last element, start over
        if (curr == this->end())
            curr = this->begin();
        return *curr;
    }
};

#endif
