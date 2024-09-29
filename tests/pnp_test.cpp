#include <fmt/core.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

#include "io/camera.hpp"
#include "io/cboard.hpp"
#include "tasks/auto_aim_sentry/solver.hpp"
#include "tasks/auto_aim_sentry/yolov8.hpp"
#include "tools/exiter.hpp"
#include "tools/img_tools.hpp"
#include "tools/logger.hpp"
#include "tools/math_tools.hpp"
#include "tools/plotter.hpp"
#include "tools/recorder.hpp"

using namespace std::chrono;

const std::string keys =
  "{help h usage ? |                     | 输出命令行参数说明}"
  "{record r       |         false       | 是否保存录像}"
  "{@config-path   | configs/sentry.yaml | 位置参数，yaml配置文件路径 }";

int main(int argc, char * argv[])
{
  tools::Exiter exiter;
  tools::Plotter plotter;
  tools::Recorder recorder(60);

  cv::CommandLineParser cli(argc, argv, keys);
  auto config_path = cli.get<std::string>(0);
  auto record = cli.get<bool>("record");
  if (cli.has("help") || config_path.empty()) {
    cli.printMessage();
    return 0;
  }

  io::CBoard cboard("can0");
  io::Camera camera(config_path);

  auto_aim::YOLOV8 detector(config_path, true);
  auto_aim::Solver solver(config_path);

  cv::Mat img;
  Eigen::Quaterniond q;
  std::chrono::steady_clock::time_point t;

  while (!exiter.exit()) {
    camera.read(img, t);
    q = cboard.imu_at(t - 1ms);
    auto yaw = tools::eulers(q, 2, 1, 0)[0];
    if (record) recorder.record(img, q, t);

    /// 自瞄核心逻辑

    solver.set_R_gimbal2world(q);

    auto armors = detector.detect(img);

    if (armors.size() != 0) {
      auto armor = armors.front();
      solver.solve(armor);
      /// 调试输出
      tools::draw_text(
        img,
        fmt::format(
          "in world frame x:{:.2f}  y:{:.2f}  z:{:.2f}", armor.xyz_in_world[0],
          armor.xyz_in_world[1], armor.xyz_in_world[2]),
        {10, 60}, {154, 50, 205});

      tools::draw_text(
        img,
        fmt::format(
          "in world frame yaw:{:.2f}  pitch:{:.2f}  roll:{:.2f}", armor.ypr_in_world[0],
          armor.ypr_in_world[1], armor.ypr_in_world[2]),
        {10, 120}, {154, 50, 205});

      nlohmann::json data;

      data["armor_0_pixel_x"] = armor.points[0].x;
      data["armor_0_pixel_y"] = armor.points[0].y;
      data["armor_1_pixel_x"] = armor.points[1].x;
      data["armor_1_pixel_y"] = armor.points[1].y;
      data["armor_2_pixel_x"] = armor.points[2].x;
      data["armor_2_pixel_y"] = armor.points[2].y;
      data["armor_3_pixel_x"] = armor.points[3].x;
      data["armor_3_pixel_y"] = armor.points[3].y;

      data["armor_x"] = armor.xyz_in_world[0];
      data["armor_y"] = armor.xyz_in_world[1];
      data["armor_z"] = armor.xyz_in_world[2];
      data["armor_dis"] = std::sqrt(
        armor.xyz_in_world[0] * armor.xyz_in_world[0] +
        armor.xyz_in_world[1] * armor.xyz_in_world[1]);

      data["armor_yaw"] = armor.ypr_in_world[0] * 57.3;
      data["armor_pitch"] = armor.ypr_in_world[1] * 57.3;
      data["armor_roll"] = armor.ypr_in_world[2] * 57.3;

      data["armor_yaw_in_gimbal"] = armor.ypr_in_gimbal[0] * 57.3;
      data["armor_pitch_in_gimbal"] = armor.ypr_in_gimbal[1] * 57.3;
      data["armor_roll_in_gimbal"] = armor.ypr_in_gimbal[2] * 57.3;

      plotter.plot(data);
    }

    cv::resize(img, img, {}, 0.5, 0.5);  // 显示时缩小图片尺寸
    cv::imshow("reprojection", img);
    auto key = cv::waitKey(1);
    if (key == 'q') break;
  }

  return 0;
}