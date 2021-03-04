// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2020 Chris Schoutrop <c.e.m.schoutrop@tue.nl>
// Copyright (C) 2020 Jens Wehner <j.wehner@esciencecenter.nl>
// Copyright (C) 2020 Jan van Dijk <j.v.dijk@tue.nl>
// Copyright (C) 2020 Adithya Vijaykumar
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

/*

        This implementation of BiCGStab(L) is based on the papers
                General algorithm:
                1. G.L.G. Sleijpen, D.R. Fokkema. (1993). BiCGstab(l) for linear equations involving unsymmetric
        matrices with complex spectrum. Electronic Transactions on Numerical Analysis. Polynomial step update:
                2. G.L.G. Sleijpen, M.B. Van Gijzen. (2010) Exploiting BiCGstab(l) strategies to induce dimension
        reduction SIAM Journal on Scientific Computing.
                3. Fokkema, Diederik R. Enhanced implementation of BiCGstab (l) for solving linear systems of equations.
        Universiteit Utrecht. Mathematisch Instituut, 1996
*/

#ifndef EIGEN_BICGSTABL_H
#define EIGEN_BICGSTABL_H

namespace Eigen
{

	namespace internal
	{
		/**     \internal Low-level bi conjugate gradient stabilized algorithm with L additional residual minimization steps
		        \param mat The matrix A
		        \param rhs The right hand side vector b
		        \param x On input and initial solution, on output the computed solution.
		        \param precond A preconditioner being able to efficiently solve for an
		                  approximation of Ax=b (regardless of b)
		        \param iters On input the max number of iteration, on output the number of performed iterations.
		        \param tol_error On input the tolerance error, on output an estimation of the relative error.
		        \param L On input Number of additional GMRES steps to take. If L is too large (~20) instabilities occur.
		        \return false in the case of numerical issue, for example a break down of BiCGSTABL.
		*/
		template <typename MatrixType, typename Rhs, typename Dest, typename Preconditioner>
		bool bicgstabl(const MatrixType& mat, const Rhs& rhs, Dest& x, const Preconditioner& precond,
			Index& iters,
			typename Dest::RealScalar& tol_error, Index L)
		{
			using numext::abs;
			using numext::sqrt;
			typedef typename Dest::RealScalar RealScalar;
			typedef typename Dest::Scalar Scalar;
			Index N = rhs.size();
			L = L < x.rows() ? L : x.rows();

			Index k = 0;

			RealScalar tol = tol_error;
			Index maxIters = iters;

			typedef Matrix<Scalar, Dynamic, 1> VectorType;
			typedef Matrix<Scalar, Dynamic, Dynamic, ColMajor> DenseMatrixType;

			DenseMatrixType rHat(N, L + 1);
			DenseMatrixType uHat(N, L + 1);

			// We start with an initial guess x_0 and let us set r_0 as (residual calculated from x_0)
			rHat.col(0) = rhs - mat * precond.solve(x);  // r_0
			// rShadow is arbritary, but must never be orthogonal to any residual.
			// VectorType rShadow = rHat.col(0);
			VectorType rShadow = VectorType::Random(N);

			VectorType x_prime = x;
			x.setZero();
			VectorType b_prime = rHat.col(0);

			// Other vectors and scalars initialization
			Scalar rho0 = 1.0;
			Scalar alpha = 1.0;
			Scalar omega = 1.0;

			uHat.col(0).setZero();

			bool bicg_convergence = false;

			RealScalar zeta0 = rhs.norm();
			if(zeta0 == 0.0){
				x.setZero();
				return true;
			}			
			RealScalar zeta = rHat.col(0).norm();
			RealScalar Mx = zeta;
			RealScalar Mr = zeta;

			//Keep track of the solution with the lowest residual
			RealScalar zeta_min = zeta;
			VectorType x_min = x_prime+x;

			// Criterion for when to apply the group-wise update, conform ref 3.
			const RealScalar delta = 0.01;

			bool compute_res = false;
			bool update_app = false;
			//Index restarts = 0;

			while (zeta > tol * zeta0 && k < maxIters)
			{
				rho0 *= -omega;

				for (Index j = 0; j < L; ++j)
				{
					Scalar rho1 = rShadow.dot(rHat.col(j));

					if (!(numext::isfinite)(rho1) || rho0 == 0.0)
					{
						//We cannot continue computing, return the best solution found.
						zeta=zeta_min;
						x=x_min;
						tol_error = zeta / zeta0;
						iters = k;
						x = precond.solve(x);
						if(zeta < tol * zeta0 ){
							return true;
						}else{
							std::cout<<"aaaaaaaaa"<<std::endl;
							return false;
						}
					}

					// if (abs(rho1) < NumTraits<Scalar>::epsilon() * zeta0)
					// {
					// 	// The new residual vector became too orthogonal to the arbitrarily chosen direction r0
					// 	// Let's restart with a new r0:
					// 	rHat.col(j)  = rhs - mat * precond.solve(x);
					// 	rShadow = rHat.col(j);
					// 	rho1 = zeta0 = rHat.col(j).norm();

					// 	if (restarts++ == 0)
					// 	{
					// 		k = 0;
					// 	}
					// }

					Scalar beta = alpha * (rho1 / rho0);
					rho0 = rho1;
					// Update search directions
					uHat.leftCols(j + 1) = rHat.leftCols(j + 1) - beta * uHat.leftCols(j + 1);
					uHat.col(j + 1) = mat * precond.solve(uHat.col(j));
					alpha = rho1 / (rShadow.dot(uHat.col(j + 1)));
					// Update residuals
					rHat.leftCols(j + 1) -= alpha * uHat.block(0, 1, N, j + 1);
					rHat.col(j + 1) = mat * precond.solve(rHat.col(j));
					// Complete BiCG iteration by updating x
					x += alpha * uHat.col(0);
					// Check for early exit
					zeta = rHat.col(0).norm();

					// if (zeta < tol * zeta0)
					// {
					// 	/*
					// 		Convergence was achieved during BiCG step.
					// 		Without this check BiCGStab(L) fails for trivial matrices, such as when the preconditioner already is
					// 		the inverse, or the input matrix is identity.
					// 	*/
					// 	bicg_convergence = true;
					// 	break;
					// }
					if(zeta<zeta_min){
						//We found an x with lower residual, keep this one.
						x_min=x+x_prime;
						zeta_min=zeta;
					}					
				}

				if (bicg_convergence == false)
				{
					/*
						The polynomial/minimize residual step.

						QR Householder method for argmin is more stable than (modified) Gram-Schmidt, in the sense that there is
						less loss of orthogonality. It is more accurate than solving the normal equations, since the normal equations
						scale with condition number squared.
					*/
					VectorType gamma = (rHat.rightCols(L)).householderQr().solve(rHat.col(0));
					x += rHat.leftCols(L) * gamma;
					rHat.col(0) -= rHat.rightCols(L) * gamma;
					uHat.col(0) -= uHat.rightCols(L) * gamma;
					zeta = rHat.col(0).norm();
					omega = gamma(L - 1);
				}

				k++;

				/*
					Reliable update part

					The recursively computed residual can deviate from the actual residual after several iterations. However,
					computing the residual from the definition costs extra MVs and should not be done at each iteration. The reliable
					update strategy computes the true residual from the definition: r=b-A*x at strategic intervals. Furthermore a
					"group wise update" strategy is used to combine updates, which improves accuracy.
				*/

				// Maximum norm of residuals since last update of x.
				Mx = (std::max)(Mx, zeta); 
				// Maximum norm of residuals since last computation of the true residual. 
				Mr = (std::max)(Mr,zeta);  

				if (zeta < delta * zeta0 && zeta0 <= Mx)
				{
					update_app = true;
				}

				if (update_app || (zeta < delta * Mr && zeta0 <= Mr))
				{
					compute_res = true;
				}

				if (bicg_convergence)
				{
					update_app = true;
					compute_res = true;
					bicg_convergence = false;
				}

				if (compute_res)
				{
					//Explicitly compute residual from the definition
					rHat.col(0) = b_prime - mat * precond.solve(x);
					zeta = rHat.col(0).norm();
					Mr = zeta;

					if (update_app)
					{
						// After the group wise update, the original problem is translated to a shifted one.
						x_prime += x;
						x.setZero();
						b_prime = rHat.col(0);
						Mx = zeta;
					}
				}
				if(zeta<zeta_min){
					//We found an x with lower residual, keep this one.
					x_min=x+x_prime;
					zeta_min=zeta;
				}

				compute_res = false;
				update_app = false;
			}

			// Convert internal variable to the true solution vector x
			x = x_min;
			zeta = zeta_min;
			tol_error = zeta / zeta0;
			iters = k;
			x = precond.solve(x);
			RealScalar res=(mat*x-rhs).norm()/rhs.norm();
			std::cout<<"tol: "<<tol<<"res: "<<res<<std::endl;
			if(!(numext::isfinite)(res)){
				std::cout<<x<<std::endl;
			}
			return true;
		}

	}  // namespace internal

