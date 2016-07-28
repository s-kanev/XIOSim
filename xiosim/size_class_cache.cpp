#include <iostream>
#include "size_class_cache.h"

std::ostream& operator<<(std::ostream& os, const index_range_t& range) {
    os << "[" << range.get_begin() << ", " << range.get_end() << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, const size_class_pair_t& pair) {
    os << "(class: " << pair.get_size_class() << ", size: " << pair.get_size() << ")";
    return os;
}
