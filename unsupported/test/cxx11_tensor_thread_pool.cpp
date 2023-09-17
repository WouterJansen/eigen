// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2014 Benoit Steiner <benoit.steiner.goog@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define EIGEN_USE_THREADS


#include "main.h"
#include <iostream>
#include <Eigen/CXX11/Tensor>
#include "AnnoyingScalar.h"

using Eigen::Tensor;

class TestAllocator : public Allocator {
 public:
  ~TestAllocator() EIGEN_OVERRIDE {}
  EIGEN_DEVICE_FUNC void* allocate(size_t num_bytes) const EIGEN_OVERRIDE {
    const_cast<TestAllocator*>(this)->alloc_count_++;
    return internal::aligned_malloc(num_bytes);
  }
  EIGEN_DEVICE_FUNC void deallocate(void* buffer) const EIGEN_OVERRIDE {
    const_cast<TestAllocator*>(this)->dealloc_count_++;
    internal::aligned_free(buffer);
  }

  int alloc_count() const { return alloc_count_; }
  int dealloc_count() const { return dealloc_count_; }

 private:
  int alloc_count_ = 0;
  int dealloc_count_ = 0;
};

void test_multithread_elementwise()
{
  Tensor<float, 3> in1(200, 30, 70);
  Tensor<float, 3> in2(200, 30, 70);
  Tensor<double, 3> out(200, 30, 70);

  in1.setRandom();
  in2.setRandom();

  Eigen::ThreadPool tp(internal::random<int>(3, 11));
  Eigen::ThreadPoolDevice thread_pool_device(&tp, internal::random<int>(3, 11));
  out.device(thread_pool_device) = (in1 + in2 * 3.14f).cast<double>();

  for (int i = 0; i < 200; ++i) {
    for (int j = 0; j < 30; ++j) {
      for (int k = 0; k < 70; ++k) {
        VERIFY_IS_APPROX(out(i, j, k), static_cast<double>(in1(i, j, k) + in2(i, j, k) * 3.14f));
      }
    }
  }
}

void test_async_multithread_elementwise()
{
  Tensor<float, 3> in1(200, 30, 70);
  Tensor<float, 3> in2(200, 30, 70);
  Tensor<double, 3> out(200, 30, 70);

  in1.setRandom();
  in2.setRandom();

  Eigen::ThreadPool tp(internal::random<int>(3, 11));
  Eigen::ThreadPoolDevice thread_pool_device(&tp, internal::random<int>(3, 11));

  Eigen::Barrier b(1);
  out.device(thread_pool_device, [&b]() { b.Notify(); }) = (in1 + in2 * 3.14f).cast<double>();
  b.Wait();

  for (int i = 0; i < 200; ++i) {
    for (int j = 0; j < 30; ++j) {
      for (int k = 0; k < 70; ++k) {
        VERIFY_IS_APPROX(out(i, j, k), static_cast<double>(in1(i, j, k) + in2(i, j, k) * 3.14f));
      }
    }
  }
}

void test_multithread_compound_assignment()
{
  Tensor<float, 3> in1(2,3,7);
  Tensor<float, 3> in2(2,3,7);
  Tensor<float, 3> out(2,3,7);

  in1.setRandom();
  in2.setRandom();

  Eigen::ThreadPool tp(internal::random<int>(3, 11));
  Eigen::ThreadPoolDevice thread_pool_device(&tp, internal::random<int>(3, 11));
  out.device(thread_pool_device) = in1;
  out.device(thread_pool_device) += in2 * 3.14f;

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 7; ++k) {
        VERIFY_IS_APPROX(out(i,j,k), in1(i,j,k) + in2(i,j,k) * 3.14f);
      }
    }
  }
}

template<int DataLayout>
void test_multithread_contraction()
{
  Tensor<float, 4, DataLayout> t_left(30, 50, 37, 31);
  Tensor<float, 5, DataLayout> t_right(37, 31, 70, 2, 10);
  Tensor<float, 5, DataLayout> t_result(30, 50, 70, 2, 10);

  t_left.setRandom();
  t_right.setRandom();

  // this contraction should be equivalent to a single matrix multiplication
  typedef Tensor<float, 1>::DimensionPair DimPair;
  Eigen::array<DimPair, 2> dims({{DimPair(2, 0), DimPair(3, 1)}});

  typedef Map<Matrix<float, Dynamic, Dynamic, DataLayout>> MapXf;
  MapXf m_left(t_left.data(), 1500, 1147);
  MapXf m_right(t_right.data(), 1147, 1400);
  Matrix<float, Dynamic, Dynamic, DataLayout> m_result(1500, 1400);

  Eigen::ThreadPool tp(4);
  Eigen::ThreadPoolDevice thread_pool_device(&tp, 4);

  // compute results by separate methods
  t_result.device(thread_pool_device) = t_left.contract(t_right, dims);
  m_result = m_left * m_right;

 for (ptrdiff_t i = 0; i < t_result.size(); i++) {
    VERIFY(&t_result.data()[i] != &m_result.data()[i]);
    if (fabsf(t_result(i) - m_result(i)) < 1e-4f) {
      continue;
    }
    if (Eigen::internal::isApprox(t_result(i), m_result(i), 1e-4f)) {
      continue;
    }
    std::cout << "mismatch detected at index " << i << ": " << t_result(i)
              << " vs " <<  m_result(i) << std::endl;
    assert(false);
  }
}

