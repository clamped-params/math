#ifndef STAN_MATH_REV_SCAL_FUNCTOR_REDUCE_SUM_HPP
#define STAN_MATH_REV_SCAL_FUNCTOR_REDUCE_SUM_HPP

#include <stan/math/prim/meta.hpp>
#include <stan/math/rev/fun/typedefs.hpp>

#include <tbb/task_arena.h>
#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>

#include <iostream>
#include <iterator>
#include <vector>

namespace stan {
namespace math {

template <typename T>
struct value_type {
  using type = double;
};

template <>
struct value_type<int> {
  using type = int;
};

template <template <typename> class T_struct, typename T_value>
struct value_type<T_struct<T_value>> {
  using type = T_struct<typename value_type<T_value>::type>;
};

template <typename T_value>
struct value_type<std::vector<T_value>> {
  using type = std::vector<typename value_type<T_value>::type>;
};

template <typename T_value, int R, int C>
struct value_type<Eigen::Matrix<T_value, R, C>> {
  using type = Eigen::Matrix<typename value_type<T_value>::type, R, C>;
};

// as_value is similar to value_of... with the difference that it
// keeps nested container structures which is not the case for
// value_of

template <typename T>
typename value_type<T>::type as_value(const T x) {
  return typename value_type<T>::type(x);
}

template <typename T>
typename value_type<std::vector<T>>::type as_value(const std::vector<T>& x) {
  const size_t size = x.size();
  typename value_type<std::vector<T>>::type result(size);
  for (size_t i = 0; i != size; ++i) {
    result[i] = as_value(x[i]);
  }
  return result;
}

inline double as_value(const var& x) { return x.vi_->val_; }

template <typename T, int R, int C>
inline Eigen::Matrix<typename value_type<T>::type, R, C> as_value(
    const Eigen::Matrix<T, R, C>& M) {
  Eigen::Matrix<typename value_type<T>::type, R, C> Md(M.rows(), M.cols());
  for (int j = 0; j < M.cols(); j++) {
    for (int i = 0; i < M.rows(); i++) {
      Md(i, j) = as_value(M(i, j));
    }
  }
  return Md;
}

template <int R, int C>
inline const Eigen::Matrix<double, R, C>& as_value(
    const Eigen::Matrix<double, R, C>& x) {
  return x.val();
}

template <int R, int C>
inline const Eigen::Matrix<int, R, C>& as_value(
    const Eigen::Matrix<int, R, C>& x) {
  return x.val();
}

namespace internal {

inline int add_adjoint(const var& op, int offset_partials,
                       Eigen::VectorXd& partials) {
  partials(offset_partials) += op.vi_->adj_;
  return ++offset_partials;
}

// template<typename Operand>
inline int add_adjoint(const int op, int offset_partials,
                       Eigen::VectorXd& partials) {
  // no-op, does nothing
  return offset_partials;
}

inline int add_adjoint(const double op, int offset_partials,
                       Eigen::VectorXd& partials) {
  // no-op, does nothing
  return offset_partials;
}

template <int R, int C>
inline int add_adjoint(const Eigen::Matrix<double, R, C>& op,
                       int offset_partials, Eigen::VectorXd& partials) {
  return offset_partials;
}

template <int R, int C>
inline int add_adjoint(const Eigen::Matrix<int, R, C>& op, int offset_partials,
                       Eigen::VectorXd& partials) {
  return offset_partials;
}

template <int R, int C>
inline int add_adjoint(const Eigen::Matrix<var, R, C>& op, int offset_partials,
                       Eigen::VectorXd& partials) {
  for (int j = 0; j != op.cols(); ++j) {
    for (int i = 0; i != op.rows(); ++i, ++offset_partials) {
      partials(offset_partials) += op(i, j).vi_->adj_;
    }
  }
  return offset_partials;
}

template <typename Operand>
inline int add_adjoint(const std::vector<Operand>& op, int offset_partials,
                       Eigen::VectorXd& partials) {
  for (size_t i = 0; i != op.size(); ++i) {
    offset_partials = add_adjoint(op[i], offset_partials, partials);
  }
  return offset_partials;
}

inline vari** register_operands(const var& op, vari** varis) {
  (*varis) = op.vi_;
  return varis + 1;
}

template <int R, int C>
inline vari** register_operands(const Eigen::Matrix<var, R, C>& M_op,
                                vari** varis) {
  const size_t size = M_op.size();
  for (size_t i = 0; i != M_op.cols(); ++i) {
    for (size_t j = 0; j != M_op.rows(); ++j, ++varis) {
      *varis = M_op(j, i).vi_;
    }
  }
  return varis;
}

template <typename T>
inline vari** register_operands(const std::vector<T>& op, vari** varis) {
  const size_t size = op.size();
  for (size_t i = 0; i != size; ++i) {
    varis = register_operands(op[i], varis);
  }
  return varis;
}

/* this template is not picked up when it should??
template <typename T_base>
const std::vector< std::vector<T_base>> initialize_from_value(
    const std::vector<std::vector<typename value_type<T_base>::type>>& value) {
  std::vector<std::vector<T_base>> base(value.size(),
std::vector<T_base>(value.size(), 0)); return base;
}
*/

// use enable_if style to allow constuction of scalars and Eigen
// things as well.
template <typename T_base>
const std::vector<T_base> initialize_from_value(
    const std::vector<typename value_type<T_base>::type>& value) {
  const std::vector<T_base> base(value.begin(), value.end());
  return base;
}

/*
template <typename T_base>
const T_base initialize_from_value(const typename value_type<T_base>::type&
value) { const T_base base(value.begin(), value.end()); return base;
}
*/

/*
 * we should apply some meta magic to defined
 * local_operands_and_partials based on wether is_constant<T> is true
 * or false (instead of having this as if in every function). As a
 * work-around I have below partial template specializations which
 * give me the same effect with much less generality.
 */
template <typename T>
struct local_operand_and_partials {
  using Op_t = std::vector<T>;
  using Op_value_t = std::vector<typename value_type<T>::type>;
  using Op_local_t = Op_t;
  using Op_local_slice_t = Op_t;
  using partials_t = Eigen::VectorXd;

