#include <memory>
#include <string>
#include <chrono>

#include <mavros_msgs/srv/command_tol.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/string.hpp>

class ControlNode : public rclcpp::Node
{
public:
  ControlNode()
  : Node("control_node")
  {
    hold_mode_ = declare_parameter<std::string>("hold_mode", "BRAKE");
    hold_request_timeout_sec_ = declare_parameter<double>("hold_request_timeout_sec", 5.0);
    land_request_timeout_sec_ = declare_parameter<double>("land_request_timeout_sec", 8.0);
    service_wait_timeout_sec_ = declare_parameter<double>("service_wait_timeout_sec", 0.5);

    abort_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/sentinel/mission/abort", 10,
      std::bind(&ControlNode::abort_callback, this, std::placeholders::_1));

    abort_status_pub_ = create_publisher<std_msgs::msg::String>(
      "/sentinel/mission/abort_status", 10);

    set_mode_client_ = create_client<mavros_msgs::srv::SetMode>("/mavros/set_mode");
    land_client_ = create_client<mavros_msgs::srv::CommandTOL>("/mavros/cmd/land");

    RCLCPP_INFO(get_logger(), "control_node started");
  }

private:
  enum class AbortState
  {
    Idle,
    AbortRequested,
    HoldRequested,
    HoldSucceeded,
    HoldFailed,
    LandRequested,
    LandSucceeded,
    LandFailed,
    Completed,
    Failed
  };

  void abort_callback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    if (!msg->data) {
      return;
    }

    if (abort_state_ != AbortState::Idle) {
      publish_abort_status("abort_ignored_in_progress");
      return;
    }

    transition_to(AbortState::AbortRequested);
    request_hold();
  }

  void request_hold()
  {
    transition_to(AbortState::HoldRequested);

    if (!set_mode_client_->wait_for_service(to_duration(service_wait_timeout_sec_))) {
      publish_abort_status("hold_service_unavailable");
      transition_to(AbortState::HoldFailed);
      request_land();
      return;
    }

    arm_hold_timeout_timer();

    auto req = std::make_shared<mavros_msgs::srv::SetMode::Request>();
    req->custom_mode = hold_mode_;

    set_mode_client_->async_send_request(
      req,
      [this](rclcpp::Client<mavros_msgs::srv::SetMode>::SharedFuture future) {
        if (abort_state_ != AbortState::HoldRequested) {
          return;
        }

        cancel_timer(hold_timeout_timer_);

        try {
          const auto response = future.get();
          if (response->mode_sent) {
            transition_to(AbortState::HoldSucceeded);
          } else {
            publish_abort_status("hold_command_rejected");
            transition_to(AbortState::HoldFailed);
          }
        } catch (const std::exception & ex) {
          RCLCPP_ERROR(get_logger(), "hold request failed: %s", ex.what());
          publish_abort_status("hold_request_exception");
          transition_to(AbortState::HoldFailed);
        }

        request_land();
      });
  }

  void request_land()
  {
    transition_to(AbortState::LandRequested);

    if (!land_client_->wait_for_service(to_duration(service_wait_timeout_sec_))) {
      publish_abort_status("land_service_unavailable");
      transition_to(AbortState::LandFailed);
      finalize_abort(false);
      return;
    }

    arm_land_timeout_timer();

    auto req = std::make_shared<mavros_msgs::srv::CommandTOL::Request>();

    land_client_->async_send_request(
      req,
      [this](rclcpp::Client<mavros_msgs::srv::CommandTOL>::SharedFuture future) {
        if (abort_state_ != AbortState::LandRequested) {
          return;
        }

        cancel_timer(land_timeout_timer_);

        try {
          const auto response = future.get();
          if (response->success) {
            transition_to(AbortState::LandSucceeded);
            finalize_abort(true);
            return;
          }
          publish_abort_status("land_command_rejected");
        } catch (const std::exception & ex) {
          RCLCPP_ERROR(get_logger(), "land request failed: %s", ex.what());
          publish_abort_status("land_request_exception");
        }

        transition_to(AbortState::LandFailed);
        finalize_abort(false);
      });
  }

  void finalize_abort(bool success)
  {
    if (success) {
      transition_to(AbortState::Completed);
    } else {
      transition_to(AbortState::Failed);
    }

    abort_state_ = AbortState::Idle;
  }

  void arm_hold_timeout_timer()
  {
    cancel_timer(hold_timeout_timer_);
    hold_timeout_timer_ = create_wall_timer(
      to_duration(hold_request_timeout_sec_),
      [this]() {
        cancel_timer(hold_timeout_timer_);
        if (abort_state_ != AbortState::HoldRequested) {
          return;
        }
        publish_abort_status("hold_request_timeout");
        transition_to(AbortState::HoldFailed);
        request_land();
      });
  }

  void arm_land_timeout_timer()
  {
    cancel_timer(land_timeout_timer_);
    land_timeout_timer_ = create_wall_timer(
      to_duration(land_request_timeout_sec_),
      [this]() {
        cancel_timer(land_timeout_timer_);
        if (abort_state_ != AbortState::LandRequested) {
          return;
        }
        publish_abort_status("land_request_timeout");
        transition_to(AbortState::LandFailed);
        finalize_abort(false);
      });
  }

  void cancel_timer(rclcpp::TimerBase::SharedPtr & timer)
  {
    if (timer) {
      timer->cancel();
      timer.reset();
    }
  }

  static std::chrono::duration<double> to_duration(double seconds)
  {
    if (seconds < 0.0) {
      return std::chrono::duration<double>(0.0);
    }
    return std::chrono::duration<double>(seconds);
  }

  void transition_to(AbortState next_state)
  {
    if (abort_state_ == next_state) {
      return;
    }

    if (!is_valid_transition(abort_state_, next_state)) {
      RCLCPP_WARN(
        get_logger(), "invalid abort transition: %s -> %s",
        state_to_string(abort_state_).c_str(), state_to_string(next_state).c_str());
      publish_abort_status("abort_transition_invalid");
      return;
    }

    abort_state_ = next_state;
    publish_abort_status(state_to_string(abort_state_));
  }

  static bool is_valid_transition(AbortState current, AbortState next_state)
  {
    switch (current) {
      case AbortState::Idle:
        return next_state == AbortState::AbortRequested;
      case AbortState::AbortRequested:
        return next_state == AbortState::HoldRequested;
      case AbortState::HoldRequested:
        return next_state == AbortState::HoldSucceeded || next_state == AbortState::HoldFailed;
      case AbortState::HoldSucceeded:
      case AbortState::HoldFailed:
        return next_state == AbortState::LandRequested;
      case AbortState::LandRequested:
        return next_state == AbortState::LandSucceeded || next_state == AbortState::LandFailed;
      case AbortState::LandSucceeded:
        return next_state == AbortState::Completed;
      case AbortState::LandFailed:
        return next_state == AbortState::Failed;
      case AbortState::Completed:
      case AbortState::Failed:
        return false;
      default:
        return false;
    }
  }

  static std::string state_to_string(AbortState state)
  {
    switch (state) {
      case AbortState::Idle:
        return "idle";
      case AbortState::AbortRequested:
        return "abort_requested";
      case AbortState::HoldRequested:
        return "hold_requested";
      case AbortState::HoldSucceeded:
        return "hold_succeeded";
      case AbortState::HoldFailed:
        return "hold_failed";
      case AbortState::LandRequested:
        return "land_requested";
      case AbortState::LandSucceeded:
        return "land_succeeded";
      case AbortState::LandFailed:
        return "land_failed";
      case AbortState::Completed:
        return "abort_completed";
      case AbortState::Failed:
        return "abort_failed";
      default:
        return "unknown";
    }
  }

  void publish_abort_status(const std::string & state)
  {
    std_msgs::msg::String msg;
    msg.data = state;
    abort_status_pub_->publish(msg);
  }

  std::string hold_mode_;
  double hold_request_timeout_sec_{5.0};
  double land_request_timeout_sec_{8.0};
  double service_wait_timeout_sec_{0.5};

  AbortState abort_state_{AbortState::Idle};

  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr abort_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr abort_status_pub_;

  rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
  rclcpp::Client<mavros_msgs::srv::CommandTOL>::SharedPtr land_client_;

  rclcpp::TimerBase::SharedPtr hold_timeout_timer_;
  rclcpp::TimerBase::SharedPtr land_timeout_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ControlNode>());
  rclcpp::shutdown();
  return 0;
}
