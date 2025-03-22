#include "subscribe2nav.hpp"

#include <sstream>
#include <vector>

namespace io
{

Subscribe2Nav::Subscribe2Nav() : Node("enemy_status_subscriber"), queue_(1)
{
  subscription_ = this->create_subscription<sp_msgs::msg::EnemyStatusMsg>(
    "enemy_status", 10, std::bind(&Subscribe2Nav::callback, this, std::placeholders::_1));

  RCLCPP_INFO(this->get_logger(), "enemy_status_subscriber node initialized.");
}

Subscribe2Nav::~Subscribe2Nav()
{
  RCLCPP_INFO(this->get_logger(), "enemy_status_subscriber node shutting down.");
}

void Subscribe2Nav::callback(const sp_msgs::msg::EnemyStatusMsg::SharedPtr msg)
{
  queue_.clear();
  queue_.push(*msg);
}

void Subscribe2Nav::start()
{
  RCLCPP_INFO(this->get_logger(), "enemy_status_subscriber node Starting to spin...");
  rclcpp::spin(this->shared_from_this());
}

std::vector<int8_t> Subscribe2Nav::subscribe_data()
{
  if (queue_.empty()) {
    // RCLCPP_INFO(this->get_logger(), "No enemy_status detected !");
    return std::vector<int8_t>();
  }
  sp_msgs::msg::EnemyStatusMsg msg;

  queue_.back(msg);
  RCLCPP_INFO(
    this->get_logger(), "Subscribe enemy_status at: %d.%09u", msg.timestamp.sec,
    msg.timestamp.nanosec);

  return msg.invincible_enemy_ids;
}

}  // namespace io