template<int DataLayout>
void test_contraction_corner_cases()
{
  Tensor<float, 2, DataLayout> t_left(32, 500);
  Tensor<float, 2, DataLayout> t_right(32, 28*28);
  Tensor<float, 2, DataLayout> t_result(500, 28*28);

  t_left = (t_left.constant(-0.5f) + t_left.random()) * 2.0f;
  t_right = (t_right.constant(-0.6f) + t_right.random()) * 2.0f;
  t_result = t_result.constant(NAN);

  // this contraction should be equivalent to a single matrix multiplication
  typedef Tensor<float, 1>::DimensionPair DimPair;
  Eigen::array<DimPair, 1> dims{{DimPair(0, 0)}};

  typedef Map<Matrix<float, Dynamic, Dynamic, DataLayout>> MapXf;
  MapXf m_left(t_left.data(), 32, 500);
  MapXf m_right(t_right.data(), 32, 28*28);
  Matrix<float, Dynamic, Dynamic, DataLayout> m_result(500, 28*28);

  Eigen::ThreadPool tp(12);
  Eigen::ThreadPoolDevice thread_pool_device(&tp, 12);

  // compute results by separate methods
  t_result.device(thread_pool_device) = t_left.contract(t_right, dims);
  m_result = m_left.transpose() * m_right;

  for (ptrdiff_t i = 0; i < t_result.size(); i++) {
    assert(!(numext::isnan)(t_result.data()[i]));
    if (fabsf(t_result.data()[i] - m_result.data()[i]) >= 1e-4f) {
      std::cout << "mismatch detected at index " << i << " : " << t_result.data()[i] << " vs " <<  m_result.data()[i] << std::endl;
      assert(false);
    }
  }

  t_left.resize(32, 1);
  t_left = (t_left.constant(-0.5f) + t_left.random()) * 2.0f;
  t_result.resize (1, 28*28);
  t_result = t_result.constant(NAN);
  t_result.device(thread_pool_device) = t_left.contract(t_right, dims);
  new(&m_left) MapXf(t_left.data(), 32, 1);
  m_result = m_left.transpose() * m_right;
  for (ptrdiff_t i = 0; i < t_result.size(); i++) {
    assert(!(numext::isnan)(t_result.data()[i]));
    if (fabsf(t_result.data()[i] - m_result.data()[i]) >= 1e-4f) {
      std::cout << "mismatch detected: " << t_result.data()[i] << " vs " <<  m_result.data()[i] << std::endl;
      assert(false);
    }
  }

  t_left.resize(32, 500);
  t_right.resize(32, 4);
  t_left = (t_left.constant(-0.5f) + t_left.random()) * 2.0f;
  t_right = (t_right.constant(-0.6f) + t_right.random()) * 2.0f;
  t_result.resize (500, 4);
  t_result = t_result.constant(NAN);
  t_result.device(thread_pool_device) = t_left.contract(t_right, dims);
  new(&m_left) MapXf(t_left.data(), 32, 500);
  new(&m_right) MapXf(t_right.data(), 32, 4);
  m_result = m_left.transpose() * m_right;
  for (ptrdiff_t i = 0; i < t_result.size(); i++) {
    assert(!(numext::isnan)(t_result.data()[i]));
    if (fabsf(t_result.data()[i] - m_result.data()[i]) >= 1e-4f) {
      std::cout << "mismatch detected: " << t_result.data()[i] << " vs " <<  m_result.data()[i] << std::endl;
      assert(false);
    }
  }

  t_left.resize(32, 1);
  t_right.resize(32, 4);
  t_left = (t_left.constant(-0.5f) + t_left.random()) * 2.0f;
  t_right = (t_right.constant(-0.6f) + t_right.random()) * 2.0f;
  t_result.resize (1, 4);
  t_result = t_result.constant(NAN);
  t_result.device(thread_pool_device) = t_left.contract(t_right, dims);
  new(&m_left) MapXf(t_left.data(), 32, 1);
  new(&m_right) MapXf(t_right.data(), 32, 4);
  m_result = m_left.transpose() * m_right;
  for (ptrdiff_t i = 0; i < t_result.size(); i++) {
    assert(!(numext::isnan)(t_result.data()[i]));
    if (fabsf(t_result.data()[i] - m_result.data()[i]) >= 1e-4f) {
      std::cout << "mismatch detected: " << t_result.data()[i] << " vs " <<  m_result.data()[i] << std::endl;
      assert(false);
    }
  }
}

template<int DataLayout>
void test_multithread_contraction_agrees_with_singlethread() {
  int contract_size = internal::random<int>(1, 5000);

  Tensor<float, 3, DataLayout> left(internal::random<int>(1, 80),
                                    contract_size,
                                    internal::random<int>(1, 100));

  Tensor<float, 4, DataLayout> right(internal::random<int>(1, 25),
                                     internal::random<int>(1, 37),
                                     contract_size,
                                     internal::random<int>(1, 51));

  left.setRandom();
  right.setRandom();

  // add constants to shift values away from 0 for more precision
  left += left.constant(1.5f);
  right += right.constant(1.5f);

  typedef Tensor<float, 1>::DimensionPair DimPair;
  Eigen::array<DimPair, 1> dims({{DimPair(1, 2)}});

  Eigen::ThreadPool tp(internal::random<int>(2, 11));
  Eigen::ThreadPoolDevice thread_pool_device(&tp, internal::random<int>(2, 11));

  Tensor<float, 5, DataLayout> st_result;
  st_result = left.contract(right, dims);

  Tensor<float, 5, DataLayout> tp_result(st_result.dimensions());
  tp_result.device(thread_pool_device) = left.contract(right, dims);

  VERIFY(dimensions_match(st_result.dimensions(), tp_result.dimensions()));
  for (ptrdiff_t i = 0; i < st_result.size(); i++) {
    // if both of the values are very small, then do nothing (because the test will fail
    // due to numerical precision issues when values are small)
    if (numext::abs(st_result.data()[i] - tp_result.data()[i]) >= 1e-4f) {
      VERIFY_IS_APPROX(st_result.data()[i], tp_result.data()[i]);
    }
  }
}

