#include "decider.hpp"

#include <yaml-cpp/yaml.h>

#include <opencv2/opencv.hpp>

#include "tools/logger.hpp"
#include "tools/math_tools.hpp"

namespace omniperception
{
Decider::Decider(const std::string & config_path) : detector_(config_path)
{
  auto yaml = YAML::LoadFile(config_path);
  img_width_ = yaml["image_width"].as<double>();
  img_height_ = yaml["image_height"].as<double>();
  fov_h_ = yaml["fov_h"].as<double>();
  fov_v_ = yaml["fov_v"].as<double>();
  enemy_color_ =
    (yaml["enemy_color"].as<std::string>() == "red") ? auto_aim::Color::red : auto_aim::Color::blue;
  mode_ = yaml["mode"].as<double>();
}

io::Command Decider::decide(
  auto_aim::YOLOV8 & yolov8, const Eigen::Vector3d & gimbal_pos, io::USBCamera & usbcam1,
  io::USBCamera & usbcam2, io::USBCamera & usbcam3, io::USBCamera & usbcam4)
{
  Eigen::Vector2d delta_angle;
  io::USBCamera * cams[] = {&usbcam1, &usbcam2, &usbcam3, &usbcam4};
  std::vector<std::string> cam_names = {"front_left", "front_right", "back_left", "back_right"};

  for (int i = 0; i < 4; ++i) {
    cv::Mat usb_img;
    std::chrono::steady_clock::time_point timestamp;
    cams[i]->read(usb_img, timestamp);
    auto armors = yolov8.detect(usb_img);
    auto empty = armor_filter(armors);

    if (!empty) {
      delta_angle = this->delta_angle(armors, cams[i]->device_name);
      tools::logger()->debug(
        "delta yaw:{:.2f},target pitch:{:.2f},armor number:{},armor name:{}", delta_angle[0],
        delta_angle[1], armors.size(), auto_aim::ARMOR_NAMES[armors.front().name]);

      return io::Command{
        true, false, tools::limit_rad(gimbal_pos[0] + delta_angle[0] / 57.3),
        tools::limit_rad(delta_angle[1] / 57.3)};
    }
  }

  // 如果没有找到目标，返回默认命令
  return io::Command{false, false, 0, 0};
}

Eigen::Vector2d Decider::delta_angle(
  const std::list<auto_aim::Armor> & armors, const std::string & camera)
{
  Eigen::Vector2d delta_angle;
  if (camera == "front_left") {
    delta_angle[0] = 45 + (fov_h_ / 2) - armors.front().center_norm.x * fov_h_;
    delta_angle[1] = -(armors.front().center_norm.y * fov_v_ - fov_v_ / 2);
    return delta_angle;
  } else if (camera == "front_right") {
    delta_angle[0] = -45 + (fov_h_ / 2) - armors.front().center_norm.x * fov_h_;
    delta_angle[1] = -(armors.front().center_norm.y * fov_v_ - fov_v_ / 2);
    return delta_angle;
  } else if (camera == "back_left") {
    delta_angle[0] = 135 + (fov_h_ / 2) - armors.front().center_norm.x * fov_h_;
    delta_angle[1] = -(armors.front().center_norm.y * fov_v_ - fov_v_ / 2);
    return delta_angle;
  } else {
    delta_angle[0] = -135 + (fov_h_ / 2) - armors.front().center_norm.x * fov_h_;
    delta_angle[1] = -(armors.front().center_norm.y * fov_v_ - fov_v_ / 2);
    return delta_angle;
  }
}

bool Decider::armor_filter(std::list<auto_aim::Armor> & armors, const std::string & armor_omit)
{
  // 过滤友方装甲板
  armors.remove_if([&](const auto_aim::Armor & a) { return a.color != enemy_color_; });

  // RMUC过滤前哨站、基地、哨兵
  // armors.remove_if([&](const auto_aim::Armor & a) {
  //   return a.name == auto_aim::ArmorName::outpost || a.name == auto_aim::ArmorName::base ||
  //          a.name == auto_aim::ArmorName::sentry;
  // });

  // 过滤掉刚复活无敌的装甲板
  if (!armor_omit.empty() || armor_omit != "0,") {
    std::vector<std::string> non_zero_numbers;
    std::vector<std::string> numbers;
    std::stringstream ss(armor_omit);
    std::string token;
    while (std::getline(ss, token, ',')) {
      numbers.push_back(token);
    }
    for (const std::string & num : numbers) {
      if (num != "0") {
        non_zero_numbers.push_back(num);
      }
    }
    armors.remove_if([&](const auto_aim::Armor & a) {
      std::string armor_name = std::to_string(static_cast<int>(a.name) + 1);
      return std::find(non_zero_numbers.begin(), non_zero_numbers.end(), armor_name) !=
             non_zero_numbers.end();
    });
  }

  return armors.empty();
}

void Decider::set_priority(std::list<auto_aim::Armor> & armors)
{
  const PriorityMap & priority_map = (mode_ == MODE_ONE) ? mode1 : mode2;

  if (!armors.empty()) {
    for (auto & armor : armors) {
      armor.priority = priority_map.at(armor.name);
    }
  }
}

bool Decider::check_perception(
  const std::string & str1, const std::string & str2, const std::string & str3)
{
  return (str1 == str2 || str2 == str3 || str1 == str3);
}

}  // namespace omniperception