#ifndef _LAT_CONTROLLER_H_
#define _LAT_CONTROLLER_H_

#include "controller.h"
#include "control/common/trajectory_analyzer.h"
#include "car_msgs/control_cmd.h"
#include "car_msgs/chassis.h"
#include "car_msgs/localization.h"
#include "car_msgs/trajectory.h"
#include "car_msgs/trajectory_point.h"


#include <fstream>
#include <memory>
#include <string>

#include "Eigen/Core"

// #include "modules/common/configs/proto/vehicle_config.pb.h"

// #include "modules/common/filters/digital_filter.h"
// #include "modules/common/filters/digital_filter_coefficients.h"
// #include "modules/common/filters/mean_filter.h"
// #include "modules/control/common/interpolation_1d.h"
// #include "modules/control/controller/controller.h"
namespace control {

class LatControllerConf{
  public:
    double query_relative_time;
    double minimum_speed_protection;

    double ts;            // sample time (dt) 0.01 now, configurable
    // preview window n, preview time = preview window * ts
    int preview_window = 2;
    double cf;
    double cr;            // N/rad
    double mass_fl;
    double mass_fr;
    double mass_rl;
    double mass_rr;
    double eps;          // converge threshold for lqr solver
    double matrix_q1;     // matrix_q size = 4 + preview_window
    double matrix_q2;     // matrix_q size = 4 + preview_window
    double matrix_q3;     // matrix_q size = 4 + preview_window
    double matrix_q4;     // matrix_q size = 4 + preview_window
    int cutoff_freq;   // cutoff frequency
    int mean_filter_window_size;  // window size of mean filter
    // for a normal car, it should be in range[16, 18]
    int max_iteration;  // maximum iteration for lqr solve
    double max_lateral_acceleration;  // limit aggressive steering

    //VehicleParam-------------------------------------------------------------------
    double front_edge_to_center;
    double back_edge_to_center;
    double left_edge_to_center;
    double right_edge_to_center;

    double length;
    double width;
    double height;

    double min_turn_radius;
    double max_acceleration;
    double max_deceleration;

    // The following items are used to compute trajectory constraints in planning/control/canbus,
    // vehicle max steer angle
    double max_steer_angle;
    // vehicle max steer rate; how fast can the steering wheel turn.
    double max_steer_angle_rate;
    // vehicle min steer rate;
    double min_steer_angle_rate;
    // ratio between the turn of steering wheel and the turn of wheels
    double steer_ratio;
    // the distance between the front and back wheels
    double wheel_base;
    // Tire effective rolling radius (vertical distance between the wheel center
    // and the ground).
    double wheel_rolling_radius;

    // minimum differentiable vehicle speed, in m/s
    float max_abs_speed_when_stopped;
};

class LatController {
  public:

    LatController();

    /**
     * @brief initialize Lateral Controller
     * @param control_conf control configurations
     * @return Status initialization status
     */
    void Init(const LatControllerConf *control_conf);

    /**
     * @brief compute steering target based on current vehicle status
     *        and target trajectory
     * @param localization vehicle location
     * @param chassis vehicle status e.g., speed, acceleration
     * @param trajectory trajectory generated by planning
     * @param cmd control command
     * @return Status computation status
     */
    void ComputeControlCommand(
          const car_msgs::trajectory &planning_published_trajectory,
          const car_msgs::vehicle_state &vehicle_state,
          car_msgs::control_cmd &cmd,
          car_msgs::lat_debug &debug);
      /**
       * @brief reset Lateral Controller
       * @return Status reset status
       */
    void Reset();

    /**
     * @brief stop Lateral controller
     */
    void Stop();

    /**
     * @brief Lateral controller name
     * @return string controller name in string
     */
    std::string Name() const ;

  protected:
    void UpdateState(const car_msgs::vehicle_state &vehicle_state,car_msgs::lat_debug &debug);