// Apply Sqrt to all output elements.
struct SqrtOutputKernel {
  template <typename Index, typename Scalar>
  EIGEN_ALWAYS_INLINE void operator()(
      const internal::blas_data_mapper<Scalar, Index, ColMajor>& output_mapper,
      const TensorContractionParams&, Index, Index, Index num_rows,
      Index num_cols) const {
    for (int i = 0; i < num_rows; ++i) {
      for (int j = 0; j < num_cols; ++j) {
        output_mapper(i, j) = std::sqrt(output_mapper(i, j));
      }
    }
  }
};

template <int DataLayout>
static void test_multithread_contraction_with_output_kernel() {
  typedef Tensor<float, 1>::DimensionPair DimPair;

  const int num_threads = internal::random<int>(2, 11);
  ThreadPool threads(num_threads);
  Eigen::ThreadPoolDevice device(&threads, num_threads);

  Tensor<float, 4, DataLayout> t_left(30, 50, 8, 31);
  Tensor<float, 5, DataLayout> t_right(8, 31, 7, 20, 10);
  Tensor<float, 5, DataLayout> t_result(30, 50, 7, 20, 10);

  t_left.setRandom();
  t_right.setRandom();
  // Put trash in mat4 to verify contraction clears output memory.
  t_result.setRandom();

  // Add a little offset so that the results won't be close to zero.
  t_left += t_left.constant(1.0f);
  t_right += t_right.constant(1.0f);

  typedef Map<Eigen::Matrix<float, Dynamic, Dynamic, DataLayout>> MapXf;
  MapXf m_left(t_left.data(), 1500, 248);
  MapXf m_right(t_right.data(), 248, 1400);
  Eigen::Matrix<float, Dynamic, Dynamic, DataLayout> m_result(1500, 1400);

  // this contraction should be equivalent to a single matrix multiplication
  Eigen::array<DimPair, 2> dims({{DimPair(2, 0), DimPair(3, 1)}});

  // compute results by separate methods
  t_result.device(device) = t_left.contract(t_right, dims, SqrtOutputKernel());

  m_result = m_left * m_right;

  for (Index i = 0; i < t_result.dimensions().TotalSize(); i++) {
    VERIFY(&t_result.data()[i] != &m_result.data()[i]);
    VERIFY_IS_APPROX(t_result.data()[i], std::sqrt(m_result.data()[i]));
  }
}

template<int DataLayout>
void test_async_multithread_contraction_agrees_with_singlethread()
{
  int contract_size = internal::random<int>(100, 500);

  Tensor<float, 3, DataLayout> left(internal::random<int>(10, 40),
                                    contract_size,
                                    internal::random<int>(10, 40));

  Tensor<float, 4, DataLayout> right(
      internal::random<int>(1, 20), internal::random<int>(1, 20), contract_size,
      internal::random<int>(1, 20));

  left.setRandom();
  right.setRandom();

  // add constants to shift values away from 0 for more precision
  left += left.constant(1.5f);
  right += right.constant(1.5f);

  typedef Tensor<float, 1>::DimensionPair DimPair;
  Eigen::array<DimPair, 1> dims({{DimPair(1, 2)}});

  Eigen::ThreadPool tp(internal::random<int>(2, 11));
  Eigen::ThreadPoolDevice thread_pool_device(&tp, internal::random<int>(8, 32));

  Tensor<float, 5, DataLayout> st_result;
  st_result = left.contract(right, dims);

  Tensor<float, 5, DataLayout> tp_result(st_result.dimensions());

  Eigen::Barrier barrier(1);
  tp_result.device(thread_pool_device, [&barrier]() { barrier.Notify(); }) =
      left.contract(right, dims);
  barrier.Wait();

  VERIFY(dimensions_match(st_result.dimensions(), tp_result.dimensions()));
  for (ptrdiff_t i = 0; i < st_result.size(); i++) {
    // if both of the values are very small, then do nothing (because the test
    // will fail due to numerical precision issues when values are small)
    if (numext::abs(st_result.data()[i] - tp_result.data()[i]) >= 1e-4f) {
      VERIFY_IS_APPROX(st_result.data()[i], tp_result.data()[i]);
    }
  }
}

