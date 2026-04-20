idf_component_register(SRCS "status_updater.c"
                       PRIV_REQUIRES esp_rainmaker rmaker_app_network
                       INCLUDE_DIRS "include")