    void UpdateMatrix(const car_msgs::vehicle_state &vehicle_state);

    void UpdateMatrixCompound();

    double ComputeFeedForward(double ref_curvature,const car_msgs::vehicle_state &vehicle_state);

    void ComputeLateralErrors(const double x, 
                              const double y, 
                              const double theta,
                              const double linear_v, 
                              const double angular_v,
                              const TrajectoryAnalyzer &trajectory_analyzer,
                              car_msgs::lat_debug &debug);
    bool LoadControlConf(const LatControllerConf *control_conf);
  

    // vehicle parameter
    //common::VehicleParam vehicle_param_;

    // a proxy to analyze the planning trajectory
    TrajectoryAnalyzer trajectory_analyzer_;

    // the following parameters are vehicle physics related.
    // control time interval
    double ts_ = 0.0;
    // corner stiffness; front
    double cf_ = 0.0;
    // corner stiffness; rear
    double cr_ = 0.0;
    // distance between front and rear wheel center
    double wheelbase_ = 0.0;
    // mass of the vehicle
    double mass_ = 0.0;
    // distance from front wheel center to COM
    double lf_ = 0.0;
    // distance from rear wheel center to COM
    double lr_ = 0.0;
    // rotational inertia
    double iz_ = 0.0;
    // the ratio between the turn of the steering wheel and the turn of the wheels
    double steer_ratio_ = 0.0;
    // the maximum turn of steer
    double steer_single_direction_max_degree_ = 0.0;

    // limit steering to maximum theoretical lateral acceleration
    double max_lat_acc_ = 0.0;

    // number of control cycles look ahead (preview controller)
    int preview_window_ = 0;
    // number of states without previews, includes
    // lateral error, lateral error rate, heading error, heading error rate
    const int basic_state_size_ = 4;
    // vehicle state matrix
    Eigen::MatrixXd matrix_a_;
    // vehicle state matrix (discrete-time)
    Eigen::MatrixXd matrix_ad_;
    // vehicle state matrix compound; related to preview
    Eigen::MatrixXd matrix_adc_;
    // control matrix
    Eigen::MatrixXd matrix_b_;
    // control matrix (discrete-time)
    Eigen::MatrixXd matrix_bd_;
    // control matrix compound
    Eigen::MatrixXd matrix_bdc_;
    // gain matrix
    Eigen::MatrixXd matrix_k_;
    // control authority weighting matrix
    Eigen::MatrixXd matrix_r_;
    // state weighting matrix
    Eigen::MatrixXd matrix_q_;
    // updated state weighting matrix
    Eigen::MatrixXd matrix_q_updated_;
    // vehicle state matrix coefficients
    Eigen::MatrixXd matrix_a_coeff_;
    // 4 by 1 matrix; state matrix
    Eigen::MatrixXd matrix_state_;

    // parameters for lqr solver; number of iterations
    int lqr_max_iteration_ = 0;
    // parameters for lqr solver; threshold for computation
    double lqr_eps_ = 0.0;

  //   common::DigitalFilter digital_filter_;

  //   std::unique_ptr<interpolation1D> lat_err_interpolation_;

  //   std::unique_ptr<interpolation1D> heading_err_interpolation_;

    // MeanFilter heading_rate_filter_;
  //   common::MeanFilter lateral_error_filter_;
  //   common::MeanFilter heading_error_filter_;

    // for logging purpose
    std::ofstream steer_log_file_;

    const std::string name_;

    double query_relative_time_;

    double pre_steer_angle_ = 0.0;

    double minimum_speed_protection_ = 0.1;

    double current_trajectory_timestamp_ = -1.0;

    double init_vehicle_x_ = 0.0;

    double init_vehicle_y_ = 0.0;

    double init_vehicle_heading_ = 0.0;

    double min_turn_radius_ = 0.0;
};
} //namespace control
#endif  // _LAT_CONTROLLER_H_