// We are triggering 'evalShardedByInnerDim' optimization.
template <int DataLayout>
static void test_sharded_by_inner_dim_contraction()
{
  typedef Tensor<float, 1>::DimensionPair DimPair;

  const int num_threads = internal::random<int>(4, 16);
  ThreadPool threads(num_threads);
  Eigen::ThreadPoolDevice device(&threads, num_threads);

  Tensor<float, 2, DataLayout> t_left(2, 10000);
  Tensor<float, 2, DataLayout> t_right(10000, 10);
  Tensor<float, 2, DataLayout> t_result(2, 10);

  t_left.setRandom();
  t_right.setRandom();
  // Put trash in t_result to verify contraction clears output memory.
  t_result.setRandom();

  // Add a little offset so that the results won't be close to zero.
  t_left += t_left.constant(1.0f);
  t_right += t_right.constant(1.0f);

  typedef Map<Eigen::Matrix<float, Dynamic, Dynamic, DataLayout>> MapXf;
  MapXf m_left(t_left.data(), 2, 10000);
  MapXf m_right(t_right.data(), 10000, 10);
  Eigen::Matrix<float, Dynamic, Dynamic, DataLayout> m_result(2, 10);

  // this contraction should be equivalent to a single matrix multiplication
  Eigen::array<DimPair, 1> dims({{DimPair(1, 0)}});

  // compute results by separate methods
  t_result.device(device) = t_left.contract(t_right, dims);
  m_result = m_left * m_right;

  for (Index i = 0; i < t_result.dimensions().TotalSize(); i++) {
    VERIFY_IS_APPROX(t_result.data()[i], m_result.data()[i]);
  }
}

// We are triggering 'evalShardedByInnerDim' optimization with output kernel.
template <int DataLayout>
static void test_sharded_by_inner_dim_contraction_with_output_kernel()
{
  typedef Tensor<float, 1>::DimensionPair DimPair;

  const int num_threads = internal::random<int>(4, 16);
  ThreadPool threads(num_threads);
  Eigen::ThreadPoolDevice device(&threads, num_threads);

  Tensor<float, 2, DataLayout> t_left(2, 10000);
  Tensor<float, 2, DataLayout> t_right(10000, 10);
  Tensor<float, 2, DataLayout> t_result(2, 10);

  t_left.setRandom();
  t_right.setRandom();
  // Put trash in t_result to verify contraction clears output memory.
  t_result.setRandom();

  // Add a little offset so that the results won't be close to zero.
  t_left += t_left.constant(1.0f);
  t_right += t_right.constant(1.0f);

  typedef Map<Eigen::Matrix<float, Dynamic, Dynamic, DataLayout>> MapXf;
  MapXf m_left(t_left.data(), 2, 10000);
  MapXf m_right(t_right.data(), 10000, 10);
  Eigen::Matrix<float, Dynamic, Dynamic, DataLayout> m_result(2, 10);

  // this contraction should be equivalent to a single matrix multiplication
  Eigen::array<DimPair, 1> dims({{DimPair(1, 0)}});

  // compute results by separate methods
  t_result.device(device) = t_left.contract(t_right, dims, SqrtOutputKernel());
  m_result = m_left * m_right;

  for (Index i = 0; i < t_result.dimensions().TotalSize(); i++) {
    VERIFY_IS_APPROX(t_result.data()[i], std::sqrt(m_result.data()[i]));
  }
}

// We are triggering 'evalShardedByInnerDim' optimization.
template <int DataLayout>
static void test_async_sharded_by_inner_dim_contraction()
{
  typedef Tensor<float, 1>::DimensionPair DimPair;

  const int num_threads = internal::random<int>(4, 16);
  ThreadPool threads(num_threads);
  Eigen::ThreadPoolDevice device(&threads, num_threads);

  Tensor<float, 2, DataLayout> t_left(2, 10000);
  Tensor<float, 2, DataLayout> t_right(10000, 10);
  Tensor<float, 2, DataLayout> t_result(2, 10);

  t_left.setRandom();
  t_right.setRandom();
  // Put trash in t_result to verify contraction clears output memory.
  t_result.setRandom();

  // Add a little offset so that the results won't be close to zero.
  t_left += t_left.constant(1.0f);
  t_right += t_right.constant(1.0f);

  typedef Map<Eigen::Matrix<float, Dynamic, Dynamic, DataLayout>> MapXf;
  MapXf m_left(t_left.data(), 2, 10000);
  MapXf m_right(t_right.data(), 10000, 10);
  Eigen::Matrix<float, Dynamic, Dynamic, DataLayout> m_result(2, 10);

  // this contraction should be equivalent to a single matrix multiplication
  Eigen::array<DimPair, 1> dims({{DimPair(1, 0)}});

  // compute results by separate methods
  Eigen::Barrier barrier(1);
  t_result.device(device, [&barrier]() { barrier.Notify(); }) =
      t_left.contract(t_right, dims);
  barrier.Wait();

  m_result = m_left * m_right;

  for (Index i = 0; i < t_result.dimensions().TotalSize(); i++) {
    VERIFY_IS_APPROX(t_result.data()[i], m_result.data()[i]);
  }
}

