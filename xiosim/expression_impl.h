/* Definition of formulas and expressions.  *
 * Brief usage:
 *   Statistic<int> some_stat;
 *   Statistic<int64_t> some_other_stat;
 *   Formula formula( constructor params... );
 *   formula = some_stat + some_other_stat;
 *   formula = some_stat + 3;
 *   formula = some_stat + some_other_stat + 3;
 */

#ifndef __EXPRESSION_IMPL_H__
#define __EXPRESSION_IMPL_H__

#include <functional>
#include <memory>

#include "expression.h"
#include "statistic.h"

namespace xiosim {
namespace stats {

/* An Expression that wraps a constant value of type int, float, or double. */
class Constant : public Expression {
  public:
    Constant(const int& constant) : value(constant) {}
    Constant(const long& constant) : value(constant) {}
    Constant(const float& constant) : value(constant) {}
    Constant(const double& constant) : value(constant) {}
    Constant(const Constant& constant) : value(constant.value) {}

    virtual Result evaluate() const { return value; }
    virtual std::unique_ptr<Expression> deep_copy() const { return std::make_unique<Constant>(value); }

  protected:
    const Result value;
};

/* Performs a unary operation (e.g. negation) on another Expression.
 *
 * The operation is defined by the Operation type, which must overload
 * operator(). */
template <typename Operation>
class SingleExpression : public Expression {
  public:
    /* Copy-construct -- deep copy expression and subexpressions. */
    SingleExpression(const SingleExpression& expr)
        : lhs(expr.lhs->deep_copy()) {}

    /* Create from a subexpression, which we'll copy and own. */
    SingleExpression(const Expression& expr)
        : lhs(expr.deep_copy()) {}

    virtual Result evaluate() const {
        Operation op;
        return op(lhs->evaluate());
    }

    /* Just copy-construct and return as an Expression pointer. */
    virtual std::unique_ptr<Expression> deep_copy() const { return std::make_unique<SingleExpression>(*this); }

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
    /* Copy-construct -- deep copy lhs and rhs subexpressions. */
    CompoundExpression(const CompoundExpression& expr)
        : lhs(expr.lhs->deep_copy())
        , rhs(expr.rhs->deep_copy()) {}

    /* Create from two subexpressions, which we'll copy and own. */
    CompoundExpression(const Expression& lhs, const Expression& rhs)
        : lhs(lhs.deep_copy())
        , rhs(rhs.deep_copy()) {}

    virtual Result evaluate() const {
        Operation op;
        return op(lhs->evaluate(), rhs->evaluate());
    }

    /* Just copy-construct and return as an Expression pointer. */
    virtual std::unique_ptr<Expression> deep_copy() const { return std::make_unique<CompoundExpression>(*this); }

  protected:
    const std::unique_ptr<Expression> lhs;
    const std::unique_ptr<Expression> rhs;
};

/* Common arithmetic operators. */
CompoundExpression<std::plus<Result>> operator+(const Expression& lhs, const Expression& rhs);
CompoundExpression<std::minus<Result>> operator-(const Expression& lhs, const Expression& rhs);
CompoundExpression<std::multiplies<Result>> operator*(const Expression& lhs, const Expression& rhs);
CompoundExpression<std::divides<Result>> operator/(const Expression& lhs, const Expression& rhs);
CompoundExpression<xiosim::stats::power<Result>> operator^(const Expression& lhs,
                                                           const Expression& rhs);
SingleExpression<std::negate<Result>> operator-(const Expression& lhs);

/* Overloads of the above when one of the sides is a Constant *value*
 * This allows us to write things like: Formula f = some_stat + 5
 * without an explicit Constant(5).
 *
 * There are two steps that need to happen: (i) create a temporary Constant object using
 * the explicit converting constructors, (ii) convert that temporary to a const Constant&.
 * After staring at the standard, both should be completely ok, and overload resolution
 * should just grab the above operators, but I must be misreading something. So, for now,
 * we'll just add overloads that call the above operators.
 */
CompoundExpression<std::plus<Result>> operator+(const Expression& lhs, Constant rhs);
CompoundExpression<std::plus<Result>> operator+(Constant lhs, const Expression& rhs);
CompoundExpression<std::minus<Result>> operator-(const Expression& lhs, Constant rhs);
CompoundExpression<std::minus<Result>> operator-(Constant lhs, const Expression& rhs);
CompoundExpression<std::multiplies<Result>> operator*(const Expression& lhs, Constant rhs);
CompoundExpression<std::multiplies<Result>> operator*(Constant lhs, const Expression& rhs);
CompoundExpression<std::divides<Result>> operator/(const Expression& lhs, Constant rhs);
CompoundExpression<std::divides<Result>> operator/(Constant lhs, const Expression& rhs);
CompoundExpression<xiosim::stats::power<Result>> operator^(const Expression& lhs, Constant rhs);
CompoundExpression<xiosim::stats::power<Result>> operator^(Constant lhs, const Expression& rhs);

/* Formula statistic - contains an expression tree of statistics.
 * Implements the Expression interface, which means that we can form expression trees
 * including Formula-s.
 * XXX: this class belongs in statistic.h, but we have to keep it here for now because
 * of the Constant operator overloads. */
class Formula : public BaseStatistic, public Expression {
  public:
    /* Copy-construct from an existing formula. */
    Formula(const Formula& formula)
        : BaseStatistic(formula)
        , expr (formula.expr->deep_copy()) {}