  const Op_t& op_;
  std::shared_ptr<const Op_value_t> op_value_ptr_;
  const Op_value_t& op_value_;
  partials_t partials_;

  explicit local_operand_and_partials(const Op_t& op)
      : op_(op),
        op_value_ptr_(new Op_value_t(as_value(op))),
        op_value_(*op_value_ptr_),
        partials_(Eigen::VectorXd::Zero(num_elements())) {}

  local_operand_and_partials(const local_operand_and_partials<T>& other)
      : op_(other.op_),
        op_value_ptr_(other.op_value_ptr_),
        op_value_(other.op_value_),
        partials_(Eigen::VectorXd::Zero(num_elements())) {}

  inline void add_local_adjoint(const Op_t& local_op, std::size_t offset = 0) {
    if (!is_constant<T>::value) {
      add_adjoint(local_op, offset, partials_);
    }
  }

  inline void add_other_adjoint(const local_operand_and_partials<T>& other) {
    if (!is_constant<T>::value) {
      partials_ += other.partials_;
    }
  }

  inline const Op_local_t local_op() const {
    return initialize_from_value<T>(op_value_);
  }

  inline const Op_local_slice_t local_op(std::size_t start,
                                         std::size_t end) const {
    const Op_value_t slice(op_value_.begin() + start, op_value_.begin() + end);
    return initialize_from_value<T>(slice);
  }

  inline std::size_t num_elements() const {
    if (!is_constant<T>::value)
      return stan::math::num_elements(op_value_);
    return 0;
  }

  inline void build(vari** varis, double* partials) const {
    if (!is_constant<T>::value) {
      register_operands(op_, varis);
      std::copy(partials_.data(), partials_.data() + partials_.size(),
                partials);
    }
  }
};

/* for types which are constants (non-vars), the implemtation can be
 * seriously simplified and based on constant refs being returned
 */

template <>
struct local_operand_and_partials<double> {
  using Op_t = std::vector<double>;
  using Op_local_t = const Op_t&;
  using Op_local_slice_t = Op_t;

