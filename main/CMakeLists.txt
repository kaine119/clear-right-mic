idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash nvs_flash status_updater inference_api mic
                       INCLUDE_DIRS "")
