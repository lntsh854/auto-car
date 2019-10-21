

#include "common/base/file_tool/file_tool.h"
#include "common/base/global_gflags/global_gflags.h"
#include "common/base/log.h"
#include "planning/common.h"
#include "planning/planning.h"
#include "planning/src/common/planning_gflags.h"

bool DEBUG = 0;
using common::base::FileTool;
int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  AINFO << "planning start...";

  /* code for main function */
  ros::init(argc, argv, "planning");
  ros::NodeHandle planning_NodeHandle;

  /*planning模块初始化*/
  Car_Planning planning(FileTool::LoadFile(FLAGS_planning_conf_path));
  debugger = new Debugger();

  /*订阅*/
  planning.localization_subscriber = planning_NodeHandle.subscribe(
      FLAGS_chassis_localization_topic, 1, &Car_Planning::localization_callback,
      &planning);
  planning.chassis_subscriber =
      planning_NodeHandle.subscribe(FLAGS_chassis_car_state_topic, 1,
                                    &Car_Planning::chassis_callback, &planning);
  planning.obstacle_subscriber = planning_NodeHandle.subscribe(
      "obstacle_topic", 1, &Car_Planning::obstacle_callback, &planning);
  /*发布*/
  planning.refrenceline_publisher =
      planning_NodeHandle.advertise<car_msgs::trajectory>(
          FLAGS_planning_refrenceline_topic, 1000);
  planning.trajectory_publisher =
      planning_NodeHandle.advertise<car_msgs::trajectory>(
          FLAGS_planning_trajectory_topic, 1000);

  // debugger
  debugger->set_NodeHandle(&planning_NodeHandle);

  // planning_init
  planning.Init();
  // 创建ros定时器
  ros::Timer cycle_timer;
  if (planning.conf.mode == "send") {
    ROS_INFO("planning send mode!");
    cycle_timer = planning_NodeHandle.createTimer(
        ros::Duration(planning.conf.period), &Car_Planning::OnTimer, &planning);
  } else if (planning.conf.mode == "write") {
    ROS_INFO("planning write mode!");
  } else {
    ROS_ERROR("unknow planning mode!");
    ros::shutdown();
  }

  ros::spin();

  return 0;
}