// We are triggering 'evalShardedByInnerDim' optimization with output kernel.
template <int DataLayout>
static void test_async_sharded_by_inner_dim_contraction_with_output_kernel()
{
  typedef Tensor<float, 1>::DimensionPair DimPair;

  const int num_threads = internal::random<int>(4, 16);
  ThreadPool threads(num_threads);
  Eigen::ThreadPoolDevice device(&threads, num_threads);

  Tensor<float, 2, DataLayout> t_left(2, 10000);
  Tensor<float, 2, DataLayout> t_right(10000, 10);
  Tensor<float, 2, DataLayout> t_result(2, 10);

  t_left.setRandom();
  t_right.setRandom();
  // Put trash in t_result to verify contraction clears output memory.
  t_result.setRandom();

  // Add a little offset so that the results won't be close to zero.
  t_left += t_left.constant(1.0f);
  t_right += t_right.constant(1.0f);

  typedef Map<Eigen::Matrix<float, Dynamic, Dynamic, DataLayout>> MapXf;
  MapXf m_left(t_left.data(), 2, 10000);
  MapXf m_right(t_right.data(), 10000, 10);
  Eigen::Matrix<float, Dynamic, Dynamic, DataLayout> m_result(2, 10);

  // this contraction should be equivalent to a single matrix multiplication
  Eigen::array<DimPair, 1> dims({{DimPair(1, 0)}});

  // compute results by separate methods
  Eigen::Barrier barrier(1);
  t_result.device(device, [&barrier]() { barrier.Notify(); }) =
      t_left.contract(t_right, dims, SqrtOutputKernel());
  barrier.Wait();
  m_result = m_left * m_right;

  for (Index i = 0; i < t_result.dimensions().TotalSize(); i++) {
    VERIFY_IS_APPROX(t_result.data()[i], std::sqrt(m_result.data()[i]));
  }
}

template<int DataLayout>
void test_full_contraction() {
  int contract_size1 = internal::random<int>(1, 500);
  int contract_size2 = internal::random<int>(1, 500);

  Tensor<float, 2, DataLayout> left(contract_size1,
                                    contract_size2);
  Tensor<float, 2, DataLayout> right(contract_size1,
                                    contract_size2);
  left.setRandom();
  right.setRandom();

  // add constants to shift values away from 0 for more precision
  left += left.constant(1.5f);
  right += right.constant(1.5f);

  typedef Tensor<float, 2>::DimensionPair DimPair;
  Eigen::array<DimPair, 2> dims({{DimPair(0, 0), DimPair(1, 1)}});

  Eigen::ThreadPool tp(internal::random<int>(2, 11));
  Eigen::ThreadPoolDevice thread_pool_device(&tp, internal::random<int>(2, 11));

  Tensor<float, 0, DataLayout> st_result;
  st_result = left.contract(right, dims);

  Tensor<float, 0, DataLayout> tp_result;
  tp_result.device(thread_pool_device) = left.contract(right, dims);

  VERIFY(dimensions_match(st_result.dimensions(), tp_result.dimensions()));
  // if both of the values are very small, then do nothing (because the test will fail
  // due to numerical precision issues when values are small)
  if (numext::abs(st_result() - tp_result()) >= 1e-4f) {
    VERIFY_IS_APPROX(st_result(), tp_result());
  }
}

template<int DataLayout>
void test_multithreaded_reductions() {
  const int num_threads = internal::random<int>(3, 11);
  ThreadPool thread_pool(num_threads);
  Eigen::ThreadPoolDevice thread_pool_device(&thread_pool, num_threads);

  const int num_rows = internal::random<int>(13, 732);
  const int num_cols = internal::random<int>(13, 732);
  Tensor<float, 2, DataLayout> t1(num_rows, num_cols);
  t1.setRandom();

  Tensor<float, 0, DataLayout> full_redux;
  full_redux = t1.sum();

  Tensor<float, 0, DataLayout> full_redux_tp;
  full_redux_tp.device(thread_pool_device) = t1.sum();

  // Check that the single threaded and the multi threaded reductions return
  // the same result.
  VERIFY_IS_APPROX(full_redux(), full_redux_tp());
}


void test_memcpy() {

  for (int i = 0; i < 5; ++i) {
    const int num_threads = internal::random<int>(3, 11);
    Eigen::ThreadPool tp(num_threads);
    Eigen::ThreadPoolDevice thread_pool_device(&tp, num_threads);

    const int size = internal::random<int>(13, 7632);
    Tensor<float, 1> t1(size);
    t1.setRandom();
    std::vector<float> result(size);
    thread_pool_device.memcpy(&result[0], t1.data(), size*sizeof(float));
    for (int j = 0; j < size; j++) {
      VERIFY_IS_EQUAL(t1(j), result[j]);
    }
  }
}


void test_multithread_random()
{
  Eigen::ThreadPool tp(2);
  Eigen::ThreadPoolDevice device(&tp, 2);
  Tensor<float, 1> t(1 << 20);
  t.device(device) = t.random<Eigen::internal::NormalRandomGenerator<float>>();
}

template<int DataLayout>
void test_multithread_shuffle(Allocator* allocator)
{
  Tensor<float, 4, DataLayout> tensor(17,5,7,11);
  tensor.setRandom();

  const int num_threads = internal::random<int>(2, 11);
  ThreadPool threads(num_threads);
  Eigen::ThreadPoolDevice device(&threads, num_threads, allocator);

  Tensor<float, 4, DataLayout> shuffle(7,5,11,17);
  array<ptrdiff_t, 4> shuffles = {{2,1,3,0}};
  shuffle.device(device) = tensor.shuffle(shuffles);

  for (int i = 0; i < 17; ++i) {
    for (int j = 0; j < 5; ++j) {
      for (int k = 0; k < 7; ++k) {
        for (int l = 0; l < 11; ++l) {
          VERIFY_IS_EQUAL(tensor(i,j,k,l), shuffle(k,j,l,i));
        }
      }
    }
  }
}

