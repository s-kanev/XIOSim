/* Implementation of operator overloads for Expression.  */

#include "expression_impl.h"

using namespace xiosim::stats;

/* Operators for common arithmetic functions. */
CompoundExpression<std::plus<Result>> xiosim::stats::operator+(const Expression& lhs,
                                                               const Expression& rhs) {
    return CompoundExpression<std::plus<Result>>(lhs, rhs);
}

CompoundExpression<std::minus<Result>> xiosim::stats::operator-(const Expression& lhs,
                                                                const Expression& rhs) {
    return CompoundExpression<std::minus<Result>>(lhs, rhs);
}

CompoundExpression<std::multiplies<Result>> xiosim::stats::operator*(const Expression& lhs,
                                                                     const Expression& rhs) {
    return CompoundExpression<std::multiplies<Result>>(lhs, rhs);
}

CompoundExpression<std::divides<Result>> xiosim::stats::operator/(const Expression& lhs,
                                                                  const Expression& rhs) {
    return CompoundExpression<std::divides<Result>>(lhs, rhs);
}

CompoundExpression<xiosim::stats::power<Result>> xiosim::stats::operator^(const Expression& lhs,
                                                                          const Expression& rhs) {
    return CompoundExpression<xiosim::stats::power<Result>>(lhs, rhs);
}

SingleExpression<std::negate<Result>> xiosim::stats::
operator-(const Expression& lhs) {
    return SingleExpression<std::negate<Result>>(lhs);
}

/* Constant overload helpers. They only call the above operators with the appropriate
 * const Expression&. See note in expression_impl.h */
CompoundExpression<std::plus<Result>> xiosim::stats::operator+(const Expression& lhs,
                                                               Constant rhs) {
    const Expression& rhs_ref = rhs;
    return xiosim::stats::operator+(lhs, rhs_ref);
}

CompoundExpression<std::plus<Result>> xiosim::stats::operator+(Constant lhs,
                                                               const Expression& rhs) {
    const Expression& lhs_ref = lhs;
    return xiosim::stats::operator+(lhs_ref, rhs);
}

CompoundExpression<std::minus<Result>> xiosim::stats::operator-(const Expression& lhs,
                                                                Constant rhs) {
    const Expression& rhs_ref = rhs;
    return xiosim::stats::operator-(lhs, rhs_ref);
}

CompoundExpression<std::minus<Result>> xiosim::stats::operator-(Constant lhs,
                                                                const Expression& rhs) {
    const Expression& lhs_ref = lhs;
    return xiosim::stats::operator-(lhs_ref, rhs);
}

CompoundExpression<std::multiplies<Result>> xiosim::stats::operator*(const Expression& lhs,
                                                                     Constant rhs) {
    const Expression& rhs_ref = rhs;
    return xiosim::stats::operator*(lhs, rhs_ref);
}

CompoundExpression<std::multiplies<Result>> xiosim::stats::operator*(Constant lhs,
                                                                     const Expression& rhs) {
    const Expression& lhs_ref = lhs;
    return xiosim::stats::operator*(lhs_ref, rhs);
}

CompoundExpression<std::divides<Result>> xiosim::stats::operator/(const Expression& lhs,
                                                                  Constant rhs) {
    const Expression& rhs_ref = rhs;
    return xiosim::stats::operator/(lhs, rhs_ref);
}

CompoundExpression<std::divides<Result>> xiosim::stats::operator/(Constant lhs,
                                                                  const Expression& rhs) {
    const Expression& lhs_ref = lhs;
    return xiosim::stats::operator/(lhs_ref, rhs);
}

CompoundExpression<xiosim::stats::power<Result>> xiosim::stats::operator^(const Expression& lhs,
                                                                          Constant rhs) {
    const Expression& rhs_ref = rhs;
    return xiosim::stats::operator^(lhs, rhs_ref);
}

CompoundExpression<xiosim::stats::power<Result>> xiosim::stats::
operator^(Constant lhs, const Expression& rhs) {
    const Expression& lhs_ref = lhs;
    return xiosim::stats::operator^(lhs_ref, rhs);
}
