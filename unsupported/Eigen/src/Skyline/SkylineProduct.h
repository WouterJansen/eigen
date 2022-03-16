// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Guillaume Saupin <guillaume.saupin@cea.fr>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_SKYLINEPRODUCT_H
#define EIGEN_SKYLINEPRODUCT_H

#include "./InternalHeaderCheck.h"

namespace Eigen { 

template<typename Lhs, typename Rhs, int ProductMode>
struct SkylineProductReturnType {
    typedef const typename internal::nested_eval<Lhs, Rhs::RowsAtCompileTime>::type LhsNested;
    typedef const typename internal::nested_eval<Rhs, Lhs::RowsAtCompileTime>::type RhsNested;

    typedef SkylineProduct<LhsNested, RhsNested, ProductMode> Type;
};

template<typename LhsNested, typename RhsNested, int ProductMode>
struct internal::traits<SkylineProduct<LhsNested, RhsNested, ProductMode> > {
    // clean the nested types:
    typedef internal::remove_all_t<LhsNested> LhsNested_;
    typedef internal::remove_all_t<RhsNested> RhsNested_;
    typedef typename LhsNested_::Scalar Scalar;

    enum {
        LhsCoeffReadCost = LhsNested_::CoeffReadCost,
        RhsCoeffReadCost = RhsNested_::CoeffReadCost,
        LhsFlags = LhsNested_::Flags,
        RhsFlags = RhsNested_::Flags,

        RowsAtCompileTime = LhsNested_::RowsAtCompileTime,
        ColsAtCompileTime = RhsNested_::ColsAtCompileTime,
        InnerSize = internal::min_size_prefer_fixed(LhsNested_::ColsAtCompileTime, RhsNested_::RowsAtCompileTime),

        MaxRowsAtCompileTime = LhsNested_::MaxRowsAtCompileTime,
        MaxColsAtCompileTime = RhsNested_::MaxColsAtCompileTime,

        EvalToRowMajor = is_row_major(RhsFlags & LhsFlags),
        ResultIsSkyline = ProductMode == SkylineTimeSkylineProduct,

        RemovedBits = ~(ResultIsSkyline ? 0 : SkylineBit),

        Flags = with_storage_order((int(LhsFlags | RhsFlags) & HereditaryBits & RemovedBits)
        | EvalBeforeAssigningBit
        | EvalBeforeNestingBit, EvalToRowMajor ? StorageOrder::RowMajor : StorageOrder::ColMajor),

        CoeffReadCost = HugeCost
    };

