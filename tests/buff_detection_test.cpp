#include <opencv2/opencv.hpp>
#include "tasks/auto_buff/buff_detector.hpp"
#include "tools/logger.hpp"

int main(int argc, char * argv[]) 
{
    if (argc < 2) {
        tools::logger()->error("Usage: ./buff_detection_test <image_path>");
        return -1;
    }

    // 初始化检测器
    auto_buff::Buff_Detector detector("configs/sentry.yaml");
    
    // 读取输入图像
    cv::Mat img = cv::imread(argv[1]);
    if (img.empty()) {
        tools::logger()->error("Failed to load image: {}", argv[1]);
        return -1;
    }

    // 执行目标检测
    auto result = detector.detect(img);
    
    // 可视化结果
    if (result.has_value()) {
        const auto& p = result.value();
        
        // 绘制扇叶关键点
        for (int i = 0; i < 4; i++) {
            cv::circle(img, p.target().points[i], 5, cv::Scalar(0, 0, 255), -1);
        }
        
        // 绘制扇叶中心
        cv::circle(img, p.target().center, 5, cv::Scalar(0, 255, 0), -1);
        
        // 绘制能量机关中心
        cv::circle(img, p.r_center, 5, cv::Scalar(255, 0, 0), -1);
    }

    // 显示结果
    cv::imshow("Detection Result", img);
    cv::waitKey(0);
    
    return 0;
}