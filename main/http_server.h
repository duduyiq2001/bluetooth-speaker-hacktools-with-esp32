#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_http_server.h"
#include "esp_err.h"

/**
 * @brief Start WiFi in AP mode to allow file download
 * 
 * @return esp_err_t ESP_OK on success, or error code
 */
esp_err_t start_wifi_ap(void);

/**
 * @brief Start web server for file downloading
 * 
 * @return httpd_handle_t Server handle, or NULL on failure
 */
httpd_handle_t start_web_server(void);

/**
 * @brief Initialize both WiFi AP and HTTP server
 * 
 * @return esp_err_t ESP_OK on success, or error code
 */
esp_err_t init_file_server(void);

#endif /* HTTP_SERVER_H */ 