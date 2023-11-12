#include <symengine/symengine_assert.h>
#include <symengine/symengine_casts.h>

#include "Tinned/ElectronicState.hpp"

namespace Tinned
{
    ElectronicState::ElectronicState(
        const std::string& name,
        const SymEngine::multiset_basic& derivative
    ) : SymEngine::MatrixSymbol(name),
        derivative_(derivative)
    {
        SYMENGINE_ASSIGN_TYPEID()
    }

    SymEngine::hash_t ElectronicState::__hash__() const
    {
        SymEngine::hash_t seed = SymEngine::MatrixSymbol::__hash__();
        for (auto& p: derivative_) {
            SymEngine::hash_combine(seed, *p);
        }
        return seed;
    }

    bool ElectronicState::__eq__(const SymEngine::Basic& o) const
    {
        if (SymEngine::is_a_sub<const ElectronicState>(o)) {
            auto& state = SymEngine::down_cast<const ElectronicState&>(o);
            return get_name() == state.get_name()
                && SymEngine::unified_eq(derivative_, state.derivative_);
        }
        return false;
    }

    int ElectronicState::compare(const SymEngine::Basic &o) const
    {
        SYMENGINE_ASSERT(SymEngine::is_a_sub<const ElectronicState>(o))
        auto& state = SymEngine::down_cast<const ElectronicState&>(o);
        if (get_name() == state.get_name()) {
            return SymEngine::unified_compare(derivative_, state.derivative_);
        }
        else {
            return get_name() < state.get_name() ? -1 : 1;
        }
    }

    //SymEngine::vec_basic ElectronicState::get_args() const
    //{
    //    if (derivative_.empty()) {
    //        return {};
    //    }
    //    else {
    //        return SymEngine::vec_basic(derivative_.begin(), derivative_.end());
    //    }
    //}

    //SymEngine::RCP<const SymEngine::Basic> ElectronicState::diff_impl(
    //    const SymEngine::RCP<const SymEngine::Symbol>& s
    //) const
    //{
    //    auto derivative = derivative_;
    //    derivative.insert(s);
    //    return SymEngine::make_rcp<const ElectronicState>(
    //        get_name(),
    //        derivative
    //    );
    //}
}
