idf_component_register(SRCS
    "app/api/config/handler_upgrade.c"
    "app/api/config/handler_wifi.c"
    "app/api/gnss/handler_stream.c"
    "app/api/handler_index.c"
    "app/api/handler_static.c"
    "app/api_server.c"
    "app/console/cmd_free.c"
    "app/console/cmd_gnss.c"
    "app/console/cmd_ip.c"
    "app/console/cmd_ps.c"
    "app/console/cmd_version.c"
    "app/console/cmd_wifi.c"
    "app/console_common.c"
    "app/gnss_server.c"
    "app/netif_common.c"
    "app/netif_lte.c"
    "app/netif_wifi.c"
    "app/version_manager.c"
    "app/vfs_common.c"
    "main.c"

INCLUDE_DIRS
    "include"
)
