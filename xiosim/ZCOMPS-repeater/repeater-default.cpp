/* repeater-default.cpp - Wrapper for default ring cache implementation (librepeater) */
/*
 * Svilen Kanev, 2013
 */

#include "mem-repeater.h"

#define COMPONENT_NAME "default"

#ifdef REPEATER_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  repeater_t * result = librepeater_create(core, name, next_level);
  result->speed = core->knobs->default_cpu_speed; // Assume repeater runs at core frequency
  return result;
}

#elif defined(REPEATER_INIT)
if(!strcasecmp(COMPONENT_NAME,type))
{
  librepeater_init(opt_string);
  return;
}

#elif defined(REPEATER_SHUTDOWN)
if(!strcasecmp(COMPONENT_NAME,type))
{
  librepeater_shutdown();
  return;
}

#else
/* repeater_default_t defined in librepeater */

#endif

#undef COMPONENT_NAME