	template <typename _MatrixType, typename _Preconditioner = DiagonalPreconditioner<typename _MatrixType::Scalar> >
	class BiCGSTABL;

	namespace internal
	{

		template <typename _MatrixType, typename _Preconditioner>
		struct traits<Eigen::BiCGSTABL<_MatrixType, _Preconditioner> >
		{
			typedef _MatrixType MatrixType;
			typedef _Preconditioner Preconditioner;
		};

	}  // namespace internal

	template <typename _MatrixType, typename _Preconditioner>
	class BiCGSTABL : public IterativeSolverBase<BiCGSTABL<_MatrixType, _Preconditioner> >
	{
			typedef IterativeSolverBase<BiCGSTABL> Base;
			using Base::m_error;
			using Base::m_info;
			using Base::m_isInitialized;
			using Base::m_iterations;
			using Base::matrix;
			Index m_L;

		public:
			typedef _MatrixType MatrixType;
			typedef typename MatrixType::Scalar Scalar;
			typedef typename MatrixType::RealScalar RealScalar;
			typedef _Preconditioner Preconditioner;

		public:
			/** Default constructor. */
			BiCGSTABL() : m_L(2){;}

			/**
			Initialize the solver with matrix \a A for further \c Ax=b solving.

			This constructor is a shortcut for the default constructor followed
			by a call to compute().

			\warning this class stores a reference to the matrix A as well as some
			precomputed values that depend on it. Therefore, if \a A is changed
			this class becomes invalid. Call compute() to update it with the new
			matrix A, or modify a copy of A.
			*/
			template <typename MatrixDerived>
			explicit BiCGSTABL(const EigenBase<MatrixDerived>& A) : Base(A.derived()),m_L(2){;}

			/** \internal */
			/** Loops over the number of columns of b and does the following:
				1. sets the tolerence and maxIterations
				2. Calls the function that has the core solver routine
			*/
			template <typename Rhs, typename Dest>
			void _solve_vector_with_guess_impl(const Rhs& b, Dest& x) const
			{
				m_iterations = Base::maxIterations();
				m_error = Base::m_tolerance;

				bool ret = internal::bicgstabl(matrix(), b, x, Base::m_preconditioner, m_iterations, m_error, m_L);

				m_info = (!ret) ? NumericalIssue : m_error <= Base::m_tolerance ? Success : NoConvergence;
				if(m_info!=Success){
					std::cout<<"m_info: "<<m_info<<std::endl;
					std::cout<<"m_error: "<<m_error<<std::endl;
					std::cout<<"ret: "<<ret<<std::endl;
					m_info=Success;
				}
			}

			/** Sets the parameter L, indicating the amount of minimize residual steps are used. Default: 2 */
			void setL(Index L)
			{
				if (L < 1)
				{
					L = 2;
				}

				m_L = L;
			}

	};

}  // namespace Eigen

#endif /* EIGEN_BICGSTABL_H */
