#include <assert.h>
#include "Sensor.h"

Sensor::Sensor() {

  sensorManager = ASensorManager_getInstance();
//  sensorManager = ASensorManager_getInstanceForPackage(kPackageName);
  assert(sensorManager != NULL);

  accelerometer = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ACCELEROMETER);
  assert(accelerometer != NULL);

  looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
  assert(looper != NULL);

  accelerometerEventQueue = ASensorManager_createEventQueue(sensorManager, looper,
                                                            LOOPER_ID_USER, NULL, NULL);
  assert(accelerometerEventQueue != NULL);

  auto status = ASensorEventQueue_enableSensor(accelerometerEventQueue, accelerometer);
  assert(status >= 0);

  status = ASensorEventQueue_setEventRate(accelerometerEventQueue,
                                          accelerometer,
                                          SENSOR_REFRESH_PERIOD_US);
  assert(status >= 0);

  (void)status;   //to silent unused compiler warning
}

void Sensor::Update(float &x, float &y, float &z, float factor) {
  ALooper_pollAll(0, NULL, NULL, NULL);
  ASensorEvent event;
  float xx,yy,zz;

  while (ASensorEventQueue_getEvents(accelerometerEventQueue, &event, 1) > 0) {
    xx = event.acceleration.x;
    yy = event.acceleration.y;
    zz = event.acceleration.z;
  }

  x += xx / factor;
  y += yy / factor;
  z += zz / factor;
}