    typedef std::conditional_t<ResultIsSkyline,
            SkylineMatrixBase<SkylineProduct<LhsNested, RhsNested, ProductMode> >,
            MatrixBase<SkylineProduct<LhsNested, RhsNested, ProductMode> > > Base;
};

namespace internal {
template<typename LhsNested, typename RhsNested, int ProductMode>
class SkylineProduct : no_assignment_operator,
public traits<SkylineProduct<LhsNested, RhsNested, ProductMode> >::Base {
public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(SkylineProduct)

private:

    typedef typename traits<SkylineProduct>::LhsNested_ LhsNested_;
    typedef typename traits<SkylineProduct>::RhsNested_ RhsNested_;

public:

    template<typename Lhs, typename Rhs>
    EIGEN_STRONG_INLINE SkylineProduct(const Lhs& lhs, const Rhs& rhs)
    : m_lhs(lhs), m_rhs(rhs) {
        eigen_assert(lhs.cols() == rhs.rows());

        enum {
            ProductIsValid = LhsNested_::ColsAtCompileTime == Dynamic
            || RhsNested_::RowsAtCompileTime == Dynamic
            || int(LhsNested_::ColsAtCompileTime) == int(RhsNested_::RowsAtCompileTime),
            AreVectors = LhsNested_::IsVectorAtCompileTime && RhsNested_::IsVectorAtCompileTime,
            SameSizes = EIGEN_PREDICATE_SAME_MATRIX_SIZE(LhsNested_, RhsNested_)
        };
        // note to the lost user:
        //    * for a dot product use: v1.dot(v2)
        //    * for a coeff-wise product use: v1.cwise()*v2
        EIGEN_STATIC_ASSERT(ProductIsValid || !(AreVectors && SameSizes),
                INVALID_VECTOR_VECTOR_PRODUCT__IF_YOU_WANTED_A_DOT_OR_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTIONS)
                EIGEN_STATIC_ASSERT(ProductIsValid || !(SameSizes && !AreVectors),
                INVALID_MATRIX_PRODUCT__IF_YOU_WANTED_A_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTION)
                EIGEN_STATIC_ASSERT(ProductIsValid || SameSizes, INVALID_MATRIX_PRODUCT)
    }

    EIGEN_STRONG_INLINE Index rows() const {
        return m_lhs.rows();
    }

    EIGEN_STRONG_INLINE Index cols() const {
        return m_rhs.cols();
    }

    EIGEN_STRONG_INLINE const LhsNested_& lhs() const {
        return m_lhs;
    }

    EIGEN_STRONG_INLINE const RhsNested_& rhs() const {
        return m_rhs;
    }

protected:
    LhsNested m_lhs;
    RhsNested m_rhs;
};

// dense = skyline * dense
// Note that here we force no inlining and separate the setZero() because GCC messes up otherwise

template<typename Lhs, typename Rhs, typename Dest>
EIGEN_DONT_INLINE void skyline_row_major_time_dense_product(const Lhs& lhs, const Rhs& rhs, Dest& dst) {
    typedef remove_all_t<Lhs> Lhs_;
    typedef remove_all_t<Rhs> Rhs_;
    typedef typename traits<Lhs>::Scalar Scalar;

    enum {
        LhsIsRowMajor = is_row_major(Lhs_::Flags),
        LhsIsSelfAdjoint = (Lhs_::Flags & SelfAdjointBit) == SelfAdjointBit,
        ProcessFirstHalf = LhsIsSelfAdjoint
        && (((Lhs_::Flags & (UpperTriangularBit | LowerTriangularBit)) == 0)
        || ((Lhs_::Flags & UpperTriangularBit) && !LhsIsRowMajor)
        || ((Lhs_::Flags & LowerTriangularBit) && LhsIsRowMajor)),
        ProcessSecondHalf = LhsIsSelfAdjoint && (!ProcessFirstHalf)
    };

    //Use matrix diagonal part <- Improvement : use inner iterator on dense matrix.
    for (Index col = 0; col < rhs.cols(); col++) {
        for (Index row = 0; row < lhs.rows(); row++) {
            dst(row, col) = lhs.coeffDiag(row) * rhs(row, col);
        }
    }
    //Use matrix lower triangular part
    for (Index row = 0; row < lhs.rows(); row++) {
        typename Lhs_::InnerLowerIterator lIt(lhs, row);
        const Index stop = lIt.col() + lIt.size();
        for (Index col = 0; col < rhs.cols(); col++) {

            Index k = lIt.col();
            Scalar tmp = 0;
            while (k < stop) {
                tmp +=
                        lIt.value() *
                        rhs(k++, col);
                ++lIt;
            }
            dst(row, col) += tmp;
            lIt += -lIt.size();
        }

    }

    //Use matrix upper triangular part
    for (Index lhscol = 0; lhscol < lhs.cols(); lhscol++) {
        typename Lhs_::InnerUpperIterator uIt(lhs, lhscol);
        const Index stop = uIt.size() + uIt.row();
        for (Index rhscol = 0; rhscol < rhs.cols(); rhscol++) {


            const Scalar rhsCoeff = rhs.coeff(lhscol, rhscol);
            Index k = uIt.row();
            while (k < stop) {
                dst(k++, rhscol) +=
                        uIt.value() *
                        rhsCoeff;
                ++uIt;
            }
            uIt += -uIt.size();
        }
    }

}

template<typename Lhs, typename Rhs, typename Dest>
EIGEN_DONT_INLINE void skyline_col_major_time_dense_product(const Lhs& lhs, const Rhs& rhs, Dest& dst) {
    typedef remove_all_t<Lhs> Lhs_;
    typedef remove_all_t<Rhs> Rhs_;
    typedef typename traits<Lhs>::Scalar Scalar;

    enum {
        LhsIsRowMajor = is_row_major(Lhs_::Flags),
        LhsIsSelfAdjoint = (Lhs_::Flags & SelfAdjointBit) == SelfAdjointBit,
        ProcessFirstHalf = LhsIsSelfAdjoint
        && (((Lhs_::Flags & (UpperTriangularBit | LowerTriangularBit)) == 0)
        || ((Lhs_::Flags & UpperTriangularBit) && !LhsIsRowMajor)
        || ((Lhs_::Flags & LowerTriangularBit) && LhsIsRowMajor)),
        ProcessSecondHalf = LhsIsSelfAdjoint && (!ProcessFirstHalf)
    };

    //Use matrix diagonal part <- Improvement : use inner iterator on dense matrix.
    for (Index col = 0; col < rhs.cols(); col++) {
        for (Index row = 0; row < lhs.rows(); row++) {
            dst(row, col) = lhs.coeffDiag(row) * rhs(row, col);
        }
    }

    //Use matrix upper triangular part
    for (Index row = 0; row < lhs.rows(); row++) {
        typename Lhs_::InnerUpperIterator uIt(lhs, row);
        const Index stop = uIt.col() + uIt.size();
        for (Index col = 0; col < rhs.cols(); col++) {

            Index k = uIt.col();
            Scalar tmp = 0;
            while (k < stop) {
                tmp +=
                        uIt.value() *
                        rhs(k++, col);
                ++uIt;
            }


            dst(row, col) += tmp;
            uIt += -uIt.size();
        }
    }

    //Use matrix lower triangular part
    for (Index lhscol = 0; lhscol < lhs.cols(); lhscol++) {
        typename Lhs_::InnerLowerIterator lIt(lhs, lhscol);
        const Index stop = lIt.size() + lIt.row();
        for (Index rhscol = 0; rhscol < rhs.cols(); rhscol++) {

            const Scalar rhsCoeff = rhs.coeff(lhscol, rhscol);
            Index k = lIt.row();
            while (k < stop) {
                dst(k++, rhscol) +=
                        lIt.value() *
                        rhsCoeff;
                ++lIt;
            }
            lIt += -lIt.size();
        }
    }

}

template<typename Lhs, typename Rhs, typename ResultType,
        int LhsStorageOrder = get_storage_order(traits<Lhs>::Flags)>
        struct skyline_product_selector;

template<typename Lhs, typename Rhs, typename ResultType>
struct skyline_product_selector<Lhs, Rhs, ResultType, RowMajor> {
    typedef typename traits<remove_all_t<Lhs>>::Scalar Scalar;

