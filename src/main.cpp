#include <Arduino.h>
#include "app/SiteController.h"

SiteController app;

void setup() {
  app.begin();
}

void loop() {
  app.update();
}