  const Op_t& op_;

  explicit local_operand_and_partials(const Op_t& op) : op_(op) {}

  local_operand_and_partials(const local_operand_and_partials<double>& other)
      : op_(other.op_) {}

  inline void add_local_adjoint(const Op_t& local_op, std::size_t offset = 0) {}

  inline void add_other_adjoint(
      const local_operand_and_partials<double>& other) {}

  inline Op_local_t local_op() const { return op_; }

  inline const Op_local_slice_t local_op(std::size_t start,
                                         std::size_t end) const {
    const Op_local_slice_t slice(op_.begin() + start, op_.begin() + end);
    return slice;
  }

  inline std::size_t num_elements() const { return 0; }

  inline void build(vari** varis, double* partials) const {}
};

template <>
struct local_operand_and_partials<int> {
  using Op_t = std::vector<int>;
  using Op_local_t = const Op_t&;
  using Op_local_slice_t = Op_t;

  const Op_t& op_;

  explicit local_operand_and_partials(const Op_t& op) : op_(op) {}

  local_operand_and_partials(const local_operand_and_partials<int>& other)
      : op_(other.op_) {}

  inline void add_local_adjoint(const Op_t& local_op, std::size_t offset = 0) {}

  inline void add_other_adjoint(const local_operand_and_partials<int>& other) {}

  inline Op_local_t local_op() const { return op_; }

  inline const Op_local_slice_t local_op(std::size_t start,
                                         std::size_t end) const {
    const Op_local_slice_t slice(op_.begin() + start, op_.begin() + end);
    return slice;
  }

  inline std::size_t num_elements() const { return 0; }

  inline void build(vari** varis, double* partials) const {}
};

/*
template <typename T, typename = void>
struct local_operand_and_partials<T, > {
};
*/

template <class ReduceFunction, class M, class T, class Arg1, class Arg2,
          class Arg3, class Arg4>
struct reduce_sum_impl<ReduceFunction, M, T, Arg1, Arg2, Arg3, Arg4, var> {
  using vmapped_t = std::vector<M>;
  using arg1_t = std::vector<Arg1>;
  using arg2_t = std::vector<Arg2>;
  using arg3_t = std::vector<Arg3>;
  using arg4_t = std::vector<Arg4>;

  using vmapped_op_t = local_operand_and_partials<M>;
  using arg1_local_op_t = local_operand_and_partials<Arg1>;
  using arg2_local_op_t = local_operand_and_partials<Arg2>;
  using arg3_local_op_t = local_operand_and_partials<Arg3>;
  using arg4_local_op_t = local_operand_and_partials<Arg4>;

  struct recursive_reducer {
    vmapped_op_t& op_vmapped_;
    arg1_local_op_t op_arg1_;
    arg2_local_op_t op_arg2_;
    arg3_local_op_t op_arg3_;
    arg4_local_op_t op_arg4_;

    const std::size_t num_vmapped_adjoints_per_element_;
    double terms_sum_;

    recursive_reducer(vmapped_op_t& op_vmapped,
                      std::size_t num_vmapped_adjoints_per_element,
                      const T& init, const arg1_t& arg1, const arg2_t& arg2,
                      const arg3_t& arg3, const arg4_t& arg4)
        : op_vmapped_(op_vmapped),
          op_arg1_(arg1),
          op_arg2_(arg2),
          op_arg3_(arg3),
          op_arg4_(arg4),
          num_vmapped_adjoints_per_element_(num_vmapped_adjoints_per_element),
          terms_sum_(as_value(init)) {}

    recursive_reducer(recursive_reducer& other, tbb::split)
        : op_vmapped_(other.op_vmapped_),
          op_arg1_(other.op_arg1_),
          op_arg2_(other.op_arg2_),
          op_arg3_(other.op_arg3_),
          op_arg4_(other.op_arg4_),
          num_vmapped_adjoints_per_element_(
              other.num_vmapped_adjoints_per_element_),
          terms_sum_(0.0) {}

