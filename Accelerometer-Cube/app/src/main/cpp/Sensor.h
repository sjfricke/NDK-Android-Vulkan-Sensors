#ifndef __SENSOR_HPP__
#define __SENSOR_HPP__

#include <android/sensor.h>

class Sensor {
 private:

  const int LOOPER_ID_USER = 3;
  const int SENSOR_REFRESH_RATE_HZ = 100;
  const int32_t SENSOR_REFRESH_PERIOD_US = int32_t(1000000 / SENSOR_REFRESH_RATE_HZ);

  ASensorManager *sensorManager;
  const ASensor *accelerometer;
  ASensorEventQueue *accelerometerEventQueue;
  ALooper *looper;

 public:
  Sensor();

  void Update(float &x, float &y, float &z, float factor);

};


#endif // __SENSOR_HPP__


