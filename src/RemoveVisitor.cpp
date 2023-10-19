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

#include "Tinned/RemoveVisitor.hpp"

namespace Tinned
{
    void RemoveVisitor::bvisit(const SymEngine::Basic& x)
    {
        throw SymEngine::NotImplementedError(
            "RemoveVisitor::bvisit() not implemented for " + x.__str__()
        );
    }

    void RemoveVisitor::bvisit(const SymEngine::Symbol& x)
    {
        remove_if_symbol_like<const SymEngine::Symbol>(x);
    }

    void RemoveVisitor::bvisit(const SymEngine::Integer& x)
    {
        remove_if_symbol_like<const SymEngine::Integer>(x);
    }

    void RemoveVisitor::bvisit(const SymEngine::Rational& x)
    {
        remove_if_symbol_like<const SymEngine::Rational>(x);
    }

    void RemoveVisitor::bvisit(const SymEngine::Complex& x)
    {
        remove_if_symbol_like<const SymEngine::Complex>(x);
    }

    void RemoveVisitor::bvisit(const SymEngine::Add& x)
    {
        // We first check if `Add` will be removed as a whole
        if (condition_(x)) {
            result_ = SymEngine::RCP<const SymEngine::Basic>();
        }
        else {
            SymEngine::umap_basic_num d;
            // First we check if the coefficient will be removed
            SymEngine::RCP<const SymEngine::Number> coef = x.get_coef();
            if (condition_(*coef)) coef = SymEngine::zero;
            // Next we check each pair (`Basic` and `Number`) in the dictionary
            // of `Add`
            for (const auto& p: x.get_dict()) {
                // Skip if this pair will be removed as a whole
                if (condition_(*SymEngine::Add::from_dict(
                    SymEngine::zero, {{p.first, p.second}}
                ))) continue;
                // Skip if `Basic` was removed
                auto new_key = apply(p.first);
                if (new_key.is_null()) continue;
                // Skip if `Number` will be removed
                if (condition_(*p.second)) continue;
                SymEngine::Add::coef_dict_add_term(
                    SymEngine::outArg(coef), d, p.second, new_key
                );
            }
            result_ = SymEngine::Add::from_dict(coef, std::move(d));
        }
    }

    void RemoveVisitor::bvisit(const SymEngine::Mul& x)
    {
        // We first check if `Mul` will be removed as a whole
        if (condition_(x)) {
            result_ = SymEngine::RCP<const SymEngine::Basic>();
        }
        else {
            // We remove the whole `Mul` if its coefficient will be removed
            SymEngine::RCP<const SymEngine::Number> coef = x.get_coef();
            if (condition_(*coef)) {
                result_ = SymEngine::RCP<const SymEngine::Basic>();
            }
            else {
                SymEngine::map_basic_basic d;
                // We check each pair (`Basic` and `Basic`) in the dictionary
                // of `Mul`
                for (const auto& p : x.get_dict()) {
                    // We remove the whole `Mul` if the pair will be removed
                    auto factor = SymEngine::make_rcp<SymEngine::Pow>(p.first, p.second);
                    if (condition_(*factor)) {
                        result_ = SymEngine::RCP<const SymEngine::Basic>();
                        return;
                    }
                    // Remove the whole `Mul` if the key will be removed
                    auto new_key = apply(p.first);
                    if (new_key.is_null()) {
                        result_ = SymEngine::RCP<const SymEngine::Basic>();
                        return;
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
                result_ = SymEngine::Mul::from_dict(coef, std::move(d));
            }
        }
    }

    void RemoveVisitor::bvisit(const SymEngine::Constant& x)
    {
        remove_if_symbol_like<const SymEngine::Constant>(x);
    }

    void RemoveVisitor::bvisit(const SymEngine::FunctionSymbol& x)
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
                "RemoveVisitor::bvisit() not implemented for FunctionSymbol " + x.__str__()
            );
        }
    }

    void RemoveVisitor::bvisit(const SymEngine::ZeroMatrix& x)
    {
        remove_if_symbol_like<const SymEngine::ZeroMatrix>(x);
    }

    void RemoveVisitor::bvisit(const SymEngine::MatrixSymbol& x)
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
            remove_if_one_arg_f<const TwoElecOperator>(
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
            remove_if_one_arg_f<const TemporumOperator>(
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
                "RemoveVisitor::bvisit() not implemented for MatrixSymbol " + x.__str__()
            );
        }
    }

    void RemoveVisitor::bvisit(const SymEngine::Trace& x)
    {
        remove_if_one_arg_f<const SymEngine::Trace>(
            x,
            [=]() -> SymEngine::RCP<const SymEngine::MatrixExpr>
            {
                return SymEngine::down_cast<const SymEngine::Trace&>(x).get_args()[0];
            }
        );
    }

    void RemoveVisitor::bvisit(const SymEngine::ConjugateMatrix& x)
    {
        remove_ifnot_one_arg_f<const SymEngine::ConjugateMatrix>(
            x,
            [=]() -> SymEngine::RCP<const SymEngine::MatrixExpr>
            {
                return SymEngine::down_cast<const SymEngine::ConjugateMatrix&>(x).get_arg();
            }
        );
    }

    void RemoveVisitor::bvisit(const SymEngine::Transpose& x)
    {
        remove_ifnot_one_arg_f<const SymEngine::Transpose>(
            x,
            [=]() -> SymEngine::RCP<const SymEngine::MatrixExpr>
            {
                return SymEngine::down_cast<const SymEngine::Transpose&>(x).get_arg();
            }
        );
    }

    void RemoveVisitor::bvisit(const SymEngine::MatrixAdd& x)
    {
        // We first check if `MatrixAdd` will be removed as a whole
        if (condition_(x)) {
            result_ = SymEngine::RCP<const SymEngine::Basic>();
        }
        else {
            // Next we check each argument of `MatrixAdd`
            SymEngine::vec_basic terms;
            for (auto arg: SymEngine::down_cast<const SymEngine::MatrixAdd&>(x).get_args()) {
                auto new_arg = apply(arg);
                if (!new_arg.is_null()) terms.push_back(new_arg);
            }
            if (terms.empty()) {
                result_ = SymEngine::RCP<const SymEngine::Basic>();
            }
            else {
                result_ = SymEngine::matrix_add(terms);
            }
        }
    }

    void RemoveVisitor::bvisit(const SymEngine::MatrixMul& x)
    {
        // We first check if `MatrixMul` will be removed as a whole
        if (condition_(x)) {
            result_ = SymEngine::RCP<const SymEngine::Basic>();
        }
        else {
            // Next we check each argument of `MatrixMul`
            SymEngine::vec_basic factors;
            for (auto arg: SymEngine::down_cast<const SymEngine::MatrixMul&>(x).get_args()) {
                auto new_arg = apply(arg);
                if (new_arg.is_null()) {
                    result_ = SymEngine::RCP<const SymEngine::Basic>();
                    return;
                }
                else {
                    factors.push_back(new_arg);
                }
            }
            // Probably we do not need to check if factors is empty because the
            // above loop over arguments should be at least executed once
            if (factors.empty()) {
                result_ = SymEngine::RCP<const SymEngine::Basic>();
            }
            else {
                result_ = SymEngine::matrix_mul(factors);
            }
        }
    }

    void RemoveVisitor::bvisit(const SymEngine::MatrixDerivative& x)
    {
        // Because only `MatrixSymbol` can be used as the argument of
        // `MatrixDerivative`, we only need to check if `MatrixDerivative` will
        // be removed as a whole
        remove_if_symbol_like<const SymEngine::MatrixDerivative>(x);
    }
}
