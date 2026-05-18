#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <std_msgs/msg/string.hpp>

using namespace std::chrono_literals;

class ObserverNode : public rclcpp::Node
{
public:
  ObserverNode()
  : Node("observer_node")
  {
    low_voltage_warn_v_ = declare_parameter<double>("low_voltage_warn_v", 14.4);
    critical_voltage_v_ = declare_parameter<double>("critical_voltage_v", 14.0);
    publish_hz_ = declare_parameter<double>("publish_hz", 1.0);

    auto qos = rclcpp::QoS(10).best_effort();

    state_sub_ = create_subscription<mavros_msgs::msg::State>(
      "/mavros/state", qos,
      [this](const mavros_msgs::msg::State::SharedPtr msg) {
        state_ = msg;
      });

    pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/mavros/local_position/pose", qos,
      [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
        pose_ = msg;
      });

    battery_sub_ = create_subscription<sensor_msgs::msg::BatteryState>(
      "/mavros/battery", qos,
      [this](const sensor_msgs::msg::BatteryState::SharedPtr msg) {
        battery_ = msg;
      });

    health_pub_ = create_publisher<std_msgs::msg::String>("/sentinel/health/status", 10);

    const auto period_ms = static_cast<int>(std::round(1000.0 / std::max(0.2, publish_hz_)));
    timer_ = create_wall_timer(
      std::chrono::milliseconds(period_ms),
      std::bind(&ObserverNode::publish_health, this));

    RCLCPP_INFO(get_logger(), "observer_node started");
  }

private:
  void publish_health()
  {
    std_msgs::msg::String msg;
    msg.data = compose_status();
    health_pub_->publish(msg);
  }

  std::string compose_status() const
  {
    std::ostringstream out;

    const bool has_state = static_cast<bool>(state_);
    const bool has_pose = static_cast<bool>(pose_);
    const bool has_battery = static_cast<bool>(battery_);

    const std::string mode = has_state ? state_->mode : "N/A";
    const bool connected = has_state && state_->connected;
    const bool armed = has_state && state_->armed;

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    if (has_pose) {
      x = pose_->pose.position.x;
      y = pose_->pose.position.y;
      z = pose_->pose.position.z;
    }

    float voltage = std::nanf("");
    float percentage = std::nanf("");
    std::string power_state = "UNKNOWN";

    if (has_battery) {
      voltage = battery_->voltage;
      percentage = battery_->percentage;

      if (!std::isnan(voltage)) {
        if (voltage <= critical_voltage_v_) {
          power_state = "CRITICAL";
        } else if (voltage <= low_voltage_warn_v_) {
          power_state = "WARN";
        } else {
          power_state = "OK";
        }
      }
    }

    out << std::fixed << std::setprecision(3);
    out << "{";
    out << "\"connected\":" << (connected ? "true" : "false") << ",";
    out << "\"armed\":" << (armed ? "true" : "false") << ",";
    out << "\"mode\":\"" << escape_json(mode) << "\",";
    out << "\"pose\":{";
    out << "\"x\":" << x << ",";
    out << "\"y\":" << y << ",";
    out << "\"z\":" << z << "},";
    if (std::isnan(voltage)) {
      out << "\"battery_v\":null,";
    } else {
      out << "\"battery_v\":" << voltage << ",";
    }
    if (std::isnan(percentage)) {
      out << "\"battery_pct\":null,";
    } else {
      out << "\"battery_pct\":" << percentage << ",";
    }
    out << "\"power_state\":\"" << power_state << "\"";
    out << "}";

    return out.str();
  }

  static std::string escape_json(const std::string & input)
  {
    std::ostringstream escaped;
    for (const char c : input) {
      switch (c) {
        case '"':
          escaped << "\\\"";
          break;
        case '\\':
          escaped << "\\\\";
          break;
        case '\n':
          escaped << "\\n";
          break;
        case '\r':
          escaped << "\\r";
          break;
        case '\t':
          escaped << "\\t";
          break;
        default:
          escaped << c;
          break;
      }
    }
    return escaped.str();
  }

  double low_voltage_warn_v_{14.4};
  double critical_voltage_v_{14.0};
  double publish_hz_{1.0};

  rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
  rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr battery_sub_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr health_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  mavros_msgs::msg::State::SharedPtr state_;
  geometry_msgs::msg::PoseStamped::SharedPtr pose_;
  sensor_msgs::msg::BatteryState::SharedPtr battery_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ObserverNode>());
  rclcpp::shutdown();
  return 0;
}
