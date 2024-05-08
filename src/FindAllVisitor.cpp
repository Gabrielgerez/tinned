#include <symengine/pow.h>

//#include "Tinned/Perturbation.hpp"
#include "Tinned/LagMultiplier.hpp"

#include "Tinned/OneElecDensity.hpp"
#include "Tinned/OneElecOperator.hpp"
#include "Tinned/TwoElecEnergy.hpp"
#include "Tinned/TwoElecOperator.hpp"
#include "Tinned/CompositeFunction.hpp"
#include "Tinned/ExchCorrEnergy.hpp"
#include "Tinned/ExchCorrPotential.hpp"
#include "Tinned/NonElecFunction.hpp"
#include "Tinned/TemporumOperator.hpp"
#include "Tinned/TemporumOverlap.hpp"

#include "Tinned/StateVector.hpp"
#include "Tinned/StateOperator.hpp"
#include "Tinned/AdjointMap.hpp"
#include "Tinned/ExpAdjointHamiltonian.hpp"

#include "Tinned/ZeroOperator.hpp"

#include "Tinned/FindAllVisitor.hpp"

namespace Tinned
{
    void FindAllVisitor::bvisit(const SymEngine::Basic& x)
    {
        throw SymEngine::NotImplementedError(
            "FindAllVisitor::bvisit() not implemented for "+x.__str__()
        );
    }

    void FindAllVisitor::bvisit(const SymEngine::Symbol& x)
    {
        find_equivalence<const SymEngine::Symbol>(x);
    }

    void FindAllVisitor::bvisit(const SymEngine::Integer& x)
    {
        find_equivalence<const SymEngine::Integer>(x);
    }

    void FindAllVisitor::bvisit(const SymEngine::Rational& x)
    {
        find_equivalence<const SymEngine::Rational>(x);
    }

    void FindAllVisitor::bvisit(const SymEngine::Complex& x)
    {
        find_equivalence<const SymEngine::Complex>(x);
    }

    void FindAllVisitor::bvisit(const SymEngine::Add& x)
    {
        // We first check if the whole `Add` is that we are looking for
        if (!find_equivalence<const SymEngine::Add>(x)) {
            // We skip all pairs if the coefficient is what we are looking for
            if (!find_equivalence<const SymEngine::Number>(*x.get_coef())) {
                // Next we check each pair (`Basic` and `Number`) in the
                // dictionary of `Add`
                for (const auto& p: x.get_dict()) {
                    // Check if `Number` matches
                    if (find_equivalence(*p.second)) continue;
                    // Check if this pair is that we are looking for
                    if (find_equivalence(*SymEngine::Add::from_dict(
                        SymEngine::zero, {{p.first, p.second}}
                    ))) continue;
                    // Check if `Basic` matches
                    auto result = apply(p.first);
                    if (!result.empty()) result_.insert(result.begin(), result.end());
                }
            }
        }
    }

    void FindAllVisitor::bvisit(const SymEngine::Mul& x)
    {
        // We first check if the whole `Mul` is that we are looking for
        if (!find_equivalence<const SymEngine::Mul>(x)) {
            // We skip all pairs if the coefficient is what we are looking for
            if (!find_equivalence<const SymEngine::Number>(*x.get_coef())) {
                // We check each pair (`Basic` and `Basic`) in the dictionary
                // of `Mul`
                for (const auto& p : x.get_dict()) {
                    // Check if this pair is that we are looking for
                    if (find_equivalence(
                        *SymEngine::make_rcp<SymEngine::Pow>(p.first, p.second)
                    )) continue;
                    // Check if the key matches
                    auto result = apply(p.first);
                    if (!result.empty()) result_.insert(result.begin(), result.end());
                    // Check if the value matches
                    result = apply(p.second);
                    if (!result.empty()) result_.insert(result.begin(), result.end());
                }
            }
        }
    }

    void FindAllVisitor::bvisit(const SymEngine::Constant& x)
    {
        find_equivalence<const SymEngine::Constant>(x);
    }

