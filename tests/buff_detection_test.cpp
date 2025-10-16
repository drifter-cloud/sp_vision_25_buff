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
    // 修改为使用绝对路径
    auto_buff::Buff_Detector detector("/home/moran/bbs/sp_vision_25/configs/sentry.yaml");
    
    // 读取输入图像
    cv::Mat img = cv::imread(argv[1]);
    if (img.empty()) {
        tools::logger()->error("Failed to load image: {}", argv[1]);
        return -1;
    }

    // 执行目标检测
    auto result = detector.detect_24(img);
    
    // 可视化结果
    if (result.has_value()) {
        const auto& p = result.value();
        
        // 获取扇叶目标
        auto& target = p.target();
        
        // 绘制扇叶关键点
        for (int i = 0; i < 4; i++) {
            cv::circle(img, target.points[i], 5, cv::Scalar(0, 0, 255), -1);
        }
        
        // 绘制扇叶中心
        cv::circle(img, target.center, 5, cv::Scalar(0, 255, 0), -1);
        
        // 绘制能量机关中心
        cv::circle(img, p.r_center, 5, cv::Scalar(255, 0, 0), -1);
    }

    // 创建输出目录
    std::filesystem::create_directories("/home/moran/bbs/sp_vision_25/assets/detection_results");

    // 保存结果图片到指定目录
    std::string input_path = argv[1];
    size_t last_slash = input_path.find_last_of("/\\");
    std::string input_filename = (last_slash != std::string::npos) ? input_path.substr(last_slash + 1) : input_path;
    size_t dot_pos = input_filename.find_last_of(".");
    std::string base_name = (dot_pos != std::string::npos) ? input_filename.substr(0, dot_pos) : input_filename;
    
    std::string output_filename = "/home/moran/bbs/sp_vision_25/assets/detection_results/" + base_name + "_result.jpg";
    cv::imwrite(output_filename, img);
    tools::logger()->info("检测结果已保存为: {}", output_filename);

  // 显示结果
    cv::imshow("Detection Result", img);

    cv::waitKey(0);
    
    return 0;
}