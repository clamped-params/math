#include <stan/math/mix/scal.hpp>

#include <gtest/gtest.h>
#include <vector>

TEST(MathMetaMix, broadcast_array) {
  using stan::math::fvar;
  using stan::math::internal::broadcast_array;
  using stan::math::var;

  fvar<var> fv(1.0, 2.1);
  broadcast_array<fvar<var> > ba(fv);
  EXPECT_EQ(1.0, ba[0].val_.vi_->val_);
  EXPECT_EQ(1.0, ba[2].val_.vi_->val_);

  double two = 2.0;
  broadcast_array<double> ba2(two);
  std::vector<double> vd = {{1.0, 2.0}};
  ba2 = vd;
  EXPECT_EQ(1.0, ba2[0]);
}