#include "planning/path_optimizer.h"

#define pi 3.14159265354
path_optimizer::path_optimizer(YAML::Node optimizer_conf){
    conf.step_t     = optimizer_conf["step_t"].as<float>();
    conf.planning_t = optimizer_conf["planning_t"].as<float>();
    conf.aim_speed  = optimizer_conf["aim_speed"].as<float>();
    interpolating = new Interpolating(optimizer_conf["Interpolating_conf"]);
}


Spline_Out* path_optimizer::get_refrenceline(const car_msgs::trajectory& trajectory_in, car_msgs::trajectory& trajectory_out){
    return interpolating->process(trajectory_in, trajectory_out);
}

 void path_optimizer::get_sampling_line(const Car_State_SL& status_sl, const Spline_Out* refrenceline_Sp, car_msgs::trajectory& trajectory_out){
    //速度规划
    VectorXf QP4(5);
    QP4 = Fitting::quartic4_polynomial(status_sl.s, status_sl.sv, status_sl.sa, conf.aim_speed, 0 , conf.planning_t);
    int len = conf.planning_t/conf.step_t+1;
	VectorXf T = VectorXf::LinSpaced(len, 0, conf.planning_t);
    MatrixXf S_out;
    Fitting::cal_point_quartic4(QP4,T,S_out);
    //转换
    if(len<5)
        cout << "sample error!"<<endl;
    trajectory_out.trajectory_path.resize(len);
    for(int i=0;i<len;i++){
        trajectory_out.trajectory_path[i].s = S_out(i,0);
        trajectory_out.trajectory_path[i].speed = S_out(i,1);
        trajectory_out.trajectory_path[i].accel = S_out(i,2);
        //debug
        // cout<<"i:"<<i+1
        // <<"   s:"<<trajectory_out.trajectory_path[i].s
        // <<"   speed:"<<trajectory_out.trajectory_path[i].speed
        // <<"   acc"<<trajectory_out.trajectory_path[i].accel<<endl;
    }
    //cout<<"status_sl.s:"<<status_sl.s<<"  s0:"<<trajectory_out.trajectory_path[0].s<<endl;
    //横向位置规划
    VectorXf QP5(6);
    QP5 = Fitting::quintic5_polynomial(status_sl.h,status_sl.dh,status_sl.ddh,0,0,0,trajectory_out.trajectory_path.back().s-status_sl.s);
    VectorXf S;
    S.resizeLike(S_out.col(0));
    for(int i=0;i<S.rows();i++){
        S(i) = S_out(i,0) - status_sl.s;
    }
    MatrixXf L_out;
    Fitting::cal_point_quintic5(QP5 ,S,L_out);

    for(int i=0;i<len;i++){
        if(i==0)
            Coordinate_converter::SL_to_POS(trajectory_out.trajectory_path[i].s, L_out(i,0),
                refrenceline_Sp->sx,refrenceline_Sp->sy,trajectory_out.trajectory_path[i],0);
        else
            Coordinate_converter::SL_to_POS(trajectory_out.trajectory_path[i].s, L_out(i,0),
                refrenceline_Sp->sx,refrenceline_Sp->sy,trajectory_out.trajectory_path[i]);
            
        trajectory_out.trajectory_path[i].relative_time = T(i);
        trajectory_out.trajectory_path[i].header.seq = i+1;
        //debug
        if(i>0&&  
        (abs(trajectory_out.trajectory_path[i-1].x-trajectory_out.trajectory_path[i].x)>10
        ||abs(trajectory_out.trajectory_path[i-1].y-trajectory_out.trajectory_path[i].y)>10)){
            ROS_INFO("pos error!"); 
        }
        // cout
        // <<trajectory_out.trajectory_path[i].header.seq<<","
        // <<trajectory_out.trajectory_path[i].x<<","
        // <<trajectory_out.trajectory_path[i].y<<","
        // <<pos<<","
        // <<trajectory_out.trajectory_path[i].s<<","
        // <<L_out(i,0)<<","
        // <<refrenceline_x<<","
        // <<refrenceline_y<<","
        // <<refrenceline_theta<<endl;
    }
   
    //计算theta
    float dx,dy;
    for(int i=0;i<len-1;i++){
        dx = trajectory_out.trajectory_path[i+1].x - trajectory_out.trajectory_path[i].x;
        dy = trajectory_out.trajectory_path[i+1].y - trajectory_out.trajectory_path[i].y;
        trajectory_out.trajectory_path[i].theta = Interpolating::yaw(dx,dy);
        //debug
        //ROS_INFO("dx:%f dy:%f theta:%f", dx,dy,trajectory_out.trajectory_path[i].theta);
        if(i>1&&i<len-2&&abs(trajectory_out.trajectory_path[i-1].theta-trajectory_out.trajectory_path[i].theta)>0.5&&
        abs(trajectory_out.trajectory_path[i-1].theta-trajectory_out.trajectory_path[i].theta)<pi){
            dx = trajectory_out.trajectory_path[i+2].x - trajectory_out.trajectory_path[i-1].x;
            dy = trajectory_out.trajectory_path[i+2].y - trajectory_out.trajectory_path[i-1].y;
            trajectory_out.trajectory_path[i].theta = Interpolating::yaw(dx,dy);
        }
    }
    //优化theta
    trajectory_out.trajectory_path[len-1].theta = 2*trajectory_out.trajectory_path[len-2].theta - trajectory_out.trajectory_path[len-3].theta;
    // //使用多项式拟合
    // VectorXf thetaVec(len);
    // for(int i=0;i<len;i++) 
    //     thetaVec(i) = trajectory_out.trajectory_path[i].theta;
    // Fitting::gradient_descent_optimizer(thetaVec);
    // //debug
    // for(int i=0;i<len;i++)
    //     ROS_INFO("%d, theta0:%8.6f theta1:%8.6f",i+1, trajectory_out.trajectory_path[i].theta, thetaVec(i)); 
    // //
    // for(int i=0;i<len;i++) 
    //     trajectory_out.trajectory_path[i].theta = thetaVec(i);

    
    //计算曲率
    for(int i=0;i<len-1;i++){
        dx = trajectory_out.trajectory_path[i+1].x - trajectory_out.trajectory_path[i].x;
        dy = trajectory_out.trajectory_path[i+1].y - trajectory_out.trajectory_path[i].y;
        float dtheta = trajectory_out.trajectory_path[i+1].theta - trajectory_out.trajectory_path[i].theta;
        if(dtheta>pi) dtheta = dtheta - 2*pi;
        if(dtheta<-pi) dtheta = dtheta + 2*pi;
        if(dx==0&&dy==0)
            trajectory_out.trajectory_path[i].kappa = 0;
        else
            trajectory_out.trajectory_path[i].kappa = dtheta/sqrt(dx*dx+dy*dy);
        
    }
    //去掉最后一个不可用点
    trajectory_out.trajectory_path.pop_back();

    trajectory_out.total_path_length = trajectory_out.trajectory_path.size();
    trajectory_out.total_path_time = conf.planning_t;
    trajectory_out.absolute_time = ros::Time::now().toSec();
    



    //debug
    // for(int i=0;i<len;i++)
    // cout<<
    // trajectory_out.trajectory_path[i].header.seq<<","<<
    // trajectory_out.trajectory_path[i].x<<","<<
    // trajectory_out.trajectory_path[i].y<<","<<
    // trajectory_out.trajectory_path[i].theta<<","<<
    // trajectory_out.trajectory_path[i].kappa<<endl;
    //debug 2
    for(int i=0;i<trajectory_out.total_path_length;i++)
    cout<<
    trajectory_out.trajectory_path[i].header.seq<<"."<<
    " x:"<<trajectory_out.trajectory_path[i].x<<
    " y:"<<trajectory_out.trajectory_path[i].y<<","<<
    " theta:"<<trajectory_out.trajectory_path[i].theta<<","<<
    " kappa:"<<trajectory_out.trajectory_path[i].kappa<<endl;
 }


void path_optimizer::process(const Car_State_SL& status_sl, const Spline_Out* refrenceline_Sp, car_msgs::trajectory& trajectory_out){
    static int count = 0;
    count++;
    get_sampling_line(status_sl, refrenceline_Sp, trajectory_out);
    //添加header
    trajectory_out.header.seq = count;
}