void test_threadpool_allocate(TestAllocator* allocator)
{
  const int num_threads = internal::random<int>(2, 11);
  const int num_allocs = internal::random<int>(2, 11);
  ThreadPool threads(num_threads);
  Eigen::ThreadPoolDevice device(&threads, num_threads, allocator);

  for (int a = 0; a < num_allocs; ++a) {
    void* ptr = device.allocate(512);
    device.deallocate(ptr);
  }
  VERIFY(allocator != NULL);
  VERIFY_IS_EQUAL(allocator->alloc_count(), num_allocs);
  VERIFY_IS_EQUAL(allocator->dealloc_count(), num_allocs);
}

template<int DataLayout>
void test_multithread_contraction_with_scalar_initialization()
{

#ifndef EIGEN_TEST_ANNOYING_SCALAR_DONT_THROW
  AnnoyingScalar::dont_throw = true;
#endif

  AnnoyingScalar::instances = 0;

  {
    Tensor<AnnoyingScalar, 2, DataLayout> A(128, 64);
    Tensor<AnnoyingScalar, 2, DataLayout> B(64, 32);
    Tensor<AnnoyingScalar, 2, DataLayout> result(A.dimension(0), B.dimension(1));

    result.setZero();

    Matrix<float, Dynamic, Dynamic, DataLayout> mat_A(A.dimension(0), A.dimension(1));
    Matrix<float, Dynamic, Dynamic, DataLayout> mat_B(B.dimension(0), B.dimension(1));

    std::default_random_engine dre(time(0));
    std::uniform_real_distribution<float> distro(0, 1);

    for (Index i = 0; i < mat_A.rows(); ++i) {
      for (Index j = 0; j < mat_A.cols(); ++j) {
        float val = distro(dre);
        mat_A(i, j) = val;
        A(i, j) = val;
      }
    }

    for (Index i = 0; i < mat_B.rows(); ++i) {
      for (Index j = 0; j < mat_B.cols(); ++j) {
        float val = distro(dre);
        mat_B(i, j) = val;
        B(i, j) = val;
      }
    }

    Eigen::ThreadPool tp(internal::random<int>(2, 11));
    Eigen::ThreadPoolDevice thread_pool_device(&tp, 4);

    typedef Tensor<AnnoyingScalar, 1>::DimensionPair Annoying_DimPair;
    Eigen::array<Annoying_DimPair, 1> dims = {{Annoying_DimPair(1, 0)}};
    typedef TensorEvaluator<decltype(A.contract(B, dims)), ThreadPoolDevice> Evaluator;
    Evaluator eval(A.contract(B, dims), thread_pool_device);
    eval.evalTo(result.data());

    Matrix<float, Dynamic, Dynamic, DataLayout> mat_result(A.dimension(0), B.dimension(1));
    mat_result = mat_A * mat_B;

    for (Index i = 0; i < mat_result.rows(); ++i) {
      for (Index j = 0; j < mat_result.cols(); ++j) {
        VERIFY_IS_APPROX(mat_result(i, j), *result(i, j).v);
      }
    }
  }

  VERIFY(AnnoyingScalar::instances == 0 && "memory leak detected in contraction on ThreadPoolDevice");

}

template<int DataLayout>
static void test_scalar_initialization_multidims()
{

#ifndef EIGEN_TEST_ANNOYING_SCALAR_DONT_THROW
  AnnoyingScalar::dont_throw = true;
#endif

  {
    Tensor<AnnoyingScalar, 3, DataLayout> A(2, 2, 2);
    Tensor<AnnoyingScalar, 4, DataLayout> B(2, 2, 2, 2);
    Tensor<AnnoyingScalar, 3, DataLayout> result(2, 2, 2);

    std::default_random_engine dre(time(0));
    std::uniform_real_distribution<float> distro(0, 1);

    for (Index i = 0; i < A.dimension(0); ++i) {
      for (Index j = 0; j < A.dimension(1); ++j) {
        for (Index k = 0; k < A.dimension(2); ++k) {
          A(i, j, k) = distro(dre);
        }
      }
    }
    for (Index i = 0; i < B.dimension(0); ++i) {
      for (Index j = 0; j < B.dimension(1); ++j) {
        for (Index k = 0; k < B.dimension(2); ++k) {
          for (Index l = 0; l < B.dimension(3); ++l) {
            B(i, j, k, l) = distro(dre);
          }
        }
      }
    }
    result.setZero();

    Eigen::ThreadPool tp(internal::random<int>(2, 11));
    Eigen::ThreadPoolDevice thread_pool_device(&tp, 4);

    typedef Tensor<AnnoyingScalar, 1>::DimensionPair Annoying_DimPair;
    Eigen::array<Annoying_DimPair, 2> dims = {{Annoying_DimPair(1, 2), Annoying_DimPair(2, 3)}};
    typedef TensorEvaluator<decltype(A.contract(B, dims)), ThreadPoolDevice> Evaluator;
    Evaluator eval(A.contract(B, dims), thread_pool_device);
    eval.evalTo(result.data());
    EIGEN_STATIC_ASSERT(Evaluator::NumDims==3ul, YOU_MADE_A_PROGRAMMING_MISTAKE);
    VERIFY_IS_EQUAL(eval.dimensions()[0], 2);
    VERIFY_IS_EQUAL(eval.dimensions()[1], 2);
    VERIFY_IS_EQUAL(eval.dimensions()[2], 2);

    VERIFY_IS_APPROX(result(0,0,0), A(0,0,0)*B(0,0,0,0) + A(0,1,0)*B(0,0,1,0) +
                                  A(0,0,1)*B(0,0,0,1) + A(0,1,1)*B(0,0,1,1));
    VERIFY_IS_APPROX(result(0,0,1), A(0,0,0)*B(0,1,0,0) + A(0,1,0)*B(0,1,1,0) +
                                  A(0,0,1)*B(0,1,0,1) + A(0,1,1)*B(0,1,1,1));
    VERIFY_IS_APPROX(result(0,1,0), A(0,0,0)*B(1,0,0,0) + A(0,1,0)*B(1,0,1,0) +
                                  A(0,0,1)*B(1,0,0,1) + A(0,1,1)*B(1,0,1,1));
    VERIFY_IS_APPROX(result(0,1,1), A(0,0,0)*B(1,1,0,0) + A(0,1,0)*B(1,1,1,0) +
                                  A(0,0,1)*B(1,1,0,1) + A(0,1,1)*B(1,1,1,1));
    VERIFY_IS_APPROX(result(1,0,0), A(1,0,0)*B(0,0,0,0) + A(1,1,0)*B(0,0,1,0) +
                                  A(1,0,1)*B(0,0,0,1) + A(1,1,1)*B(0,0,1,1));
    VERIFY_IS_APPROX(result(1,0,1), A(1,0,0)*B(0,1,0,0) + A(1,1,0)*B(0,1,1,0) +
                                  A(1,0,1)*B(0,1,0,1) + A(1,1,1)*B(0,1,1,1));
    VERIFY_IS_APPROX(result(1,1,0), A(1,0,0)*B(1,0,0,0) + A(1,1,0)*B(1,0,1,0) +
                                  A(1,0,1)*B(1,0,0,1) + A(1,1,1)*B(1,0,1,1));
    VERIFY_IS_APPROX(result(1,1,1), A(1,0,0)*B(1,1,0,0) + A(1,1,0)*B(1,1,1,0) +
                                  A(1,0,1)*B(1,1,0,1) + A(1,1,1)*B(1,1,1,1));
  }

  VERIFY(AnnoyingScalar::instances==0 && "memory leak detected in contraction on ThreadPoolDevice");
}

