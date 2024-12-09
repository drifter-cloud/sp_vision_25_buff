#include <fmt/core.h>

#include <chrono>
#include <future>
#include <mutex>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <thread>

#include "io/camera.hpp"
#include "io/cboard.hpp"
#include "io/usbcamera/usbcamera.hpp"
#include "tasks/auto_aim_sentry/aimer.hpp"
#include "tasks/auto_aim_sentry/solver.hpp"
#include "tasks/auto_aim_sentry/tracker.hpp"
#include "tasks/auto_aim_sentry/yolov8.hpp"
#include "tasks/omniperception/decider.hpp"
#include "tools/exiter.hpp"
#include "tools/img_tools.hpp"
#include "tools/logger.hpp"
#include "tools/math_tools.hpp"
#include "tools/plotter.hpp"
#include "tools/recorder.hpp"

using namespace std::chrono;

const std::string keys =
  "{help h usage ? |                     | 输出命令行参数说明}"
  "{d debug        |                     | 显示视频流}"
  "{@config-path   | configs/newsentry.yaml | 位置参数，yaml配置文件路径 }";

int main(int argc, char * argv[])
{
  tools::Exiter exiter;
  tools::Plotter plotter;
  tools::Recorder recorder;

  cv::CommandLineParser cli(argc, argv, keys);
  if (cli.has("help")) {
    cli.printMessage();
    return 0;
  }
  auto config_path = cli.get<std::string>(0);
  auto debug = cli.has("debug");

  io::CBoard cboard(config_path);
  io::Camera camera(config_path);
  io::USBCamera usbcam1("video0", config_path);
  io::USBCamera usbcam2("video2", config_path);
  io::USBCamera usbcam3("video4", config_path);
  io::USBCamera usbcam4("video6", config_path);

  auto_aim::YOLOV8 yolov8(config_path, false);
  auto_aim::Solver solver(config_path);
  auto_aim::Tracker tracker(config_path, solver);
  auto_aim::Aimer aimer(config_path);

  omniperception::Decider decider(config_path);

  cv::Mat img;
  cv::Mat usb_img1, usb_img2, usb_img3, usb_img4;

  std::chrono::steady_clock::time_point timestamp;
  auto last_stamp = std::chrono::steady_clock::now();
  io::Command last_command;

  while (!exiter.exit()) {
    camera.read(img, timestamp);
    Eigen::Quaterniond q = cboard.imu_at(timestamp - 1ms);
    auto dt = tools::delta_time(timestamp, last_stamp);
    last_stamp = timestamp;

    tools::logger()->info("{:.2f} fps", 1 / dt);
    // recorder.record(img, q, timestamp);

    /// 自瞄核心逻辑
    solver.set_R_gimbal2world(q);

    Eigen::Vector3d gimbal_pos = tools::eulers(solver.R_gimbal2world(), 2, 1, 0);

    auto armors = yolov8.detect(img);

    decider.armor_filter(armors);

    decider.set_priority(armors);

    auto targets = tracker.track(armors, timestamp);

    io::Command command{false, false, 0, 0};

    /// 全向感知逻辑
    if (tracker.state() == "lost")
      command = decider.decide(yolov8, gimbal_pos, usbcam1, usbcam2, usbcam3, usbcam4);
    else
      command = aimer.aim(targets, timestamp, cboard.bullet_speed);

    if (
      command.control && aimer.debug_aim_point.valid &&
      std::abs(last_command.yaw - command.yaw) * 57.3 < 2 &&
      std::abs(gimbal_pos[0] - last_command.yaw) * 57.3 < 1.5) {  //应该减去上一次command的yaw值
      tools::logger()->debug("#####shoot#####");
      command.shoot = true;
    }

    if (command.control) last_command = command;

    cboard.send(command);

    /// 调试输出
    if (debug) {
      tools::draw_text(img, fmt::format("[{}]", tracker.state()), {10, 30}, {255, 255, 255});

      nlohmann::json data;

      data["fps"] = 1 / dt;
      data["shoot"] = command.shoot;
      // 装甲板原始观测数据
      data["armor_num"] = armors.size();
      if (!armors.empty()) {
        const auto & armor = armors.front();
        data["armor_x"] = armor.xyz_in_world[0];
        data["armor_y"] = armor.xyz_in_world[1];
        data["armor_yaw"] = armor.ypr_in_world[0] * 57.3;
      }

      if (!targets.empty()) {
        auto target = targets.front();

        // 当前帧target更新后
        std::vector<Eigen::Vector4d> armor_xyza_list = target.armor_xyza_list();
        for (const Eigen::Vector4d & xyza : armor_xyza_list) {
          auto image_points =
            solver.reproject_armor(xyza.head(3), xyza[3], target.armor_type, target.name);
          tools::draw_points(img, image_points, {0, 255, 0});
        }

        // aimer瞄准位置
        auto aim_point = aimer.debug_aim_point;
        Eigen::Vector4d aim_xyza = aim_point.xyza;
        auto image_points =
          solver.reproject_armor(aim_xyza.head(3), aim_xyza[3], target.armor_type, target.name);
        if (aim_point.valid)
          tools::draw_points(img, image_points, {0, 0, 255});  // red
        else
          tools::draw_points(img, image_points, {255, 0, 0});  // blue

        // 观测器内部数据
        Eigen::VectorXd x = target.ekf_x();
        data["x"] = x[0];
        data["vx"] = x[1];
        data["y"] = x[2];
        data["vy"] = x[3];
        data["z"] = x[4];
        data["vz"] = x[5];
        data["a"] = x[6] * 57.3;
        data["w"] = x[7];
        data["r"] = x[8];
        data["l"] = x[9];
        data["h"] = x[10];
        data["last_id"] = target.last_id;
      }

      // 云台响应情况
      Eigen::Vector3d ypr = tools::eulers(solver.R_gimbal2world(), 2, 1, 0);
      data["gimbal_yaw"] = ypr[0] * 57.3;
      data["gimbal_pitch"] = -ypr[1] * 57.3;

      if (command.control) {
        data["cmd_yaw"] = command.yaw * 57.3;
        data["cmd_pitch"] = command.pitch * 57.3;
      }

      plotter.plot(data);

      cv::resize(img, img, {}, 0.5, 0.5);  // 显示时缩小图片尺寸
      cv::imshow("reprojection", img);
      auto key = cv::waitKey(1);
      if (key == 'q') break;
    }
  }

  return 0;
}