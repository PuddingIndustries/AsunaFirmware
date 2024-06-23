#ifndef __NL_DEBUG_H__
#define __NL_DEBUG_H__

#include "nl_cfg.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifndef NL_LOG_BUF_SIZE
#define NL_LOG_BUF_SIZE           (256)
#endif

#ifndef NL_DEBUG_RTCM
#define NL_DEBUG_RTCM                   0
#endif

#ifndef NL_DEBUG_GNSS
#define NL_DEBUG_GNSS                   0
#endif

#ifndef NL_DEBUG_RCV
#define NL_DEBUG_RCV                   0
#endif

#ifndef NL_DEBUG_IMU
#define NL_DEBUG_IMU                   0
#endif

#define NL_DEBUG_LOG(type, message)                                           \
do                                                                            \
{                                                                             \
    if (type)                                                                 \
        NL_TRACE message;                                                     \
}                                                                             \
while (0)

#define NL_ASSERT(EX)                                                         \
if (!(EX))                                                                    \
{                                                                             \
    nl_assert_handler(#EX, __FUNCTION__, __LINE__);                           \
}


#ifdef __cplusplus
}
#endif

#endif



