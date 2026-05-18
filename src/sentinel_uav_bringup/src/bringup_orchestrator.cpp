#include <chrono>
#include <cstdio>
#include <memory>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/command_tol.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/string.hpp>

using namespace std::chrono_literals;

class BringupOrchestrator : public rclcpp::Node
{
public:
  BringupOrchestrator()
  : Node("bringup_orchestrator")
  {
    check_period_ms_ = declare_parameter<int>("check_period_ms", 500);
    stale_pose_sec_ = declare_parameter<double>("pose_stale_threshold_sec", 1.0);

    auto qos = rclcpp::QoS(10).best_effort();

    state_sub_ = create_subscription<mavros_msgs::msg::State>(
      "/mavros/state", qos,
      [this](const mavros_msgs::msg::State::SharedPtr msg) {
        last_state_ = msg;
        last_state_stamp_ = now();
      });

    pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/mavros/local_position/pose", qos,
      [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
        (void)msg;
        last_pose_stamp_ = now();
      });

    battery_sub_ = create_subscription<sensor_msgs::msg::BatteryState>(
      "/mavros/battery", qos,
      [this](const sensor_msgs::msg::BatteryState::SharedPtr msg) {
        (void)msg;
        last_battery_stamp_ = now();
      });

    set_mode_client_ = create_client<mavros_msgs::srv::SetMode>("/mavros/set_mode");
    arming_client_ = create_client<mavros_msgs::srv::CommandBool>("/mavros/cmd/arming");
    takeoff_client_ = create_client<mavros_msgs::srv::CommandTOL>("/mavros/cmd/takeoff");
    land_client_ = create_client<mavros_msgs::srv::CommandTOL>("/mavros/cmd/land");

    ready_pub_ = create_publisher<std_msgs::msg::Bool>("/sentinel/bringup/ready", 10);
    reason_pub_ = create_publisher<std_msgs::msg::String>("/sentinel/bringup/reason", 10);

    timer_ = create_wall_timer(
      std::chrono::milliseconds(check_period_ms_),
      std::bind(&BringupOrchestrator::tick, this));

    RCLCPP_INFO(get_logger(), "bringup_orchestrator started");
  }

private:
  void tick()
  {
    const bool has_state = static_cast<bool>(last_state_);
    const bool fcu_connected = has_state && last_state_->connected;
    const bool pose_fresh = (now() - last_pose_stamp_).seconds() <= stale_pose_sec_;
    const bool battery_seen = (now() - last_battery_stamp_).seconds() <= 5.0;

    const bool services_ready =
      set_mode_client_->service_is_ready() &&
      arming_client_->service_is_ready() &&
      takeoff_client_->service_is_ready() &&
      land_client_->service_is_ready();

    const bool ready = has_state && fcu_connected && pose_fresh && battery_seen && services_ready;

    std_msgs::msg::Bool ready_msg;
    ready_msg.data = ready;
    ready_pub_->publish(ready_msg);

    std_msgs::msg::String reason_msg;
    reason_msg.data = build_reason(
      has_state, fcu_connected, pose_fresh, battery_seen, services_ready);
    reason_pub_->publish(reason_msg);

    if (ready != last_ready_ || reason_msg.data != last_reason_) {
      RCLCPP_INFO(
        get_logger(), "bringup ready=%s reason=%s", ready ? "true" : "false",
        reason_msg.data.c_str());
      last_ready_ = ready;
      last_reason_ = reason_msg.data;
    }
  }

  static std::string build_reason(
    bool has_state,
    bool fcu_connected,
    bool pose_fresh,
    bool battery_seen,
    bool services_ready)
  {
    if (!has_state) {
      return "waiting_state";
    }
    if (!fcu_connected) {
      return "waiting_fcu_connection";
    }
    if (!pose_fresh) {
      return "waiting_fresh_pose";
    }
    if (!battery_seen) {
      return "waiting_battery";
    }
    if (!services_ready) {
      return "waiting_mavros_services";
    }
    return "ready";
  }

  int check_period_ms_{500};
  double stale_pose_sec_{1.0};

  rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
  rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr battery_sub_;

  rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
  rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arming_client_;
  rclcpp::Client<mavros_msgs::srv::CommandTOL>::SharedPtr takeoff_client_;
  rclcpp::Client<mavros_msgs::srv::CommandTOL>::SharedPtr land_client_;

  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr ready_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr reason_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  mavros_msgs::msg::State::SharedPtr last_state_;
  rclcpp::Time last_state_stamp_{0, 0, RCL_ROS_TIME};
  rclcpp::Time last_pose_stamp_{0, 0, RCL_ROS_TIME};
  rclcpp::Time last_battery_stamp_{0, 0, RCL_ROS_TIME};

  bool last_ready_{false};
  std::string last_reason_{"uninitialized"};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BringupOrchestrator>());
  rclcpp::shutdown();
  return 0;
}
