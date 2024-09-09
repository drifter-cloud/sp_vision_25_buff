#ifndef AUTO_AIM__SOLVER_HPP
#define AUTO_AIM__SOLVER_HPP

#include <Eigen/Dense>  // 必须在opencv2/core/eigen.hpp上面
#include <Eigen/Geometry>
#include <opencv2/core/eigen.hpp>

#include "armor.hpp"

namespace auto_aim
{
class Solver
{
public:
  explicit Solver(const std::string & config_path);

  Eigen::Matrix3d R_gimbal2world() const;

  void set_R_gimbal2world(const Eigen::Quaterniond & q);

  void solve(Armor & armor) const;

  std::vector<cv::Point2f> reproject_armor(
    const Eigen::Vector3d & xyz_in_world, double yaw, ArmorType type, ArmorName name) const;

private:
  cv::Mat camera_matrix_;
  cv::Mat distort_coeffs_;
  Eigen::Matrix3d R_gimbal2imubody_;
  Eigen::Matrix3d R_camera2gimbal_;
  Eigen::Vector3d t_camera2gimbal_;
  Eigen::Matrix3d R_gimbal2world_;

  void optimize_yaw(Armor & armor) const;

  double armor_reprojection_error(const Armor & armor, double yaw) const;
};

}  // namespace auto_aim

#endif  // AUTO_AIM__SOLVER_HPP