    /* Create a new formula with an empty expression. */
    Formula(const char* name,
            const char* desc,
            const char* output_fmt = "%12.4f",
            bool print = true,
            bool scale = true)
        : BaseStatistic(name, desc, output_fmt, print, scale) {
        if (this->output_fmt.empty())
            set_output_format_default();
    }

    /* Assign an expression to the current formula object. */
    void operator=(const Expression& expr) { this->expr = expr.deep_copy(); }

    /* Operators for incrementally building a Formula.
     *
     * These are useful when building an expression that has an unknown number
     * of terms at compile-time. In all cases, we have to check if the
     * current Expression unique_ptr is NULL or not; if it is, we don't want to
     * build a CompoundExpression, we just want to assign it to the new
     * Expression.
     */

    void operator+=(const Expression& rhs) {
        if (expr) {
            expr = (*expr + rhs).deep_copy();
        } else {
            expr = rhs.deep_copy();
        }
    }

    void operator-=(const Expression& rhs) {
        if (expr) {
            expr = (*expr - rhs).deep_copy();
        } else {
            expr = rhs.deep_copy();
        }
    }

    void operator*=(const Expression& rhs) {
        if (expr) {
            expr = (*expr * rhs).deep_copy();
        } else {
            expr = rhs.deep_copy();
        }
    }

    void operator/=(const Expression& rhs) {
        if (expr) {
            expr = (*expr / rhs).deep_copy();
        } else {
            expr = rhs.deep_copy();
        }
    }

    void operator^=(const Expression& rhs) {
        if (expr) {
            expr = (*expr ^ rhs).deep_copy();
        } else {
            expr = rhs.deep_copy();
        }
    }

    /* See the comment above aboust Constant overloads. These should disappear. */
    void operator+=(Constant rhs) {
        const Expression& rhs_ref = rhs;
        this->operator+=(rhs_ref);
    }

    void operator-=(Constant rhs) {
        const Expression& rhs_ref = rhs;
        this->operator-=(rhs_ref);
    }

    void operator*=(Constant rhs) {
        const Expression& rhs_ref = rhs;
        this->operator*=(rhs_ref);
    }

    void operator/=(Constant rhs) {
        const Expression& rhs_ref = rhs;
        this->operator/=(rhs_ref);
    }

    void operator^=(Constant rhs) {
        const Expression& rhs_ref = rhs;
        this->operator^=(rhs_ref);
    }

    /* BaseStatistic members: */

    void set_output_format_default() { this->output_fmt = "%12.4f"; }

    void print_value(FILE* fd) {
        Result value = evaluate();
        fprintf(fd, "%-28s", this->name.c_str());
        fprintf(fd, output_fmt.c_str(), value);
        fprintf(fd, " # %s\n", desc.c_str());
    }

    // Formula statistics are not scaled, accumulated, or saved; if the
    // terms are, then the formula will be recomputed anyways.
    virtual void scale_value(double weight) {}
    virtual void accum_stat(BaseStatistic* other) {}
    virtual void save_value() {}
    virtual void save_delta() {}

    /* Expression members: */

    virtual Result evaluate() const { return expr->evaluate(); }
    virtual std::unique_ptr<Expression> deep_copy() const { return std::make_unique<Formula>(*this); }

  protected:
    std::unique_ptr<Expression> expr;
};

}  // namespace stats
}  // namespace xiosim

#endif