    void FindAllVisitor::bvisit(const SymEngine::FunctionSymbol& x)
    {
        if (SymEngine::is_a_sub<const NonElecFunction>(x)) {
            find_with_dependencies<const NonElecFunction>(
                SymEngine::down_cast<const NonElecFunction&>(x)
            );
        }
        else if (SymEngine::is_a_sub<const TwoElecEnergy>(x)) {
            auto& op = SymEngine::down_cast<const TwoElecEnergy&>(x);
            // For `TwoElecEnergy`, we actually check its two-electron
            // integrals using `find_with_dependencies`
            if (!find_with_dependencies<const TwoElecEnergy>(op)) {
                auto result = apply(op.get_inner_state());
                if (!result.empty()) result_.insert(result.begin(), result.end());
                result = apply(op.get_outer_state());
                if (!result.empty()) result_.insert(result.begin(), result.end());
            }
        }
        else if (SymEngine::is_a_sub<const CompositeFunction>(x)) {
            auto& op = SymEngine::down_cast<const CompositeFunction&>(x);
            auto inner = op.get_inner();
            // We first check if the composite function is that we are finding,
            // by comparing its name and inner function
            if (SymEngine::is_a_sub<const CompositeFunction>(*symbol_)) {
                auto s = SymEngine::rcp_dynamic_cast<const CompositeFunction>(symbol_);
                if (op.get_name()==s->get_name()
                    && inner->__eq__(*s->get_inner())) {
                    result_.insert(x.rcp_from_this());
                    return;
                }
            }
            // If the composite function is not we are looking for, we check
            // its inner function
            auto result = apply(inner);
            if (!result.empty()) result_.insert(result.begin(), result.end());
        }
        else if (SymEngine::is_a_sub<const ExchCorrEnergy>(x)) {
            auto& op = SymEngine::down_cast<const ExchCorrEnergy&>(x);
            // We first check if `ExchCorrEnergy` is that we are finding, by
            // comparing its name and arguments
            if (SymEngine::is_a_sub<const ExchCorrEnergy>(*symbol_)) {
                auto s = SymEngine::rcp_dynamic_cast<const ExchCorrEnergy>(symbol_);
                if (op.get_name()==s->get_name()
                    && SymEngine::unified_eq(op.get_args(), s->get_args())) {
                    result_.insert(x.rcp_from_this());
                    return;
                }
            }
            // If the `ExchCorrEnergy` is not we are looking for, we check
            // its XC energy or derivatives
            auto result = apply(op.get_energy());
            if (!result.empty()) result_.insert(result.begin(), result.end());
        }
        else {
            throw SymEngine::NotImplementedError(
                "FindAllVisitor::bvisit() not implemented for FunctionSymbol "+x.__str__()
            );
        }
    }

    void FindAllVisitor::bvisit(const SymEngine::ZeroMatrix& x)
    {
        find_equivalence<const SymEngine::ZeroMatrix>(x);
    }

    void FindAllVisitor::bvisit(const SymEngine::MatrixSymbol& x)
    {
        // We check only the name for the Lagrangian multiplier
        if (SymEngine::is_a_sub<const LagMultiplier>(x)) {
            find_only_name<const LagMultiplier>(
                SymEngine::down_cast<const LagMultiplier&>(x)
            );
        }
        // We check only the name for one-electron spin-orbital density matrix
        else if (SymEngine::is_a_sub<const OneElecDensity>(x)) {
            find_only_name<const OneElecDensity>(
                SymEngine::down_cast<const OneElecDensity&>(x)
            );
        }
        else if (SymEngine::is_a_sub<const OneElecOperator>(x)) {
            find_with_dependencies<const OneElecOperator>(
                SymEngine::down_cast<const OneElecOperator&>(x)
            );
        }
        else if (SymEngine::is_a_sub<const TwoElecOperator>(x)) {
            auto& op = SymEngine::down_cast<const TwoElecOperator&>(x);
            // For `TwoElecOperator`, we actually check its two-electron
            // integrals using `find_with_dependencies`
            if (!find_with_dependencies<const TwoElecOperator>(op)) {
                auto result = apply(op.get_state());
                if (!result.empty()) result_.insert(result.begin(), result.end());
            }
        }
        else if (SymEngine::is_a_sub<const ExchCorrPotential>(x)) {
            auto& op = SymEngine::down_cast<const ExchCorrPotential&>(x);
            // We first check if `ExchCorrPotential` is that we are finding, by
            // comparing its name and arguments
            if (SymEngine::is_a_sub<const ExchCorrPotential>(*symbol_)) {
                auto s = SymEngine::rcp_dynamic_cast<const ExchCorrPotential>(symbol_);
                if (op.get_name()==s->get_name()
                    && SymEngine::unified_eq(op.get_args(), s->get_args())) {
                    result_.insert(x.rcp_from_this());
                    return;
                }
            }
            // If the `ExchCorrPotential` is not we are looking for, we check
            // its XC potential operator or derivatives
            auto result = apply(op.get_potential());
            if (!result.empty()) result_.insert(result.begin(), result.end());
        }
        else if (SymEngine::is_a_sub<const TemporumOperator>(x)) {
            auto& op = SymEngine::down_cast<const TemporumOperator&>(x);
            // If the symbol to find is also a `TemporumOperator` object, we
            // compare their arguments according to the rules of
            // `FindAllVisitor`
            if (SymEngine::is_a_sub<const TemporumOperator>(*symbol_)) {
                auto s = SymEngine::rcp_dynamic_cast<const TemporumOperator>(symbol_);
                auto visitor = FindAllVisitor(s->get_target());
                if (!visitor.apply(op.get_target()).empty())
                    result_.insert(x.rcp_from_this());
            }
            // If the symbol to find is not a `TemporumOperator` object, we
            // simply check the target of `x`
            else {
                auto result = apply(op.get_target());
                if (!result.empty()) result_.insert(result.begin(), result.end());
            }
        }
        else if (SymEngine::is_a_sub<const TemporumOverlap>(x)) {
            find_with_dependencies<const TemporumOverlap>(
                SymEngine::down_cast<const TemporumOverlap&>(x)
            );
        }
        else if (SymEngine::is_a_sub<const ZeroOperator>(x)) {
            find_equivalence<const ZeroOperator>(
                SymEngine::down_cast<const ZeroOperator&>(x)
            );
        }
        else {
            throw SymEngine::NotImplementedError(
                "FindAllVisitor::bvisit() not implemented for MatrixSymbol "+x.__str__()
            );
        }
    }

