#include "perceptron.hpp"

#include <chrono>
#include <memory>
#include <thread>

#include "tasks/auto_aim/yolov8.hpp"
#include "tools/exiter.hpp"
#include "tools/logger.hpp"

namespace omniperception
{
Perceptron::Perceptron(
  io::USBCamera * usbcam1, io::USBCamera * usbcam2, io::USBCamera * usbcam3,
  io::USBCamera * usbcam4, const std::string & config_path)
: detection_queue_(10), decider_(config_path), stop_flag_(false)
{
  // 初始化 YOLO 模型
  yolov8_parallel1_ = std::make_shared<auto_aim::YOLOV8>(config_path, false);
  yolov8_parallel2_ = std::make_shared<auto_aim::YOLOV8>(config_path, false);
  yolov8_parallel3_ = std::make_shared<auto_aim::YOLOV8>(config_path, false);
  yolov8_parallel4_ = std::make_shared<auto_aim::YOLOV8>(config_path, false);

  std::this_thread::sleep_for(std::chrono::seconds(2));
  // 创建四个线程进行并行推理
  threads_.emplace_back([&] { parallel_infer(usbcam1, yolov8_parallel1_); });
  threads_.emplace_back([&] { parallel_infer(usbcam2, yolov8_parallel2_); });
  threads_.emplace_back([&] { parallel_infer(usbcam3, yolov8_parallel3_); });
  threads_.emplace_back([&] { parallel_infer(usbcam4, yolov8_parallel4_); });

  tools::logger()->info("Perceptron initialized.");
}

Perceptron::~Perceptron()
{
  {
    std::unique_lock<std::mutex> lock(mutex_);
    stop_flag_ = true;  // 设置退出标志
  }
  condition_.notify_all();  // 唤醒所有等待的线程

  // 等待线程结束
  for (auto & t : threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  tools::logger()->info("Perceptron destructed.");
}

tools::ThreadSafeQueue<DetectionResult> Perceptron::get_detection_queue()
{
  tools::ThreadSafeQueue<DetectionResult> queue_copy(1);
  {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_copy = detection_queue_;
    // 获取队列副本
  }

  // 清空队列
  if (!detection_queue_.empty()) {
    detection_queue_.clear();
  }

  return queue_copy;
}

// 将并行推理逻辑移动到类成员函数
void Perceptron::parallel_infer(
  io::USBCamera * cam, std::shared_ptr<auto_aim::YOLOV8> & yolov8_parallel)
{
  if (!cam) {
    tools::logger()->error("Camera pointer is null!");
    return;
  }
  try {
    while (true) {
      cv::Mat usb_img;
      std::chrono::steady_clock::time_point ts;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_flag_) break;  // 检查是否需要退出
      }

      cam->read(usb_img, ts);
      if (usb_img.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        continue;
      }

      auto armors = yolov8_parallel->detect(usb_img);
      if (!armors.empty()) {
        auto delta_angle = decider_.delta_angle(armors, cam->device_name);

        DetectionResult dr;
        dr.armors = std::move(armors);
        dr.timestamp = ts;
        dr.delta_yaw = delta_angle[0] / 57.3;
        dr.delta_pitch = delta_angle[1] / 57.3;
        {
          std::unique_lock<std::mutex> lock(mutex_);
          detection_queue_.push(dr);  // 推入线程安全队列
        }
      }
    }
  } catch (const std::exception & e) {
    tools::logger()->error("Exception in parallel_infer: {}", e.what());
  }
}

}  // namespace omniperception