template<int DataLayout>
static void test_scalar_initialization_in_large_contraction()
{

#ifndef EIGEN_TEST_ANNOYING_SCALAR_DONT_THROW
  AnnoyingScalar::dont_throw = true;
#endif

  AnnoyingScalar::instances = 0;

  {
    Tensor<AnnoyingScalar, 4, DataLayout> A(20, 45, 8, 31);
    Tensor<AnnoyingScalar, 5, DataLayout> B(8, 31, 7, 3, 5);
    Tensor<AnnoyingScalar, 5, DataLayout> result(20, 45, 7, 3, 5);

    result.setZero();

    // Tensor<AnnoyingScalar>.setRandom() causes overloaded ambiguous calls
    std::default_random_engine dre(time(0));
    std::uniform_real_distribution<float> distro(.0f, 10.f);

    for (Index i = 0; i < A.dimension(0); ++i) {
      for (Index j = 0; j < A.dimension(1); ++j) {
        for (Index k = 0; k < A.dimension(2); ++k) {
          for (Index l = 0; l < A.dimension(3); ++l) {
            A(i, j, k, l) = distro(dre);
          }
        }
      }
    }

    for (Index i = 0; i < B.dimension(0); ++i) {
      for (Index j = 0; j < B.dimension(1); ++j) {
        for (Index k = 0; k < B.dimension(2); ++k) {
          for (Index l = 0; l < B.dimension(3); ++l) {
            for (Index m = 0; m < B.dimension(4); ++m) {
              B(i, j, k, l, m) = distro(dre);
            }
          }
        }
      }
    }

    Eigen::ThreadPool tp(internal::random<int>(2, 11));
    Eigen::ThreadPoolDevice thread_pool_device(&tp, 4);

    typedef Tensor<AnnoyingScalar, 1>::DimensionPair Annoying_DimPair;
    Eigen::array<Annoying_DimPair, 2> dims({{Annoying_DimPair(2, 0), Annoying_DimPair(3, 1)}});
    typedef TensorEvaluator<decltype(A.contract(B, dims)), ThreadPoolDevice> Evaluator;
    Evaluator eval(A.contract(B, dims), thread_pool_device);
    eval.evalTo(result.data());

  }

  VERIFY(AnnoyingScalar::instances == 0 && "memory leak detected in contraction on ThreadPoolDevice");

}

