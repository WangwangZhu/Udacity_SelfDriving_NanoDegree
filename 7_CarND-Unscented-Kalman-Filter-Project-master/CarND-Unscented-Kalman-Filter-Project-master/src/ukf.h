#ifndef UKF_H
#define UKF_H

#include "Eigen/Dense"
#include "measurement_package.h"
#include <iostream>
using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
class UKF
{
public:
  /**
   * Constructor
   */
  UKF();

  /**
   * Destructor
   */
  virtual ~UKF();

  /**
   * ProcessMeasurement
   * @param meas_package The latest measurement data of either radar or laser
   */
  void ProcessMeasurement(MeasurementPackage meas_package);

  /**
   * Prediction Predicts sigma points, the state, and the state covariance
   * matrix
   * @param delta_t Time between k and k+1 in s
   */
  void Prediction(double const delta_t);

  /**
   * Updates the state and the state covariance matrix using a laser measurement
   * @param meas_package The measurement at k+1
   */
  void UpdateLidar(MeasurementPackage meas_package);
  void NormalizeAngle(double& phi);
  /**
   * Updates the state and the state covariance matrix using a radar measurement
   * @param meas_package The measurement at k+1
   */
  void UpdateRadar(MeasurementPackage meas_package);

  MatrixXd AugmentedSigmaPoints();

  MatrixXd SigmaPointPrediction(double const delta_t);

  // initially set to false, set to true in first call of ProcessMeasurement
  bool is_initialized_;

  // if this is false, laser measurements will be ignored (except for init)
  bool use_laser_;

  // if this is false, radar measurements will be ignored (except for init)
  bool use_radar_;

  // state vector: [pos1 pos2 vel_abs yaw_angle yaw_rate] in SI units and rad
  Eigen::VectorXd x_;

  // augmented state vector: [pos1 pos2 vel_abs yaw_angle yaw_rate vel_abs_d yaw_rate_d] in SI units and rad
  Eigen::VectorXd x_aug_;

  // state covariance matrix
  Eigen::MatrixXd P_;

  // predicted sigma points matrix
  Eigen::MatrixXd Xsig_pred_;

  // time when the state is true, in us
  long long time_us_;

  // Process noise standard deviation longitudinal acceleration in m/s^2
  double std_a_;

  // Process noise standard deviation yaw acceleration in rad/s^2
  double std_yawdd_;

  // Laser measurement noise standard deviation position1 in m
  double std_laspx_;

  // Laser measurement noise standard deviation position2 in m
  double std_laspy_;

  // Radar measurement noise standard deviation radius in m
  double std_radr_;

  // Radar measurement noise standard deviation angle in rad
  double std_radphi_;

  // Radar measurement noise standard deviation radius change in m/s
  double std_radrd_;

  // Weights of sigma points
  Eigen::VectorXd weights_;

  // State dimension
  int n_x_;

  // Augmented state dimension
  int n_aug_;

  // Sigma point spreading parameter
  double lambda_;

  int n_sigma_points_;

    // Sensor's measurement size
  int n_z_radar_; // radar -> rho, phi, rho_dot
  int n_z_lidar_; // lidar -> px, py

  // Measurement covariance matrices
  MatrixXd R_lidar_;
  MatrixXd R_radar_;
};

#endif // UKF_H