    static void run(const Lhs& lhs, const Rhs& rhs, ResultType & res) {
        skyline_row_major_time_dense_product<Lhs, Rhs, ResultType > (lhs, rhs, res);
    }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct skyline_product_selector<Lhs, Rhs, ResultType, ColMajor> {
    typedef typename traits<remove_all_t<Lhs>>::Scalar Scalar;

    static void run(const Lhs& lhs, const Rhs& rhs, ResultType & res) {
        skyline_col_major_time_dense_product<Lhs, Rhs, ResultType > (lhs, rhs, res);
    }
};

} // end namespace internal

// template<typename Derived>
// template<typename Lhs, typename Rhs >
// Derived & MatrixBase<Derived>::lazyAssign(const SkylineProduct<Lhs, Rhs, SkylineTimeDenseProduct>& product) {
//     typedef internal::remove_all_t<Lhs> Lhs_;
//     internal::skyline_product_selector<internal::remove_all_t<Lhs>,
//             internal::remove_all_t<Rhs>,
//             Derived>::run(product.lhs(), product.rhs(), derived());
// 
//     return derived();
// }

// skyline * dense

template<typename Derived>
template<typename OtherDerived >
EIGEN_STRONG_INLINE const typename SkylineProductReturnType<Derived, OtherDerived>::Type
SkylineMatrixBase<Derived>::operator*(const MatrixBase<OtherDerived> &other) const {

    return typename SkylineProductReturnType<Derived, OtherDerived>::Type(derived(), other.derived());
}

} // end namespace Eigen

#endif // EIGEN_SKYLINEPRODUCT_H
