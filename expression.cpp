/* Implementation of operator overloads for ExpressionWrapper.  */

#include "expression.h"

using namespace xiosim::stats;

/* Operators for common arithmetic functions. */
ExpressionWrapper xiosim::stats::operator+(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    return ExpressionWrapper(
        std::unique_ptr<Expression>(new CompoundExpression<std::plus<Result>>(lhs, rhs)));
}

ExpressionWrapper xiosim::stats::operator-(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    return ExpressionWrapper(
        std::unique_ptr<Expression>(new CompoundExpression<std::minus<Result>>(lhs, rhs)));
}

ExpressionWrapper xiosim::stats::operator*(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    return ExpressionWrapper(
        std::unique_ptr<Expression>(new CompoundExpression<std::multiplies<Result>>(lhs, rhs)));
}

ExpressionWrapper xiosim::stats::operator/(ExpressionWrapper lhs, ExpressionWrapper rhs) {
    return ExpressionWrapper(
        std::unique_ptr<Expression>(new CompoundExpression<std::divides<Result>>(lhs, rhs)));
}

ExpressionWrapper xiosim::stats::operator-(ExpressionWrapper lhs) {
    return ExpressionWrapper(
        std::unique_ptr<Expression>(new SingleExpression<std::negate<Result>>(lhs)));
}
