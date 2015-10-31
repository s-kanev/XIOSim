/* Implementation of operator overloads for ExpressionWrapper.  */

#include "expression.h"

using namespace xiosim::stats;

/* Operators for common arithmetic functions. */
ExpressionWrapper xiosim::stats::operator+(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    std::unique_ptr<Expression> ptr(new CompoundExpression<std::plus<Result>>(lhs, rhs));
    return ExpressionWrapper(ptr);
}

ExpressionWrapper xiosim::stats::operator-(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    std::unique_ptr<Expression> ptr(new CompoundExpression<std::minus<Result>>(lhs, rhs));
    return ExpressionWrapper(ptr);
}

ExpressionWrapper xiosim::stats::operator*(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    std::unique_ptr<Expression> ptr(new CompoundExpression<std::multiplies<Result>>(lhs, rhs));
    return ExpressionWrapper(ptr);
}

ExpressionWrapper xiosim::stats::operator/(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    std::unique_ptr<Expression> ptr(new CompoundExpression<std::divides<Result>>(lhs, rhs));
    return ExpressionWrapper(ptr);
}

ExpressionWrapper xiosim::stats::operator^(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    std::unique_ptr<Expression> ptr(new CompoundExpression<xiosim::stats::power<Result>>(lhs, rhs));
    return ExpressionWrapper(ptr);
}

ExpressionWrapper xiosim::stats::operator-(ExpressionWrapper lhs) {
    std::unique_ptr<Expression> ptr(new SingleExpression<std::negate<Result>>(lhs));
    return ExpressionWrapper(ptr);
}
