#ifndef __NL_CFG_H__
#define __NL_CFG_H__



typedef float nl_t;


#define NL_DEBUG_RTCM       0
#define NL_DEBUG_GNSS       0
#define NL_DEBUG_RCV        0
#define NL_DEBUG_IMU        0


#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

#define nl_malloc   malloc
#define nl_free     free
#define nl_memset   memset
#define NL_TRACE    printf

#endif
