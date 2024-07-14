#ifndef APP_API_GNSS_HANDLER_STREAM_H
#define APP_API_GNSS_HANDLER_STREAM_H

extern const httpd_uri_t app_api_gnss_handler_stream_ws_uri;
int                      app_api_gnss_handler_stream_ws_init(void);
int                      app_api_gnss_handler_stream_ws_onopen(httpd_handle_t handle, int fd);
int                      app_api_gnss_handler_stream_ws_onclose(httpd_handle_t handle, int fd);

#endif  // APP_API_GNSS_HANDLER_STREAM_H