template<int DataLayout>
static void test_scalar_initialization_in_large_contraction_allocator(TestAllocator* allocator)
{

#ifndef EIGEN_TEST_ANNOYING_SCALAR_DONT_THROW
  AnnoyingScalar::dont_throw = true;
#endif

  AnnoyingScalar::instances = 0;

  {
    Tensor<AnnoyingScalar, 4, DataLayout> A(20, 45, 8, 31);
    Tensor<AnnoyingScalar, 5, DataLayout> B(8, 31, 7, 3, 5);
    Tensor<AnnoyingScalar, 5, DataLayout> result(20, 45, 7, 3, 5);

    result.setZero();

    // Tensor<AnnoyingScalar>.setRandom() causes overloaded ambiguous calls
    std::default_random_engine dre(time(0));
    std::uniform_real_distribution<float> distro(.0f, 10.f);

    for (Index i = 0; i < A.dimension(0); ++i) {
      for (Index j = 0; j < A.dimension(1); ++j) {
        for (Index k = 0; k < A.dimension(2); ++k) {
          for (Index l = 0; l < A.dimension(3); ++l) {
            A(i, j, k, l) = distro(dre);
          }
        }
      }
    }

    for (Index i = 0; i < B.dimension(0); ++i) {
      for (Index j = 0; j < B.dimension(1); ++j) {
        for (Index k = 0; k < B.dimension(2); ++k) {
          for (Index l = 0; l < B.dimension(3); ++l) {
            for (Index m = 0; m < B.dimension(4); ++m) {
              B(i, j, k, l, m) = distro(dre);
            }
          }
        }
      }
    }

    const int num_threads = internal::random<int>(2, 11);
    ThreadPool threads(num_threads);
    Eigen::ThreadPoolDevice thread_pool_device(&threads, num_threads, allocator);

    typedef Tensor<AnnoyingScalar, 1>::DimensionPair Annoying_DimPair;
    Eigen::array<Annoying_DimPair, 2> dims({{Annoying_DimPair(2, 0), Annoying_DimPair(3, 1)}});
    typedef TensorEvaluator<decltype(A.contract(B, dims)), ThreadPoolDevice> Evaluator;
    Evaluator eval(A.contract(B, dims), thread_pool_device);
    eval.evalTo(result.data());

  }

  VERIFY(AnnoyingScalar::instances == 0 && "memory leak detected in contraction on ThreadPoolDevice");

}

EIGEN_DECLARE_TEST(cxx11_tensor_thread_pool)
{
  CALL_SUBTEST_1(test_multithread_elementwise());
  CALL_SUBTEST_1(test_async_multithread_elementwise());
  CALL_SUBTEST_1(test_multithread_compound_assignment());

  CALL_SUBTEST_2(test_multithread_contraction<ColMajor>());
  CALL_SUBTEST_2(test_multithread_contraction<RowMajor>());

  CALL_SUBTEST_3(test_multithread_contraction_agrees_with_singlethread<ColMajor>());
  CALL_SUBTEST_3(test_multithread_contraction_agrees_with_singlethread<RowMajor>());
  CALL_SUBTEST_3(test_multithread_contraction_with_output_kernel<ColMajor>());
  CALL_SUBTEST_3(test_multithread_contraction_with_output_kernel<RowMajor>());

  CALL_SUBTEST_4(test_async_multithread_contraction_agrees_with_singlethread<ColMajor>());
  CALL_SUBTEST_4(test_async_multithread_contraction_agrees_with_singlethread<RowMajor>());

  // Test EvalShardedByInnerDimContext parallelization strategy.
  CALL_SUBTEST_5(test_sharded_by_inner_dim_contraction<ColMajor>());
  CALL_SUBTEST_5(test_sharded_by_inner_dim_contraction<RowMajor>());
  CALL_SUBTEST_5(test_sharded_by_inner_dim_contraction_with_output_kernel<ColMajor>());
  CALL_SUBTEST_5(test_sharded_by_inner_dim_contraction_with_output_kernel<RowMajor>());

  CALL_SUBTEST_6(test_async_sharded_by_inner_dim_contraction<ColMajor>());
  CALL_SUBTEST_6(test_async_sharded_by_inner_dim_contraction<RowMajor>());
  CALL_SUBTEST_6(test_async_sharded_by_inner_dim_contraction_with_output_kernel<ColMajor>());
  CALL_SUBTEST_6(test_async_sharded_by_inner_dim_contraction_with_output_kernel<RowMajor>());

  // Exercise various cases that have been problematic in the past.
  CALL_SUBTEST_7(test_contraction_corner_cases<ColMajor>());
  CALL_SUBTEST_7(test_contraction_corner_cases<RowMajor>());

  CALL_SUBTEST_8(test_full_contraction<ColMajor>());
  CALL_SUBTEST_8(test_full_contraction<RowMajor>());

  CALL_SUBTEST_9(test_multithreaded_reductions<ColMajor>());
  CALL_SUBTEST_9(test_multithreaded_reductions<RowMajor>());

  CALL_SUBTEST_10(test_memcpy());
  CALL_SUBTEST_10(test_multithread_random());

  TestAllocator test_allocator;
  CALL_SUBTEST_11(test_multithread_shuffle<ColMajor>(NULL));
  CALL_SUBTEST_11(test_multithread_shuffle<RowMajor>(&test_allocator));
  CALL_SUBTEST_11(test_threadpool_allocate(&test_allocator));

  // testing with AnnoyingScalar

  CALL_SUBTEST_12(test_multithread_contraction_with_scalar_initialization<ColMajor>());
  CALL_SUBTEST_12(test_multithread_contraction_with_scalar_initialization<RowMajor>());

  CALL_SUBTEST_13(test_scalar_initialization_multidims<ColMajor>());
  CALL_SUBTEST_13(test_scalar_initialization_multidims<RowMajor>());

  CALL_SUBTEST_14(test_scalar_initialization_in_large_contraction<ColMajor>());
  CALL_SUBTEST_14(test_scalar_initialization_in_large_contraction<RowMajor>());

  CALL_SUBTEST_15(test_scalar_initialization_in_large_contraction_allocator<ColMajor>(&test_allocator));
  CALL_SUBTEST_15(test_scalar_initialization_in_large_contraction_allocator<RowMajor>(&test_allocator));

  // Force CMake to split this test.
  // EIGEN_SUFFIXES;1;2;3;4;5;6;7;8;9;10;11;12;13;14;15
}
