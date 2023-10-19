#include <utility>

#include <symengine/pow.h>
#include <symengine/symengine_exception.h>

#include "Tinned/Perturbation.hpp"
#include "Tinned/PertDependency.hpp"
#include "Tinned/ElectronicState.hpp"
#include "Tinned/OneElecDensity.hpp"
#include "Tinned/OneElecOperator.hpp"
#include "Tinned/TwoElecOperator.hpp"
#include "Tinned/ExchCorrEnergy.hpp"
#include "Tinned/ExchCorrPotential.hpp"
#include "Tinned/NonElecFunction.hpp"
#include "Tinned/TemporumOperator.hpp"
#include "Tinned/TemporumOverlap.hpp"

#include "Tinned/KeepVisitor.hpp"

namespace Tinned
{
    // This function is similar to that for MatrixMul, so see the detailed
    // explanation for that function
    void KeepVisitor::bvisit(const SymEngine::Mul& x)
    {
        if (condition_(x)) {
            SymEngine::RCP<const SymEngine::Number> coef = x.get_coef();
            auto new_coef = apply(*coef);
            if (!new_coef.is_null()) {
                // `Mul` will be kept as a whole if the coefficient is kept
                if (SymEngine::eq(*coef, *new_coef)) {
                    result_ = x.rcp_from_this();
                    return;
                }
                // The coefficient is partially kept: c = ck + cr where ck is
                // kept and cr is remove. We save cr = c - ck
                else {
                    coef = SymEngine::subnum(coef, new_coef);
                }
            }
            SymEngine::map_basic_basic d;
            // We check each pair (`Basic` and `Basic`) in the dictionary
            // of `Mul`
            for (const auto& p : x.get_dict()) {
                // First check the whole factor, i.e. key^value
                auto factor = SymEngine::make_rcp<SymEngine::Pow>(p.first, p.second);
                if (condition_(*factor)) {
                    auto new_key = apply(p.first);
                    if (new_key.is_null()) {
                        // The key does not match any given symbols, but we
                        // save this pair in case that there will be factor(s)
                        // kept
                        SymEngine::Mul::dict_add_term_new(
                            SymEngine::outArg(coef), d, p.second, p.first
                        );
                    }
                    else {
                        // `Mul` will be kept as a whole if this key matches
                        // any given symbols and is kept
                        if (SymEngine::eq(*p.first, *new_key)) {
                            result_ = x.rcp_from_this();
                            return;
                        }
                        // (Ak+Ar)^a where Ak matches a given symbol and kept,
                        // not Ar. We simply use Newton's generalized binomial
                        // theorem
                        // (https://en.wikipedia.org/wiki/Binomial_theorem#Newton's_generalized_binomial_theorem),
                        // and save Ar^a.
                        else {
                            SymEngine::Mul::dict_add_term_new(
                                SymEngine::outArg(coef),
                                d,
                                p.second,
                                SymEngine::sub(p.first, new_key)
                            );
                        }

                        // The value in the pair (the exponent) is not allowed to
                        // be removed completely
                        auto new_value = apply(p.second);
                        if (new_value.is_null()) {
                            throw SymEngine::SymEngineException(
                                "Removing the exponent in a key-value pair of Mul is not allowed."
                            );
                        }
                        else {
                            SymEngine::Mul::dict_add_term_new(
                                SymEngine::outArg(coef), d, new_value, new_key
                            );
                        }

                    }
                }
                // `Mul` will be kept as a whole if this factor matches any
                // given symbols and is kept
                else {
                    result_ = x.rcp_from_this();
                    return;
                }
            }
            result_ = SymEngine::Mul::from_dict(coef, std::move(d));
        }
        // `Mul` will be kept as a whole
        else {
            result_ = x.rcp_from_this();
        }
    }

    void KeepVisitor::bvisit(const SymEngine::FunctionSymbol& x)
    {
        // We don't allow for the removal of derivative symbols, but only check
        // if the `NonElecFunction` (or its derivative) can be removed as a whole
        if (SymEngine::is_a_sub<const NonElecFunction>(x)) {
            remove_if_symbol_like<const SymEngine::NonElecFunction>(
                SymEngine::down_cast<const NonElecFunction&>(x)
            );
        }
        else if (SymEngine::is_a_sub<const ExchCorrEnergy>(x)) {

        }
        else {
            throw SymEngine::NotImplementedError(
                "KeepVisitor::bvisit() not implemented for FunctionSymbol " + x.__str__()
            );
        }
    }

