// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_NUMERICS_CLAMPED_MATH_H_
#define V8_BASE_NUMERICS_CLAMPED_MATH_H_

#include <type_traits>

#include "src/base/numerics/clamped_math_impl.h"  // IWYU pragma: export
#include "src/base/numerics/safe_conversions.h"
#include "src/base/numerics/safe_math_shared_impl.h"  // IWYU pragma: export

namespace v8::base {
namespace internal {

template <typename T>
  requires std::is_arithmetic<T>::value
class ClampedNumeric {
 public:
  using type = T;

  constexpr ClampedNumeric() = default;

  // Copy constructor.
  template <typename Src>
  constexpr ClampedNumeric(const ClampedNumeric<Src>& rhs)
      : value_(saturated_cast<T>(rhs.value_)) {}

  // This is not an explicit constructor because we implicitly upgrade regular
  // numerics to ClampedNumerics to make them easier to use.
  template <typename Src>
    requires(IsNumeric<Src>)
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr ClampedNumeric(Src value) : value_(saturated_cast<T>(value)) {}

  // This is not an explicit constructor because we want a seamless conversion
  // from StrictNumeric types.
  template <typename Src>
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr ClampedNumeric(StrictNumeric<Src> value)
      : value_(saturated_cast<T>(static_cast<Src>(value))) {}

  // Returns a ClampedNumeric of the specified type, cast from the current
  // ClampedNumeric, and saturated to the destination type.
  template <typename Dst>
  constexpr ClampedNumeric<UnderlyingType<Dst>> Cast() const {
    return *this;
  }

  // Prototypes for the supported arithmetic operator overloads.
  template <typename Src>
  constexpr ClampedNumeric& operator+=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator-=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator*=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator/=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator%=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator<<=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator>>=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator&=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator|=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator^=(const Src rhs);

  constexpr ClampedNumeric operator-() const {
    // The negation of two's complement int min is int min, so that's the
    // only overflow case where we will saturate.
    return ClampedNumeric<T>(SaturatedNegWrapper(value_));
  }

  constexpr ClampedNumeric operator~() const {
    return ClampedNumeric<decltype(InvertWrapper(T()))>(InvertWrapper(value_));
  }

  constexpr ClampedNumeric Abs() const {
    // The negation of two's complement int min is int min, so that's the
    // only overflow case where we will saturate.
    return ClampedNumeric<T>(SaturatedAbsWrapper(value_));
  }

  template <typename U>
  constexpr ClampedNumeric<typename MathWrapper<ClampedMaxOp, T, U>::type> Max(
      U rhs) const {
    using result_type = typename MathWrapper<ClampedMaxOp, T, U>::type;
    return ClampedNumeric<result_type>(
        ClampedMaxOp<T, U>::Do(value_, Wrapper<U>::value(rhs)));
  }

  template <typename U>
  constexpr ClampedNumeric<typename MathWrapper<ClampedMinOp, T, U>::type> Min(
      U rhs) const {
    using result_type = typename MathWrapper<ClampedMinOp, T, U>::type;
    return ClampedNumeric<result_type>(
        ClampedMinOp<T, U>::Do(value_, Wrapper<U>::value(rhs)));
  }

  // This function is available only for integral types. It returns an unsigned
  // integer of the same width as the source type, containing the absolute value
  // of the source, and properly handling signed min.
  constexpr ClampedNumeric<typename UnsignedOrFloatForSize<T>::type>
  UnsignedAbs() const {
    return ClampedNumeric<typename UnsignedOrFloatForSize<T>::type>(
        SafeUnsignedAbs(value_));
  }

  constexpr ClampedNumeric& operator++() {
    *this += 1;
    return *this;
  }

  constexpr ClampedNumeric operator++(int) {
    ClampedNumeric value = *this;
    *this += 1;
    return value;
  }

  constexpr ClampedNumeric& operator--() {
    *this -= 1;
    return *this;
  }

  constexpr ClampedNumeric operator--(int) {
    ClampedNumeric value = *this;
    *this -= 1;
    return value;
  }

