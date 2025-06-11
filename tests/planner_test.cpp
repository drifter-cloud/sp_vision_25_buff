#include "tasks/auto_aim/planner/planner.hpp"

#include <chrono>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <thread>

#include "io/gimbal/gimbal.hpp"
#include "tools/exiter.hpp"
#include "tools/logger.hpp"
#include "tools/math_tools.hpp"
#include "tools/plotter.hpp"

using namespace std::chrono_literals;

const std::string keys =
  "{help h usage ? | | 输出命令行参数说明}"
  "{@config-path   | | yaml配置文件路径 }";

int main(int argc, char * argv[])
{
  cv::CommandLineParser cli(argc, argv, keys);
  auto config_path = cli.get<std::string>("@config-path");
  if (cli.has("help") || !cli.has("@config-path")) {
    cli.printMessage();
    return 0;
  }

  tools::Exiter exiter;
  tools::Plotter plotter;

  io::Gimbal gimbal(config_path);
  auto_aim::Planner planner(config_path);
  auto_aim::Target target(2, -5.0, 0.2, 0.1);

  auto t0 = std::chrono::steady_clock::now();

  while (!exiter.exit()) {
    target.predict(0.01);

    auto gs = gimbal.state();
    auto plan = planner.plan(target, gs);

    gimbal.send(
      plan.control, plan.fire, plan.yaw, plan.vyaw, plan.yaw_torque, plan.pitch, plan.vpitch,
      plan.pitch_torque);

    nlohmann::json data;
    data["t"] = tools::delta_time(std::chrono::steady_clock::now(), t0);
    data["yaw"] = gs.yaw;
    data["vyaw"] = gs.vyaw;
    data["pitch"] = gs.pitch;
    data["vpitch"] = gs.vpitch;
    data["yaw_ref"] = plan.yaw;
    data["vyaw_ref"] = plan.vyaw;
    data["yaw_torque"] = plan.yaw_torque;
    data["pitch_ref"] = plan.pitch;
    data["vpitch_ref"] = plan.vpitch;
    data["pitch_torque"] = plan.pitch_torque;
    plotter.plot(data);

    std::this_thread::sleep_for(10ms);
  }

  gimbal.send(false, false, 0, 0, 0, 0, 0, 0);

  return 0;
}