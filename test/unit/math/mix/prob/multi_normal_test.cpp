#include <test/unit/math/test_ad.hpp>

TEST(ProbDistributionsInvWishart, matvar) {
  auto f = [](const auto& y, const auto& mu, const auto& sigma) {
    auto sigma_sym = stan::math::multiply(0.5, sigma + sigma.transpose());
    return stan::math::multi_normal_lpdf(y, mu, sigma_sym);
  };

  Eigen::VectorXd y1(1);
  y1 << 1;
  Eigen::VectorXd mu1(1);
  mu1 << 3.4;
  Eigen::MatrixXd Sigma11(1, 1);
  Sigma11 << 1;
  stan::test::expect_ad(f, y1, mu1, Sigma11);
  stan::test::expect_ad_matvar(f, y1, mu1, Sigma11);

  /*Eigen::MatrixXd y00(0, 0);
  Eigen::MatrixXd Sigma00(0, 0);
  stan::test::expect_ad(f, y00, dof, Sigma00);
  stan::test::expect_ad_matvar(f, y00, dof, Sigma00);

  Eigen::MatrixXd y22(2, 2);
  y22 << 1.0, 0.1, 0.1, 2.0;
  Eigen::MatrixXd Sigma22(2, 2);
  Sigma22 << 2.0, 0.5, 0.5, 1.1;
  stan::test::expect_ad(f, y22, dof, Sigma22);
  stan::test::expect_ad_matvar(f, y22, dof, Sigma22);

  // Error sizes
  stan::test::expect_ad(f, y00, dof, Sigma11);
  stan::test::expect_ad(f, y11, dof, Sigma00);
  stan::test::expect_ad_matvar(f, y00, dof, Sigma11);
  stan::test::expect_ad_matvar(f, y11, dof, Sigma00);*/
}

TEST(ProbDistributionsMultiNormal, fvar_var) {
  using Eigen::Dynamic;
  using Eigen::Matrix;
  using stan::math::fvar;
  using stan::math::var;
  using std::vector;

  Matrix<fvar<var>, Dynamic, 1> y(3, 1);
  y << 2.0, -2.0, 11.0;
  Matrix<fvar<var>, Dynamic, 1> mu(3, 1);
  mu << 1.0, -1.0, 3.0;
  Matrix<fvar<var>, Dynamic, Dynamic> Sigma(3, 3);
  Sigma << 9.0, -3.0, 0.0, -3.0, 4.0, 0.0, 0.0, 0.0, 5.0;
  for (int i = 0; i < 3; i++) {
    y(i).d_ = 1.0;
    mu(i).d_ = 1.0;
    for (int j = 0; j < 3; j++)
      Sigma(i, j).d_ = 1.0;
  }

  fvar<var> res = stan::math::multi_normal_log(y, mu, Sigma);
  EXPECT_FLOAT_EQ(-11.73908, res.val_.val());
  EXPECT_FLOAT_EQ(0.54899865, res.d_.val());

  stan::math::recover_memory();
}

TEST(ProbDistributionsMultiNormal, fvar_fvar_var) {
  using Eigen::Dynamic;
  using Eigen::Matrix;
  using stan::math::fvar;
  using stan::math::var;
  using std::vector;

  Matrix<fvar<fvar<var> >, Dynamic, 1> y(3, 1);
  y << 2.0, -2.0, 11.0;
  Matrix<fvar<fvar<var> >, Dynamic, 1> mu(3, 1);
  mu << 1.0, -1.0, 3.0;
  Matrix<fvar<fvar<var> >, Dynamic, Dynamic> Sigma(3, 3);
  Sigma << 9.0, -3.0, 0.0, -3.0, 4.0, 0.0, 0.0, 0.0, 5.0;
  for (int i = 0; i < 3; i++) {
    y(i).d_ = 1.0;
    mu(i).d_ = 1.0;
    for (int j = 0; j < 3; j++)
      Sigma(i, j).d_ = 1.0;
  }

  fvar<fvar<var> > res = stan::math::multi_normal_log(y, mu, Sigma);
  EXPECT_FLOAT_EQ(-11.73908, res.val_.val_.val());
  EXPECT_FLOAT_EQ(0.54899865, res.d_.val_.val());

  stan::math::recover_memory();
}
