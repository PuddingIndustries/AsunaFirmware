#include <stddef.h>

/* App */
#include <string.h>

#include "frontend_manager.h"

#define FEMGR_CSS_FILENAME "730q9ghE"
#define FEMGR_JS_FILENAME  "vcbbHh1K"

extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_html_gz_end[] asm("_binary_index_html_gz_end");
extern const uint8_t index_css_gz_start[] asm("_binary_index_" FEMGR_CSS_FILENAME "_css_gz_start");
extern const uint8_t index_css_gz_end[] asm("_binary_index_" FEMGR_CSS_FILENAME "_css_gz_end");
extern const uint8_t index_js_start[] asm("_binary_index_" FEMGR_JS_FILENAME "_js_gz_start");
extern const uint8_t index_js_end[] asm("_binary_index_" FEMGR_JS_FILENAME "_js_gz_end");

typedef struct {
    const char    *path;
    const char    *mime_type;
    const uint8_t *data_start;
    const uint8_t *data_end;
} femgr_file_t;

static const femgr_file_t s_femgr_files[] = {
    {
        .path       = "/index.html",
        .mime_type  = "text/html",
        .data_start = index_html_gz_start,
        .data_end   = index_html_gz_end,
    },
    {
        .path       = "/assets/index-" FEMGR_CSS_FILENAME ".css",
        .mime_type  = "text/css",
        .data_start = index_css_gz_start,
        .data_end   = index_css_gz_end,
    },
    {
        .path       = "/assets/index-" FEMGR_JS_FILENAME ".js",
        .mime_type  = "text/javascript",
        .data_start = index_js_start,
        .data_end   = index_js_end,
    },
    {
        .path       = "/",
        .mime_type  = "text/html",
        .data_start = index_html_gz_start,
        .data_end   = index_html_gz_end,
    }, /* Must be the last one to avoid short matching. */
};

int frontend_mgr_get_file_count(void) {
    return sizeof(s_femgr_files) / sizeof(s_femgr_files[0]);
}

const char *frontend_mgr_get_file_path(size_t id) {
    return s_femgr_files[id].path;
}

const char *frontend_mgr_get_file_mime_type(size_t id) {
    return s_femgr_files[id].mime_type;
}

size_t frontend_mgr_get_file_size(size_t id) {
    return s_femgr_files[id].data_end - s_femgr_files[id].data_start;
}

const uint8_t *frontend_mgr_get_file_data(size_t id) {
    return s_femgr_files[id].data_start;
}