    // For `Trace`, `ConjugateMatrix` and `Transpose`, we follow the same
    // procedure as `TemporumOperator`
    void FindAllVisitor::bvisit(const SymEngine::Trace& x)
    {
        if (SymEngine::is_a_sub<const SymEngine::Trace>(*symbol_)) {
            auto s = SymEngine::rcp_dynamic_cast<const SymEngine::Trace>(symbol_);
            auto visitor = FindAllVisitor(s->get_args()[0]);
            if (!visitor.apply(x.get_args()[0]).empty())
                result_.insert(x.rcp_from_this());
        }
        else {
            auto result = apply(x.get_args()[0]);
            if (!result.empty()) result_.insert(result.begin(), result.end());
        }
    }

    void FindAllVisitor::bvisit(const SymEngine::ConjugateMatrix& x)
    {
        if (SymEngine::is_a_sub<const SymEngine::ConjugateMatrix>(*symbol_)) {
            auto s = SymEngine::rcp_dynamic_cast<const SymEngine::ConjugateMatrix>(symbol_);
            auto visitor = FindAllVisitor(s->get_arg());
            if (!visitor.apply(x.get_arg()).empty())
                result_.insert(x.rcp_from_this());
        }
        else {
            auto result = apply(x.get_arg());
            if (!result.empty()) result_.insert(result.begin(), result.end());
        }
    }

    void FindAllVisitor::bvisit(const SymEngine::Transpose& x)
    {
        if (SymEngine::is_a_sub<const SymEngine::Transpose>(*symbol_)) {
            auto s = SymEngine::rcp_dynamic_cast<const SymEngine::Transpose>(symbol_);
            auto visitor = FindAllVisitor(s->get_arg());
            if (!visitor.apply(x.get_arg()).empty())
                result_.insert(x.rcp_from_this());
        }
        else {
            auto result = apply(x.get_arg());
            if (!result.empty()) result_.insert(result.begin(), result.end());
        }
    }

    void FindAllVisitor::bvisit(const SymEngine::MatrixAdd& x)
    {
        // We first check if the whole `MatrixAdd` is that we are looking for
        if (!find_equivalence<const SymEngine::MatrixAdd>(x)) {
            // Next we check each argument of `MatrixAdd`
            for (auto arg: SymEngine::down_cast<const SymEngine::MatrixAdd&>(x).get_args()) {
                auto result = apply(arg);
                if (!result.empty()) result_.insert(result.begin(), result.end());
            }
        }
    }

    void FindAllVisitor::bvisit(const SymEngine::MatrixMul& x)
    {
        // We first check if the whole `MatrixMul` is that we are looking for
        if (!find_equivalence<const SymEngine::MatrixMul>(x)) {
            // Next we check each argument of `MatrixMul`
            for (auto arg: SymEngine::down_cast<const SymEngine::MatrixMul&>(x).get_args()) {
                auto result = apply(arg);
                if (!result.empty()) result_.insert(result.begin(), result.end());
            }
        }
    }

    void FindAllVisitor::bvisit(const SymEngine::MatrixDerivative& x)
    {
        // `MatrixDerivative` represents derivatives of a `MatrixSymbol`
        // object, so we need only check if its this object is that we are
        // looking for
        auto arg = x.get_arg();
        if (arg->__eq__(*symbol_)) result_.insert(x.rcp_from_this());
    }
}
