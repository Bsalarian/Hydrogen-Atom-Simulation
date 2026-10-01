#include "app/app.h"
#include "app/callbacks.h"
#include "gfx/engine.h"
#include "gfx/camera.h"
#include "sim/particle_system.h"
#include <glm/gtc/type_ptr.hpp>
#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
#endif