  // These perform the actual math operations on the ClampedNumerics.
  // Binary arithmetic operations.
  template <template <typename, typename> class M, typename L, typename R>
  static constexpr ClampedNumeric MathOp(L lhs, R rhs) {
    using Math = typename MathWrapper<M, L, R>::math;
    return ClampedNumeric<T>(
        Math::template Do<T>(Wrapper<L>::value(lhs), Wrapper<R>::value(rhs)));
  }

  // Assignment arithmetic operations.
  template <template <typename, typename> class M, typename R>
  constexpr ClampedNumeric& MathOp(R rhs) {
    using Math = typename MathWrapper<M, T, R>::math;
    *this =
        ClampedNumeric<T>(Math::template Do<T>(value_, Wrapper<R>::value(rhs)));
    return *this;
  }

  template <typename Dst>
    requires std::is_arithmetic<ArithmeticOrUnderlyingEnum<Dst>::value>
  constexpr operator Dst() const {  // NOLINT(runtime/explicit)
    return saturated_cast<ArithmeticOrUnderlyingEnum<Dst>>(value_);
  }

  // This method extracts the raw integer value without saturating it to the
  // destination type as the conversion operator does. This is useful when
  // e.g. assigning to an auto type or passing as a deduced template parameter.
  constexpr T RawValue() const { return value_; }

 private:
  template <typename U>
    requires std::is_arithmetic<U>::value
  friend class ClampedNumeric;

  T value_ = 0;

  // These wrappers allow us to handle state the same way for both
  // ClampedNumeric and POD arithmetic types.
  template <typename Src>
  struct Wrapper {
    static constexpr UnderlyingType<Src> value(Src value) { return value; }
  };
};

template <typename T>
ClampedNumeric(T) -> ClampedNumeric<T>;

// Convenience wrapper to return a new ClampedNumeric from the provided
// arithmetic or ClampedNumericType.
template <typename T>
constexpr ClampedNumeric<UnderlyingType<T>> MakeClampedNum(T value) {
  return value;
}

// These implement the variadic wrapper for the math operations.
template <template <typename, typename> class M, typename L, typename R>
constexpr ClampedNumeric<typename MathWrapper<M, L, R>::type> ClampMathOp(
    L lhs, R rhs) {
  using Math = typename MathWrapper<M, L, R>::math;
  return ClampedNumeric<typename Math::result_type>::template MathOp<M>(lhs,
                                                                        rhs);
}

// General purpose wrapper template for arithmetic operations.
template <template <typename, typename> class M, typename L, typename R,
          typename... Args>
constexpr auto ClampMathOp(L lhs, R rhs, Args... args) {
  return ClampMathOp<M>(ClampMathOp<M>(lhs, rhs), args...);
}

BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Add, +, +=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Sub, -, -=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Mul, *, *=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Div, /, /=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Mod, %, %=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Lsh, <<, <<=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Rsh, >>, >>=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, And, &, &=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Or, |, |=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Xor, ^, ^=)
BASE_NUMERIC_ARITHMETIC_VARIADIC(Clamped, Clamp, Max)
BASE_NUMERIC_ARITHMETIC_VARIADIC(Clamped, Clamp, Min)
BASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsLess, <)
BASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsLessOrEqual, <=)
BASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsGreater, >)
BASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsGreaterOrEqual, >=)
BASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsEqual, ==)
BASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsNotEqual, !=)

}  // namespace internal

using internal::ClampAdd;
using internal::ClampAnd;
using internal::ClampDiv;
using internal::ClampedNumeric;
using internal::ClampLsh;
using internal::ClampMax;
using internal::ClampMin;
using internal::ClampMod;
using internal::ClampMul;
using internal::ClampOr;
using internal::ClampRsh;
using internal::ClampSub;
using internal::ClampXor;
using internal::MakeClampedNum;

}  // namespace v8::base

#endif  // V8_BASE_NUMERICS_CLAMPED_MATH_H_
