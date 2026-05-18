#include <memory>
#include <string>
#include <chrono>
#include <functional>

#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/command_tol.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/string.hpp>

class ControlNode : public rclcpp::Node
{
public:
  ControlNode()
  : Node("control_node")
  {
    hold_mode_ = declare_parameter<std::string>("hold_mode", "BRAKE");
    guided_mode_ = declare_parameter<std::string>("guided_mode", "GUIDED");

    arm_request_timeout_sec_ = declare_parameter<double>("arm_request_timeout_sec", 8.0);
    takeoff_request_timeout_sec_ = declare_parameter<double>("takeoff_request_timeout_sec", 30.0);
    hold_request_timeout_sec_ = declare_parameter<double>("hold_request_timeout_sec", 5.0);
    land_request_timeout_sec_ = declare_parameter<double>("land_request_timeout_sec", 8.0);
    service_wait_timeout_sec_ = declare_parameter<double>("service_wait_timeout_sec", 0.5);
    fcu_state_stale_sec_ = declare_parameter<double>("fcu_state_stale_sec", 2.0);

    state_sub_ = create_subscription<mavros_msgs::msg::State>(
      "/mavros/state", 10,
      std::bind(&ControlNode::state_callback, this, std::placeholders::_1));

    arm_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/sentinel/control/arm", 10,
      std::bind(&ControlNode::arm_callback, this, std::placeholders::_1));

    takeoff_sub_ = create_subscription<std_msgs::msg::Float64>(
      "/sentinel/control/takeoff_altitude", 10,
      std::bind(&ControlNode::takeoff_callback, this, std::placeholders::_1));

