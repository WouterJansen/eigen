// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2022 Shawn Li <tokinobug@163.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_HEU_GABASE_H
#define EIGEN_HEU_GABASE_H

#include <tuple>
#include <vector>
#include <list>
#include <cmath>
#include <random>
#include <algorithm>
#include <type_traits>

#ifdef EIGEN_HEU_DO_OUTPUT
#include <iostream>
#endif

#include <unsupported/Eigen/CXX14/src/MetaHeuristic/Global/Global>
#include "InternalHeaderCheck.h"
#include "GAOption.hpp"
#include "GAAbstract.hpp"

namespace Eigen {

namespace internal {

/**
 * \ingroup CXX14_METAHEURISTIC
 * \class GABase
 * \brief Genetic algorithm base class.
 *  It's an abstrcat base class for all genetic algorithm solvers.
 *
 * This class maintance its GAOption as a member.It also implements
 *
 * \tparam Var_t Encoded solution type. For instance, when solving TSP problems, what we really want to solve is a
 * permulation to arrange order between nodes, but the permulation is encoded into a double array and decoded via
 * sorting. So in this instance, Var_t is assigned to std::vector<double> instead of an permulation type. You should
 * decode when calculating fitness value.
 * \tparam Fitness_t Type of fitness
 * \tparam Record Whether the solver records the changes of fitness value or not.
 * \tparam Args_t Extra custom parameters.
 * \tparam _iFun_ Function to initialize an individual in population. This function will be called only
 * when initializing. Use nullptr if you hope to determine it at runtime
 * \tparam _fFun_ Funtion to compute fitness for any individual. Use nullptr if you hope to determine it at runtime
 * \tparam _cFun_ Function to apply crossover. Use nullptr if you hope to determine it at runtime
 * \tparam _mFun_ Function to apply mutation. Use nullptr if you hope to determine it at runtime
 */
template <typename Var_t, typename Fitness_t, RecordOption Record, class Args_t,
          typename GAAbstract<Var_t, Fitness_t, Args_t>::initializeFun _iFun_,
          typename GAAbstract<Var_t, Fitness_t, Args_t>::fitnessFun _fFun_,
          typename GAAbstract<Var_t, Fitness_t, Args_t>::crossoverFun _cFun_,
          typename GAAbstract<Var_t, Fitness_t, Args_t>::mutateFun _mFun_>
class GABase : public GAAbstract<Var_t, Fitness_t, Args_t>,
               public GAAbstract<Var_t, Fitness_t, Args_t>::template iFunBody<_iFun_>,
               public GAAbstract<Var_t, Fitness_t, Args_t>::template fFunBody<_fFun_>,
               public GAAbstract<Var_t, Fitness_t, Args_t>::template cFunBody<_cFun_>,
               public GAAbstract<Var_t, Fitness_t, Args_t>::template mFunBody<_mFun_> {
 private:
  using Base_t = GAAbstract<Var_t, Fitness_t, Args_t>;

 public:
  EIGEN_HEU_MAKE_GAABSTRACT_TYPES(Base_t)

  ~GABase() {}
  /**
   * \class Gene
   * \brief Type of a individual
   *
   */
  class Gene {
   public:
    /**
     * \brief Type that are light to be returned.
     *
     * Use Fitness_t if it's size is not greater that 8, and const Fitness_t & otherwise.
     */
    using fastFitness_t =
        typename std::conditional<(sizeof(Fitness_t) > sizeof(double)), const Fitness_t &, Fitness_t>::type;

    Var_t self;          ///< Value of decision variable
    Fitness_t _Fitness;  ///< Value of fitness
    bool _isCalculated;  ///< Whether the fitness is computed

    bool isCalculated() const { return _isCalculated; }  ///< If the fitness is computed
    void setUncalculated() { _isCalculated = false; }    ///< Set the fitness to be uncomputed
    fastFitness_t fitness() const { return _Fitness; }   ///< Get fitness
  };
  /// list iterator to Gene
  using GeneIt_t = typename std::list<Gene>::iterator;

 public:
  /**
   * \brief Set the option object
   *
   * \param o GAOption
   */
  inline void setOption(const GAOption &o) { _option = o; }

  /**
   * \brief Get the option object
   *
   * \return const GAOption& Const reference to _option
   */
  inline const GAOption &option() const { return _option; }

  /**
   * \brief Initialize the whole population
   *
   */
  void initializePop() {
    _population.resize(_option.populationSize);
    for (auto &i : _population) {
      GAExecutor<Base_t::HasParameters>::doInitialization(this, &i.self);

      i.setUncalculated();
    }
  }

  /**
   * \brief Get the whole population
   *
   * \return const std::list<Gene>& Const reference the the population
   */
  inline const std::list<Gene> &population() const { return _population; }

  /**
   * \brief Get the population that has been used.
   *
   * \return size_t generation
   */
  inline size_t generation() const { return _generation; }

  /**
   * \brief Get the current fail times
   *
   * \return size_t fail times
   */
  inline size_t failTimes() const { return _failTimes; }

 protected:
  std::list<Gene> _population;  ///< Population stored in list
  GAOption _option;             ///< Option of GA solver

  size_t _generation;  ///< Current generation
  size_t _failTimes;   ///< Current failtimes

  inline void __impl_clearRecord() {}  ///< Nothing is need to do if the solver doesn't record fitnesses.

  template <class this_t>
  inline void __impl_recordFitness() {}  ///< Nothing is need to do if the solver doesn't record fitnesses.

  /**
   * \brief Run the solver
   *
   * \tparam this_t Type of a solver. This type is added to record fitness through some template tricks like CRTP.
   */
  template <class this_t>
  void __impl_run() {
    _generation = 0;
    _failTimes = 0;
    static_cast<this_t *>(this)->__impl_clearRecord();
    while (true) {
      _generation++;
      static_cast<this_t *>(this)->__impl_computeAllFitness();

      static_cast<this_t *>(this)->__impl_select();

      static_cast<this_t *>(this)->template __impl_recordFitness<this_t>();

      if (_generation > _option.maxGenerations) {
#ifdef EIGEN_HEU_DO_OUTPUT
        std::cout << "Terminated by max generation limitation" << std::endl;
#endif
        break;
      }
      if (_option.maxFailTimes > 0 && _failTimes > _option.maxFailTimes) {
#ifdef EIGEN_HEU_DO_OUTPUT
        std::cout << "Terminated by max failTime limitation" << std::endl;
#endif
        break;
      }
#ifdef EIGEN_HEU_DO_OUTPUT
      std::cout << "Generation "
                << _generation
                //<<" , elite fitness="<<_eliteIt->fitness()
                << std::endl;
#endif
      static_cast<this_t *>(this)->__impl_crossover();
      static_cast<this_t *>(this)->__impl_mutate();
    }
    _generation--;
  }

  /**
   * \brief Compute fitness for the whole population
   *
   * If OpenMP is used, this process will be parallelized.
   *
   */
  void __impl_computeAllFitness() {
#ifdef EIGEN_HAS_OPENMP
    std::vector<Gene *> tasks;
    tasks.resize(0);
    tasks.reserve(_population.size());
    for (Gene &i : _population) {
      if (i._isCalculated) {
        continue;
      }
      tasks.emplace_back(&i);
    }
    static const int32_t thN = Eigen::nbThreads();
#pragma omp parallel for schedule(dynamic, tasks.size() / thN)
    for (int i = 0; i < tasks.size(); i++) {
      Gene *ptr = tasks[i];

      GAExecutor<Base_t::HasParameters>::doFitness(this, &ptr->self, &ptr->_Fitness);

      ptr->_isCalculated = true;
    }
#else
    for (Gene &i : _population) {
      if (i._isCalculated) {
        continue;
      }

      GAExecutor<Base_t::HasParameters>::doFitness(this, &i.self, &i._Fitness);

      i._isCalculated = true;
    }
#endif
  }

  /**
   * \brief Virtual function to apply selection.
   *
   * Since selection differes a lot, this function can only be implemented by derived classes.
   *
   * Usually the population will exceeds the population size, so this procedure erases relatively worse genes to ensure
   * the size of population can be restored.
   */
  // void select() = 0;

  /**
   * \brief Apply crossover.
   *
   * Apply crossover operation which let randomly 2 gene born 2 more new one. They will be added to population.
   */
  void __impl_crossover() {
    std::vector<GeneIt_t> crossoverQueue;
    crossoverQueue.clear();
    crossoverQueue.reserve(_population.size());

    for (GeneIt_t it = _population.begin(); it != _population.end(); it++) {
      if (ei_randD() <= _option.crossoverProb) {
        crossoverQueue.emplace_back(it);
      }
    }

    std::shuffle(crossoverQueue.begin(), crossoverQueue.end(), global_mt19937());

    if (crossoverQueue.size() % 2 == 1) {
      crossoverQueue.pop_back();
    }

    while (!crossoverQueue.empty()) {
      GeneIt_t a, b;
      a = crossoverQueue.back();
      crossoverQueue.pop_back();
      b = crossoverQueue.back();
      crossoverQueue.pop_back();
      _population.emplace_back();
      Gene *childA = &_population.back();
      _population.emplace_back();
      Gene *childB = &_population.back();

      GAExecutor<Base_t::HasParameters>::doCrossover(this, &a->self, &b->self, &childA->self, &childB->self);

      childA->setUncalculated();
      childB->setUncalculated();
    }
  }

  /**
   * \brief Apply mutation operation which slightly modify gene. The modified gene will add to the population.
   */
  void __impl_mutate() {
    std::vector<GeneIt_t> mutateList;
    mutateList.reserve(size_t(this->_population.size() * this->_option.mutateProb * 2));
    for (auto it = this->_population.begin(); it != this->_population.end(); ++it) {
      if (ei_randD() <= this->_option.mutateProb) {
        mutateList.emplace_back(it);
      }
    }
    for (auto src : mutateList) {
      this->_population.emplace_back();
      GAExecutor<Base_t::HasParameters>::doMutation(this, &src->self, &this->_population.back().self);
      this->_population.back().setUncalculated();
    }
  }

 protected:
  // Internal class to apply the four operation
  template <bool HasParameters, class unused = void>
  struct GAExecutor {
    inline static void doInitialization(GABase *s, Var_t *v) { s->runiFun(v, &s->_args); }

    inline static void doFitness(GABase *s, const Var_t *v, Fitness_t *f) { s->runfFun(v, &s->_args, f); }

    inline static void doCrossover(GABase *s, const Var_t *p1, const Var_t *p2, Var_t *c1, Var_t *c2) {
      s->runcFun(p1, p2, c1, c2, &s->_args);
    }

    inline static void doMutation(GABase *s, const Var_t *src, Var_t *dst) { s->runmFun(src, dst, &s->_args); }
    static_assert(HasParameters == GABase::HasParameters, "struct GAExecutor actived with wrong template parameter.");
  };

  // Internal class to apply the four operation
  template <class unused>
  struct GAExecutor<false, unused> {
    inline static void doInitialization(GABase *s, Var_t *v) { s->runiFun(v); }

    inline static void doFitness(GABase *s, const Var_t *v, Fitness_t *f) { s->runfFun(v, f); }

    inline static void doCrossover(GABase *s, const Var_t *p1, const Var_t *p2, Var_t *c1, Var_t *c2) {
      s->runcFun(p1, p2, c1, c2);
    }

    inline static void doMutation(GABase *s, const Var_t *src, Var_t *dst) { s->runmFun(src, dst); }

    static_assert(GABase::HasParameters == false, "struct GAExecutor actived with wrong template parameter.");
  };
};

#define EIGEN_HEU_MAKE_GABASE_TYPES(Base_t)   \
  using Gene = typename Base_t::Gene;         \
  using GeneIt_t = typename Base_t::GeneIt_t; \
  EIGEN_HEU_MAKE_GAABSTRACT_TYPES(Base_t)

/**
 * \ingroup CXX14_METAHEURISTIC
 * \class GABase
 * \brief partial specialization for GABase with record.
 *
 * GABase with fitness maintains a record of fitness.
 * It's an abstrcat base class for all genetic algorithm solvers that records fitness.
 *
 * \tparam Var_t  Type of decisition variable
 * \tparam Fitness_t  Type of fitness value(objective value)
 * \tparam RecordOption  Whether the solver records fitness changelog
 * \tparam Args_t  Type of other parameters.
 *
 * \note GABase with record is derived from GABase without record to avoid duplicated implementation.
 */
template <typename Var_t, typename Fitness_t, class Args_t,
          typename GAAbstract<Var_t, Fitness_t, Args_t>::initializeFun _iFun_,
          typename GAAbstract<Var_t, Fitness_t, Args_t>::fitnessFun _fFun_,
          typename GAAbstract<Var_t, Fitness_t, Args_t>::crossoverFun _cFun_,
          typename GAAbstract<Var_t, Fitness_t, Args_t>::mutateFun _mFun_>
class GABase<Var_t, Fitness_t, RECORD_FITNESS, Args_t, _iFun_, _fFun_, _cFun_, _mFun_>
    : public GABase<Var_t, Fitness_t, DONT_RECORD_FITNESS, Args_t, _iFun_, _fFun_, _cFun_, _mFun_> {
 private:
  using Base_t = GABase<Var_t, Fitness_t, DONT_RECORD_FITNESS, Args_t, _iFun_, _fFun_, _cFun_, _mFun_>;
  friend class GABase<Var_t, Fitness_t, DONT_RECORD_FITNESS, Args_t, _iFun_, _fFun_, _cFun_, _mFun_>;

 public:
  EIGEN_HEU_MAKE_GABASE_TYPES(Base_t)

  ~GABase() {}

  /// best fitness

  /// result

  /**
   * \brief Get the record.
   *
   * \return const std::vector<Fitness_t> & Const reference to the record.
   */
  const std::vector<Fitness_t> &record() const { return _record; }

 protected:
  /**
   * \brief Record the best fitness of each generation.
   *
   * \note This member only exists when RecordOption is RECORD_FITNESS
   */
  std::vector<Fitness_t> _record;

  inline void __impl_clearRecord() {
    _record.clear();
    _record.reserve(this->_option.maxGenerations + 1);
  }

  template <class this_t>
  inline void __impl_recordFitness() {
    _record.emplace_back(static_cast<this_t *>(this)->bestFitness());
  }
};
}  //  namespace internal
}  //  namespace Eigen

#endif  // EIGEN_HEU_GABASE_H
