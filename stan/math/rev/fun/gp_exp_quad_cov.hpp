#ifndef STAN_MATH_REV_FUN_GP_EXP_QUAD_COV_HPP
#define STAN_MATH_REV_FUN_GP_EXP_QUAD_COV_HPP

#include <stan/math/rev/meta.hpp>
#include <stan/math/rev/core.hpp>
#include <stan/math/rev/fun/value_of.hpp>
#include <stan/math/rev/functor/reverse_pass_callback.hpp>
#include <stan/math/rev/core/arena_matrix.hpp>
#include <stan/math/prim/err.hpp>
#include <stan/math/prim/fun/Eigen.hpp>
#include <stan/math/prim/fun/exp.hpp>
#include <stan/math/prim/fun/squared_distance.hpp>
#include <cmath>
#include <type_traits>
#include <vector>

namespace stan {
namespace math {

/**
 * Returns a squared exponential kernel.
 *
 * @tparam T_x type of elements in the vector
 * @param x std::vector input that can be used in square distance
 *    Assumes each element of x is the same size
 * @param sigma standard deviation
 * @param length_scale length scale
 * @return squared distance
 * @throw std::domain_error if sigma <= 0, l <= 0, or
 *   x is nan or infinite
 */
template <typename T_x, typename T_sigma, typename T_l,
          require_all_stan_scalar_t<T_sigma, T_l>* = nullptr,
          require_any_st_var<T_x, T_sigma, T_l>* = nullptr>
inline auto gp_exp_quad_cov(const std::vector<T_x>& x, T_sigma sigma,
                            T_l length_scale) {
  using ret_type = Eigen::Matrix<var, Eigen::Dynamic, Eigen::Dynamic>;

  check_positive("gp_exp_quad_cov", "marginal standard deviation", sigma);
  check_positive("gp_exp_quad_cov", "length scale", length_scale);

  for (size_t i = 0; i < x.size(); ++i) {
    check_not_nan("gp_exp_quad_cov", "x", x[i]);
  }

  if (x.size() == 0) {
    return ret_type();
  }
  if (is_constant<T_x>::value && !is_constant<T_sigma>::value
      && !is_constant<T_l>::value) {
    const size_t N = x.size();
    const size_t P = (N * N - N) / 2 + N;
    const double inv_half_sq_l_d
        = 0.5 / (value_of(length_scale) * value_of(length_scale));
    const double sigma_sq_d = value_of(sigma) * value_of(sigma);
    arena_t<ret_type> res(N, N);
    arena_t<Eigen::VectorXd> arena_dist(P);
    size_t pos = 0;
    for (size_t j = 0; j < N; ++j) {
      for (size_t i = 0; i <= j; ++i) {
        if (unlikely(i == j)) {
          arena_dist.coeffRef(pos) = 0.0;
          res.coeffRef(i, j) = sigma_sq_d;
        } else {
          double dist_sq;

          if (is_stan_scalar<T_x>::value) {
            double diff = forward_as<double>(value_of(x[i]) - value_of(x[j]));
            dist_sq = diff * diff;
          } else if (is_eigen_col_vector<T_x>::value)
            dist_sq
                = forward_as<Eigen::VectorXd>(value_of(x[i]) - value_of(x[j]))
                      .squaredNorm();
          else if (is_eigen_row_vector<T_x>::value)
            dist_sq = forward_as<Eigen::RowVectorXd>(value_of(x[i])
                                                     - value_of(x[j]))
                          .squaredNorm();

          arena_dist.coeffRef(pos) = dist_sq;
          res.coeffRef(i, j)
              = sigma_sq_d * std::exp(-dist_sq * inv_half_sq_l_d);
        }
        ++pos;
      }
    }

    for (size_t j = 0; j < N; ++j) {
      for (size_t i = 0; i <= j; ++i) {
        res.coeffRef(j, i) = res.coeff(i, j);
      }
    }
    reverse_pass_callback([res, arena_dist, sigma, length_scale, N]() mutable {
      const double l_d = value_of(length_scale);
      auto& sigma_adj = forward_as<var>(sigma).adj();
      auto& l_adj = forward_as<var>(length_scale).adj();

      size_t pos = 0;
      for (size_t j = 0; j < N; ++j) {
        for (size_t i = 0; i <= j; ++i) {
          const double adj_times_val
              = res.coeff(i, j).val() * res.coeff(i, j).adj();
          sigma_adj += adj_times_val;
          l_adj += arena_dist.coeff(pos) * adj_times_val;
          ++pos;
        }
      }
      sigma_adj *= 2.0;
      sigma_adj /= value_of(sigma);
      l_adj /= (l_d * l_d * l_d);
    });
    return ret_type(res);
  } else {
    double sigma_sq_d = value_of(sigma) * value_of(sigma);
    double inv_half_sq_l_d
        = 0.5 / (value_of(length_scale) * value_of(length_scale));

    size_t N = x.size();
    size_t P = (N * N - N) / 2 + N;
    arena_t<Eigen::VectorXd> dist(P);
    arena_t<ret_type> res(N, N);
    auto arena_x = to_arena_if<!is_constant<T_x>::value>(x);

    size_t pos = 0;
    for (size_t j = 0; j < N; ++j) {
      for (size_t i = 0; i <= j; ++i) {
        if (i != j) {
          double dist_sq;

          if (is_stan_scalar<T_x>::value) {
            double diff = forward_as<double>(value_of(x[i]) - value_of(x[j]));
            dist_sq = diff * diff;
          } else if (is_eigen_col_vector<T_x>::value)
            dist_sq
                = forward_as<Eigen::VectorXd>(value_of(x[i]) - value_of(x[j]))
                      .squaredNorm();
          else if (is_eigen_row_vector<T_x>::value)
            dist_sq = forward_as<Eigen::RowVectorXd>(value_of(x[i])
                                                     - value_of(x[j]))
                          .squaredNorm();

          dist.coeffRef(pos) = dist_sq;
          res.coeffRef(i, j)
              = sigma_sq_d * std::exp(-dist_sq * inv_half_sq_l_d);
        } else {
          dist.coeffRef(pos) = 0.0;
          res.coeffRef(i, j) = sigma_sq_d;
        }
        ++pos;
      }
    }

    for (size_t j = 0; j < N; ++j) {
      for (size_t i = 0; i <= j; ++i) {
        res.coeffRef(j, i) = res.coeff(i, j);
      }
    }

    reverse_pass_callback(
        [res, arena_x, dist, sigma, length_scale, N]() mutable {
          const double& l_d = value_of(length_scale);
          const double inv_l_d_squared = 1.0 / (l_d * l_d);

          size_t pos = 0;
          for (size_t j = 0; j < N; ++j) {
            for (size_t i = 0; i <= j; ++i) {
              const double adj_times_val
                  = res.coeff(i, j).val() * res.coeff(i, j).adj();

              if (!is_constant<T_sigma>::value) {
                forward_as<var>(sigma).adj() += adj_times_val;
              }

              if (!is_constant<T_l>::value) {
                forward_as<var>(length_scale).adj()
                    += dist.coeff(pos) * adj_times_val;
              }

              if (!is_constant<T_x>::value) {
                if (i != j) {
                  auto adj = eval(-(value_of(arena_x[i]) - value_of(arena_x[j]))
                                  * adj_times_val * inv_l_d_squared);
                  using T_x_var = promote_scalar_t<var, T_x>;
                  forward_as<T_x_var>(arena_x[i]).adj() += adj;
                  forward_as<T_x_var>(arena_x[j]).adj() -= adj;
                }
              }
              ++pos;
            }
          }

          if (!is_constant<T_sigma>::value) {
            forward_as<var>(sigma).adj() *= 2.0;
            forward_as<var>(sigma).adj() /= value_of(sigma);
          }

          if (!is_constant<T_l>::value) {
            forward_as<var>(length_scale).adj() /= (l_d * l_d * l_d);
          }
        });

    return ret_type(res);
  }
}

}  // namespace math
}  // namespace stan
#endif