    void KeepVisitor::bvisit(const SymEngine::MatrixSymbol& x)
    {
        if (SymEngine::is_a_sub<const OneElecDensity>(x)) {
            remove_if_symbol_like<const OneElecDensity>(
                SymEngine::down_cast<const OneElecDensity&>(x)
            );
        }
        else if (SymEngine::is_a_sub<const OneElecOperator>(x)) {
            remove_if_symbol_like<const OneElecOperator>(
                SymEngine::down_cast<const OneElecOperator&>(x)
            );
        }
        else if (SymEngine::is_a_sub<const TwoElecOperator>(x)) {
            auto& op = SymEngine::down_cast<const TwoElecOperator&>(x);
            remove_ifnot_one_arg_f<const TwoElecOperator>(
                x,
                [=]() -> SymEngine::RCP<const SymEngine::MatrixExpr>
                {
                    return op.get_state();
                },
                op.get_dependencies(),
                op.get_derivative()
            );
        }
        else if (SymEngine::is_a_sub<const ExchCorrPotential>(x)) {

        }
        else if (SymEngine::is_a_sub<const TemporumOperator>(x)) {
            auto& op = SymEngine::down_cast<const TemporumOperator&>(x);
            remove_ifnot_one_arg_f<const TemporumOperator>(
                x,
                [=]() -> SymEngine::RCP<const SymEngine::MatrixExpr>
                {
                    return op.get_target();
                },
                op.get_type()
            );
        }
        else if (SymEngine::is_a_sub<const TemporumOverlap>(x)) {
            remove_if_symbol_like<const TemporumOverlap>(
                SymEngine::down_cast<const TemporumOverlap&>(x)
            );
        }
        else {
            throw SymEngine::NotImplementedError(
                "KeepVisitor::bvisit() not implemented for MatrixSymbol " + x.__str__()
            );
        }
    }

    void KeepVisitor::bvisit(const SymEngine::Trace& x)
    {
        remove_ifnot_one_arg_f<const SymEngine::Trace>(
            x,
            [=]() -> SymEngine::RCP<const SymEngine::MatrixExpr>
            {
                return SymEngine::down_cast<const SymEngine::Trace&>(x).get_args()[0];
            }
        );
    }

    void KeepVisitor::bvisit(const SymEngine::ConjugateMatrix& x)
    {
        remove_ifnot_one_arg_f<const SymEngine::ConjugateMatrix>(
            x,
            [=]() -> SymEngine::RCP<const SymEngine::MatrixExpr>
            {
                return SymEngine::down_cast<const SymEngine::ConjugateMatrix&>(x).get_arg();
            }
        );
    }

    void KeepVisitor::bvisit(const SymEngine::Transpose& x)
    {
        remove_ifnot_one_arg_f<const SymEngine::Transpose>(
            x,
            [=]() -> SymEngine::RCP<const SymEngine::MatrixExpr>
            {
                return SymEngine::down_cast<const SymEngine::Transpose&>(x).get_arg();
            }
        );
    }

    void KeepVisitor::bvisit(const SymEngine::MatrixMul& x)
    {
        // If `MatrixMul` will not be kept as whole, we then check if its
        // factors will be kept
        if (condition_(x)) {
            // `factors` will be used to construct the product that will be
            // removed. The first factor is -1 for substraction, see
            // information below.
            SymEngine::vec_basic factors = SymEngine::vec_basic({SymEngine::minus_one});
            // Indicates if there is factor(s) kept
            bool factors_kept = false;
            for (auto arg: SymEngine::down_cast<const SymEngine::MatrixMul&>(x).get_args()) {
                auto new_arg = apply(arg);
                if (new_arg.is_null()) {
                    // This factor does not match any given symbols, but we
                    // save it in case that there will be factor(s) kept
                    factors.push_back(arg);
                }
                else {
                    // `MatrixMul` will be kept as a whole if this factor
                    // matches any given symbols and is kept
                    if (SymEngine::eq(*arg, *new_arg)) {
                        result_ = x.rcp_from_this();
                        return;
                    }
                    else {
                        // Suppose `MatrixMul` is A*B*C*... = (Ak+Ar)*B*C*...,
                        // where Ak will be kept and Ar will be removed. The
                        // result after removal will be Ak*B*C*..., and we save
                        // Ar = A-Ak. The result can also be computed as
                        // A*B*C*... - Ar*B*C*...
                        factors.push_back(SymEngine::matrix_add(
                            SymEngine::vec_basic({
                                arg,
                                SymEngine::matrix_mul(
                                    SymEngine::vec_basic({SymEngine::minus_one, new_arg})
                                )
                            })
                        ));
                        factors_kept = true;
                    }
                }
            }
            // As aforementioned, when there are factors partially kept, the
            // result can be computed as A*B*C*...*R*S*T*... - Ar*Br*Cr*...*R*S*T*...,
            // where Ar, Br, Cr, ... are parts that are removed
            if (factors_kept) {
                result_ = SymEngine::matrix_add(
                    SymEngine::vec_basic({
                        x.rcp_from_this(),
                        SymEngine::matrix_mul(factors)
                    })
                );
            }
            // `MatrixMul` will be removed since all its factors are null after
            // removal
            else {
                result_ = SymEngine::RCP<const SymEngine::Basic>();
            }
        }
        // `MatrixMul` will be kept as a whole
        else {
            result_ = x.rcp_from_this();
        }
    }
}
