#ifndef __EXPRESSION_H__
#define __EXPRESSION_H__

#include <cmath>
#include <memory>

namespace xiosim {
namespace stats {

/* All stats get returned as floats. */
typedef float Result;

// C++ does not have a standard template operator for exponentiation.
template <typename T>
class power {
  public:
    Result operator()(T base, T exp) {
        return pow(base, exp);
    }
};

/* Base interface of an expression tree node.
 *
 * An equation is broken down into a binary tree of Expression nodes. Calling
 * evaluate() on any one of these will recursively evaluate all the nodes below
 * it. Leaf nodes are Statistic objects or wrappers around constants,
 * and non-leaf nodes represent operations to be performed on
 * one or two leaf nodes.
 */
class Expression {
  public:
    virtual ~Expression() {}
    virtual Result evaluate() const = 0;
    /* Returns a deep copy of the current expression and all its subexpressions. */
    virtual std::unique_ptr<Expression> deep_copy() const = 0;
};

}  // namespace stats
}  // namespace xiosim

#endif /* __EXPRESSION_H__ */
