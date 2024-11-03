#include <chrono>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <thread>

#include "io/cboard.hpp"
#include "io/command.hpp"
#include "tools/exiter.hpp"
#include "tools/logger.hpp"
#include "tools/math_tools.hpp"
#include "tools/plotter.hpp"

using namespace std::chrono_literals;

const std::string keys =
  "{help h usage ? |                     | 输出命令行参数说明}"
  "{delta-angle a  |         30          | yaw轴delta角}"
  "{slice       s  |         250         | delta_angle的切片数}"
  "{signal-mode m  |     triangle_wave   | 发送信号的模式}"
  "{@config-path   | configs/sentry.yaml | 位置参数，yaml配置文件路径 }";

int main(int argc, char * argv[])
{
  cv::CommandLineParser cli(argc, argv, keys);
  auto config_path = cli.get<std::string>(0);
  auto delta_angle = cli.get<double>("delta-angle");
  auto slice = cli.get<double>("slice");
  auto signal_mode = cli.get<std::string>("signal-mode");
  if (cli.has("help") || config_path.empty()) {
    cli.printMessage();
    return 0;
  }

  tools::Exiter exiter;
  tools::Plotter plotter;

  io::CBoard cboard(config_path);

  auto init_yaw = 0;
  auto dyaw = delta_angle / slice;
  double cmd_yaw = init_yaw;
  double error = 0;
  int count = 0;
  io::Command init_command{1, 0, 0, 0};
  cboard.send(init_command);
  std::this_thread::sleep_for(1s);  //等待云台归零

  while (!exiter.exit()) {
    io::Command command;
    io::Command last_command;

    nlohmann::json data;
    auto timestamp = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(1ms);

    Eigen::Quaterniond q = cboard.imu_at(timestamp);

    Eigen::Vector3d eulers = tools::eulers(q, 2, 1, 0);

    if (signal_mode == "triangle_wave") {
      if (count == slice) {
        cmd_yaw = init_yaw;
        error /= slice;
        tools::logger()->info("average static error is {:.4f} degree", error * 57.3);
        command = {1, 0, cmd_yaw / 57.3, 0};
        count = 0;

      } else {
        cmd_yaw += dyaw;
        error += last_command.yaw - eulers[0];
        command = {1, 0, cmd_yaw / 57.3, 0};
        count++;
      }

      cboard.send(command);
      data["cmd_yaw"] = last_command.yaw * 57.3;
      data["gimbal_yaw"] = eulers[0] * 57.3;
      last_command = command;
      plotter.plot(data);
      std::this_thread::sleep_for(8ms);  //模拟自瞄100fps
    } else if (signal_mode == "step") {
      cmd_yaw = delta_angle;
      command = {1, 0, cmd_yaw / 57.3, 0};

      cboard.send(command);
      data["cmd_yaw"] = last_command.yaw * 57.3;
      data["gimbal_yaw"] = eulers[0] * 57.3;
      last_command = command;
      plotter.plot(data);
      std::this_thread::sleep_for(8ms);  //模拟自瞄100fps
    }
  }
  return 0;
}