    void operator()(const tbb::blocked_range<size_t>& r) {
      if (r.empty())
        return;

      try {
        start_nested();

        // create a deep copy of all var's so that these are not
        // linked to any outer AD tree

        const typename vmapped_op_t::Op_local_slice_t local_sub_slice
            = op_vmapped_.local_op(r.begin(), r.end());
        const typename arg1_local_op_t::Op_local_t local_arg1
            = op_arg1_.local_op();
        const typename arg2_local_op_t::Op_local_t local_arg2
            = op_arg2_.local_op();
        const typename arg3_local_op_t::Op_local_t local_arg3
            = op_arg3_.local_op();
        const typename arg4_local_op_t::Op_local_t local_arg4
            = op_arg4_.local_op();

        T sub_sum_v
            = ReduceFunction()(r.begin(), r.end() - 1, local_sub_slice,
                               local_arg1, local_arg2, local_arg3, local_arg4);

        sub_sum_v.grad();

        terms_sum_ += sub_sum_v.val();

        op_vmapped_.add_local_adjoint(
            local_sub_slice, num_vmapped_adjoints_per_element_ * r.begin());

        op_arg1_.add_local_adjoint(local_arg1);
        op_arg2_.add_local_adjoint(local_arg2);
        op_arg3_.add_local_adjoint(local_arg3);
        op_arg4_.add_local_adjoint(local_arg4);

      } catch (const std::exception& e) {
        recover_memory_nested();
        throw;
      }
      recover_memory_nested();
    }

    void join(const recursive_reducer& child) {
      terms_sum_ += child.terms_sum_;

      op_arg1_.add_other_adjoint(child.op_arg1_);
      op_arg2_.add_other_adjoint(child.op_arg2_);
      op_arg3_.add_other_adjoint(child.op_arg3_);
      op_arg4_.add_other_adjoint(child.op_arg4_);
    }
  };

  T operator()(const vmapped_t& vmapped, T init, std::size_t grainsize,
               const arg1_t& arg1, const arg2_t& arg2, const arg3_t& arg3,
               const arg4_t& arg4) const {
    const std::size_t num_jobs = vmapped.size();

    if (num_jobs == 0)
      return init;

    vmapped_op_t op_vmapped(vmapped);

    recursive_reducer worker(op_vmapped, num_elements(vmapped[0]), init, arg1,
                             arg2, arg3, arg4);

#ifdef STAN_DETERMINISTIC
    tbb::static_partitioner partitioner;
    tbb::parallel_deterministic_reduce(
        tbb::blocked_range<std::size_t>(0, num_jobs, grainsize), worker,
        partitioner);
#else
    tbb::parallel_reduce(
        tbb::blocked_range<std::size_t>(0, num_jobs, grainsize), worker);
#endif

    std::vector<std::size_t> num_terms_arg(5, 0);

    num_terms_arg[0] = op_vmapped.num_elements();
    num_terms_arg[1] = worker.op_arg1_.num_elements();
    num_terms_arg[2] = worker.op_arg2_.num_elements();
    num_terms_arg[3] = worker.op_arg3_.num_elements();
    num_terms_arg[4] = worker.op_arg4_.num_elements();

    const std::size_t num_terms = sum(num_terms_arg);

    vari** varis
        = ChainableStack::instance_->memalloc_.alloc_array<vari*>(num_terms);
    double* partials
        = ChainableStack::instance_->memalloc_.alloc_array<double>(num_terms);

    std::size_t idx = 0;

    op_vmapped.build(&varis[idx], &partials[idx]);
    idx += num_terms_arg[0];
    worker.op_arg1_.build(&varis[idx], &partials[idx]);
    idx += num_terms_arg[1];
    worker.op_arg2_.build(&varis[idx], &partials[idx]);
    idx += num_terms_arg[2];
    worker.op_arg3_.build(&varis[idx], &partials[idx]);
    idx += num_terms_arg[3];
    worker.op_arg4_.build(&varis[idx], &partials[idx]);

    return var(new precomputed_gradients_vari(worker.terms_sum_, num_terms,
                                              varis, partials));
  }
};
}  // namespace internal

}  // namespace math
}  // namespace stan

#endif