    land_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/sentinel/control/land", 10,
      std::bind(&ControlNode::land_callback, this, std::placeholders::_1));

    abort_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/sentinel/mission/abort", 10,
      std::bind(&ControlNode::abort_callback, this, std::placeholders::_1));

    control_status_pub_ = create_publisher<std_msgs::msg::String>(
      "/sentinel/control/status", 10);

    abort_status_pub_ = create_publisher<std_msgs::msg::String>(
      "/sentinel/mission/abort_status", 10);

    arm_client_ = create_client<mavros_msgs::srv::CommandBool>("/mavros/cmd/arming");
    set_mode_client_ = create_client<mavros_msgs::srv::SetMode>("/mavros/set_mode");
    takeoff_client_ = create_client<mavros_msgs::srv::CommandTOL>("/mavros/cmd/takeoff");
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

  void state_callback(const mavros_msgs::msg::State::SharedPtr msg)
  {
    last_state_ = *msg;
    last_state_stamp_ = now();
    has_state_ = true;
  }

  void arm_callback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    if (!msg->data) {
      return;
    }

    if (!begin_control_command("arm")) {
      return;
    }

    if (last_state_.armed) {
      publish_control_status("arm_already_armed");
      finish_control_command(true, "arm_succeeded");
      return;
    }

    if (!arm_client_->wait_for_service(to_duration(service_wait_timeout_sec_))) {
      finish_control_command(false, "arm_service_unavailable");
      return;
    }

    arm_control_timeout_timer("arm", arm_request_timeout_sec_, "arm_request_timeout");

    auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
    req->value = true;

    arm_client_->async_send_request(
      req,
      [this](rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedFuture future) {
        if (!is_active_control_command("arm")) {
          return;
        }

        cancel_timer(control_timeout_timer_);

        try {
          const auto response = future.get();
          if (response->success) {
            finish_control_command(true, "arm_succeeded");
            return;
          }
          finish_control_command(false, "arm_command_rejected");
        } catch (const std::exception & ex) {
          RCLCPP_ERROR(get_logger(), "arm request failed: %s", ex.what());
          finish_control_command(false, "arm_request_exception");
        }
      });
  }

  void takeoff_callback(const std_msgs::msg::Float64::SharedPtr msg)
  {
    const double altitude = msg->data;
    if (altitude <= 0.0) {
      publish_control_status("takeoff_invalid_altitude");
      return;
    }

    if (!begin_control_command("takeoff")) {
      return;
    }

    if (!last_state_.armed) {
      finish_control_command(false, "takeoff_precondition_not_armed");
      return;
    }

    arm_control_timeout_timer("takeoff", takeoff_request_timeout_sec_, "takeoff_request_timeout");

    ensure_guided_mode_then(
      [this, altitude](bool ok) {
        if (!ok) {
          finish_control_command(false, "takeoff_mode_set_failed");
          return;
        }
        send_takeoff_request(altitude);
      });
  }

  void land_callback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    if (!msg->data) {
      return;
    }

    if (!begin_control_command("land")) {
      return;
    }

    if (!land_client_->wait_for_service(to_duration(service_wait_timeout_sec_))) {
      finish_control_command(false, "land_service_unavailable");
      return;
    }

    arm_control_timeout_timer("land", land_request_timeout_sec_, "land_request_timeout");
    auto req = std::make_shared<mavros_msgs::srv::CommandTOL::Request>();

    land_client_->async_send_request(
      req,
      [this](rclcpp::Client<mavros_msgs::srv::CommandTOL>::SharedFuture future) {
        if (!is_active_control_command("land")) {
          return;
        }

        cancel_timer(control_timeout_timer_);

        try {
          const auto response = future.get();
          if (response->success) {
            finish_control_command(true, "land_succeeded");
            return;
          }
          finish_control_command(false, "land_command_rejected");
        } catch (const std::exception & ex) {
          RCLCPP_ERROR(get_logger(), "land request failed: %s", ex.what());
          finish_control_command(false, "land_request_exception");
        }
      });
  }

  void abort_callback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    if (!msg->data) {
      return;
    }

    preempt_control_command();

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

  bool begin_control_command(const std::string & command_name)
  {
    if (abort_state_ != AbortState::Idle) {
      publish_control_status(command_name + "_rejected_abort_in_progress");
      return false;
    }

    if (control_command_in_progress_) {
      publish_control_status(command_name + "_rejected_busy");
      return false;
    }

    if (!fcu_state_ready(command_name)) {
      return false;
    }

    control_command_in_progress_ = true;
    control_command_name_ = command_name;
    publish_control_status(command_name + "_requested");
    return true;
  }

  void finish_control_command(bool success, const std::string & status)
  {
    publish_control_status(status);
    control_command_in_progress_ = false;
    control_command_name_.clear();
    cancel_timer(control_timeout_timer_);

    if (!success) {
      RCLCPP_WARN(get_logger(), "control command failed: %s", status.c_str());
    }
  }

  void preempt_control_command()
  {
    if (!control_command_in_progress_) {
      return;
    }

    publish_control_status("control_command_preempted_by_abort");
    control_command_in_progress_ = false;
    control_command_name_.clear();
    cancel_timer(control_timeout_timer_);
  }

  bool is_active_control_command(const std::string & name) const
  {
    return control_command_in_progress_ && control_command_name_ == name;
  }

  bool fcu_state_ready(const std::string & command_name)
  {
    if (!has_state_) {
      publish_control_status(command_name + "_precondition_no_state");
      return false;
    }

    if ((now() - last_state_stamp_) > rclcpp::Duration::from_seconds(fcu_state_stale_sec_)) {
      publish_control_status(command_name + "_precondition_state_stale");
      return false;
    }

    if (!last_state_.connected) {
      publish_control_status(command_name + "_precondition_fcu_disconnected");
      return false;
    }

    return true;
  }

  void arm_control_timeout_timer(
    const std::string & command_name,
    double timeout_sec,
    const std::string & timeout_status)
  {
    cancel_timer(control_timeout_timer_);
    control_timeout_timer_ = create_wall_timer(
      to_duration(timeout_sec),
      [this, command_name, timeout_status]() {
        cancel_timer(control_timeout_timer_);
        if (!is_active_control_command(command_name)) {
          return;
        }
        finish_control_command(false, timeout_status);
      });
  }

  void ensure_guided_mode_then(const std::function<void(bool)> & on_done)
  {
    if (last_state_.mode == guided_mode_) {
      on_done(true);
      return;
    }

    if (!set_mode_client_->wait_for_service(to_duration(service_wait_timeout_sec_))) {
      publish_control_status("takeoff_set_mode_service_unavailable");
      on_done(false);
      return;
    }

    auto req = std::make_shared<mavros_msgs::srv::SetMode::Request>();
    req->custom_mode = guided_mode_;

    set_mode_client_->async_send_request(
      req,
      [this, on_done](rclcpp::Client<mavros_msgs::srv::SetMode>::SharedFuture future) {
        if (!is_active_control_command("takeoff")) {
          return;
        }

        try {
          const auto response = future.get();
          if (response->mode_sent) {
            publish_control_status("takeoff_guided_mode_set");
            on_done(true);
            return;
          }
          publish_control_status("takeoff_guided_mode_rejected");
        } catch (const std::exception & ex) {
          RCLCPP_ERROR(get_logger(), "takeoff set_mode failed: %s", ex.what());
          publish_control_status("takeoff_guided_mode_exception");
        }

        on_done(false);
      });
  }

  void send_takeoff_request(double altitude)
  {
    if (!is_active_control_command("takeoff")) {
      return;
    }

    if (!takeoff_client_->wait_for_service(to_duration(service_wait_timeout_sec_))) {
      finish_control_command(false, "takeoff_service_unavailable");
      return;
    }

    auto req = std::make_shared<mavros_msgs::srv::CommandTOL::Request>();
    req->altitude = altitude;

    takeoff_client_->async_send_request(
      req,
      [this](rclcpp::Client<mavros_msgs::srv::CommandTOL>::SharedFuture future) {
        if (!is_active_control_command("takeoff")) {
          return;
        }

        cancel_timer(control_timeout_timer_);

        try {
          const auto response = future.get();
          if (response->success) {
            finish_control_command(true, "takeoff_succeeded");
            return;
          }
          finish_control_command(false, "takeoff_command_rejected");
        } catch (const std::exception & ex) {
          RCLCPP_ERROR(get_logger(), "takeoff request failed: %s", ex.what());
          finish_control_command(false, "takeoff_request_exception");
        }
      });
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

  void publish_control_status(const std::string & status)
  {
    std_msgs::msg::String msg;
    msg.data = status;
    control_status_pub_->publish(msg);
  }

  std::string hold_mode_;
  std::string guided_mode_;

  double arm_request_timeout_sec_{8.0};
  double takeoff_request_timeout_sec_{30.0};
  double hold_request_timeout_sec_{5.0};
  double land_request_timeout_sec_{8.0};
  double service_wait_timeout_sec_{0.5};
  double fcu_state_stale_sec_{2.0};

  bool has_state_{false};
  mavros_msgs::msg::State last_state_;
  rclcpp::Time last_state_stamp_{0, 0, RCL_ROS_TIME};

  bool control_command_in_progress_{false};
  std::string control_command_name_;

  AbortState abort_state_{AbortState::Idle};

  rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr arm_sub_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr takeoff_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr land_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr abort_sub_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr control_status_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr abort_status_pub_;

  rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arm_client_;
  rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
  rclcpp::Client<mavros_msgs::srv::CommandTOL>::SharedPtr takeoff_client_;
  rclcpp::Client<mavros_msgs::srv::CommandTOL>::SharedPtr land_client_;

  rclcpp::TimerBase::SharedPtr control_timeout_timer_;
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
