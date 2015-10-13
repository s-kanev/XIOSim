/* Definition of formulas and expressions.
 *
 * Brief usage:
 *   Statistic<int> some_stat;
 *   Statistic<long long> some_other_stat;
 *   Formula formula( constructor params... );
 *   formula = some_stat + some_other_stat;
 *   formula = some_stat + Constant(3);
 *   formula = some_stat + some_other_stat + Constant(3);
 */

#ifndef __EXPRESSION_H__
#define __EXPRESSION_H__

#include <functional>
#include <memory>

#include "statistic.h"

namespace xiosim {
namespace stats {

/* All stats get returned as floats. */
typedef float Result;

/* Base interface of an expression tree node.
 *
 * An equation is broken down into a binary tree of Expression nodes. Calling
 * evaluate() on any one of these will recursively evaluate all the nodes below
 * it. Leaf nodes are wrappers around Statistic objects or wrappers around a
 * constant value, and non-leaf nodes represent operations to be performed on
 * one or two leaf nodes.
 */
class Expression {
  public:
    virtual Result evaluate() const = 0;
};

/* An Expression that wraps a Statistic. */
template <typename V>
class Scalar : public Expression {
  public:
    Scalar(const Statistic<V>& stat) : scalar(stat) {}

    virtual Result evaluate() const { return scalar.get_value(); }

  private:
    const Statistic<V>& scalar;
};

/* An Expression that wraps a constant value of type int, float, or double. */
class Constant : public Expression {
  public:
    Constant(const int& constant) : value(constant) {}
    Constant(const long& constant) : value(constant) {}
    Constant(const float& constant) : value(constant) {}
    Constant(const double& constant) : value(constant) {}
    Constant(const Constant& constant) : value(constant.value) {}

    virtual Result evaluate() const { return value; }

  private:
    const Result value;
};

/* Performs a unary operation (e.g. negation) on another Expression.
 *
 * The operation is defined by the Operation type, which must overload
 * operator(). */
template <typename Operation>
class SingleExpression : public Expression {
  public:
    SingleExpression(const SingleExpression& expr) : lhs(std::move(expr.lhs)) {}

    SingleExpression(std::unique_ptr<Expression> lhs) : lhs(std::move(lhs)) {}

    virtual Result evaluate() const {
        Operation op;
        return op(lhs->evaluate());
    }

  protected:
    const std::unique_ptr<Expression> lhs;
};

/* Performs an arithmetic operation on two Expressions.
 *
 * The operation is defined by the Operation type, which must overload
 * operator().
 */
template <typename Operation>
class CompoundExpression : public Expression {
  public:
    CompoundExpression(const CompoundExpression& expr)
        : lhs(std::move(expr.lhs))
        , rhs(std::move(expr.rhs)) {}

    CompoundExpression(std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs)
        : lhs(std::move(lhs))
        , rhs(std::move(rhs)) {}

    virtual Result evaluate() const {
        Operation op;
        return op(lhs->evaluate(), rhs->evaluate());
    }

  protected:
    const std::unique_ptr<Expression> lhs;
    const std::unique_ptr<Expression> rhs;
};

/*
 * A helper class for constructing Expression trees.
 *
 * ExpressionWrapper is responsible for constructing new Expression objects out
 * of Statistics or Constants. It is an interface that obviates the need for
 * temporary class constructions or casts when declaring formulas. All pointers
 * to newly constructed Expressions are unique_ptrs, and ownership is
 * tranferred between ExpressionWrappers when new SingleExpression or
 * CompoundExpression nodes are constructed.
 *
 * ExpressionWrapper explicitly lists all the type-specific Statistics
 * supported in this library to avoid being templated itself.
 *
 * Author: Sam Xi
 */

class ExpressionWrapper {
  public:
    ExpressionWrapper() {}

    ExpressionWrapper(std::unique_ptr<Expression> expression) : expr(std::move(expression)) {}

    ExpressionWrapper(const Statistic<int>& scalar) : expr(new Scalar<int>(scalar)) {}

    ExpressionWrapper(const Statistic<unsigned int>& scalar)
        : expr(new Scalar<unsigned int>(scalar)) {}

    ExpressionWrapper(const Statistic<double>& scalar) : expr(new Scalar<double>(scalar)) {}

    ExpressionWrapper(const Statistic<float>& scalar) : expr(new Scalar<float>(scalar)) {}

    ExpressionWrapper(const Statistic<long long>& scalar) : expr(new Scalar<long long>(scalar)) {}

    ExpressionWrapper(const Statistic<unsigned long long>& scalar)
        : expr(new Scalar<unsigned long long>(scalar)) {}

    ExpressionWrapper(const Constant& constant) : expr(new Constant(constant)) {}

    operator std::unique_ptr<Expression>() { return std::move(expr); }

  protected:
    std::unique_ptr<Expression> expr;
};

/* Formula statistic. */
class Formula : public BaseStatistic {
  public:
    Formula(const char* name,
            const char* desc,
            const char* output_fmt = "%12.4f",
            bool print = true,
            bool scale = true)
        : BaseStatistic(name, desc, output_fmt, print, scale) {}

    void operator=(ExpressionWrapper expr) { this->expr = std::unique_ptr<Expression>(expr); }

    /* Needed if the Formula is assigned an Expression not directly, but
     * through another function's parameters. */
    void operator=(std::unique_ptr<Expression>& expression) { expr = std::move(expression); }

    Result evaluate() { return expr->evaluate(); }

    void print_value(FILE* fd) {
        Result value = evaluate();
        fprintf(fd, output_fmt.c_str(), value);
        fprintf(fd, " # %s\n", desc.c_str());
    }

    // Formula statistics are not scaled, accumulated, or saved; if the
    // terms are, then the formula will be recomputed anyways.
    virtual void scale_value(double weight) {}
    virtual void accum_stat(BaseStatistic* other) {}
    virtual void save_value() {}
    virtual void save_delta() {}

  private:
    std::unique_ptr<Expression> expr;
};

/* Common arithmetic operators. */
ExpressionWrapper operator+(ExpressionWrapper lhs, ExpressionWrapper rhs);
ExpressionWrapper operator-(ExpressionWrapper lhs, ExpressionWrapper rhs);
ExpressionWrapper operator*(ExpressionWrapper lhs, ExpressionWrapper rhs);
ExpressionWrapper operator/(ExpressionWrapper lhs, ExpressionWrapper rhs);
ExpressionWrapper operator-(ExpressionWrapper lhs);

}  // namespace stats
}  // namespace xiosim

#endif
