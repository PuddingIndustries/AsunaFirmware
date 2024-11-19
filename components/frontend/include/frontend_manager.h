#ifndef FRONTEND_MANAGER_H
#define FRONTEND_MANAGER_H

#include <stdint.h>

int            frontend_mgr_get_file_count(void);
const char*    frontend_mgr_get_file_path(size_t id);
const char*    frontend_mgr_get_file_mime_type(size_t id);
size_t         frontend_mgr_get_file_size(size_t id);
const uint8_t* frontend_mgr_get_file_data(size_t id);
#endif  // FRONTEND_MANAGER_H
