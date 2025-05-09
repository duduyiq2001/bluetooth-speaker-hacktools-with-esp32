#include "http_server.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include "lwip/ip4_addr.h"

static const char *TAG = "HTTP_SERVER";

/* File server handler: lists all files in SPIFFS */
static esp_err_t file_list_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Listing files");
    
    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SPIFFS mount failed");
        return ESP_FAIL;
    }
    
    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        esp_vfs_spiffs_unregister(NULL);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open directory");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head><title>ESP32 Files</title>");
    httpd_resp_sendstr_chunk(req, "<style>body{font-family:Arial,sans-serif;margin:20px;line-height:1.6}h1{color:#0066cc}ul{list-style-type:none;padding:0}li{margin:10px 0;padding:10px;background-color:#f0f0f0;border-radius:5px}a{color:#0066cc;text-decoration:none}a:hover{text-decoration:underline}</style>");
    httpd_resp_sendstr_chunk(req, "</head><body>");
    httpd_resp_sendstr_chunk(req, "<h1>ESP32 Captured BT Packet Files</h1>");
    httpd_resp_sendstr_chunk(req, "<p>Click on a file to download:</p><ul>");
    
    struct dirent *entry;
    char file_url[1024]; // Large enough for any reasonable HTML output
    char filepath[512];  // Large enough for /spiffs/ + max filename (255)
    char size_str[32];
    
    while ((entry = readdir(dir)) != NULL) {
        // Safety check for filename length to prevent buffer overflow
        if (strlen(entry->d_name) > 200) {
            ESP_LOGW(TAG, "Skipping file with very long name");
            continue;
        }
        
        // Get file size
        snprintf(filepath, sizeof(filepath), "/spiffs/%s", entry->d_name);
        struct stat st;
        if (stat(filepath, &st) != 0) {
            ESP_LOGW(TAG, "Could not stat file: %s", entry->d_name);
            continue;
        }
        
        // Format size display
        if (st.st_size < 1024) {
            snprintf(size_str, sizeof(size_str), "%ld bytes", st.st_size);
        } else if (st.st_size < 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%.1f KB", st.st_size / 1024.0);
        } else {
            snprintf(size_str, sizeof(size_str), "%.1f MB", st.st_size / (1024.0 * 1024.0));
        }
        
        // Build HTML entry
        snprintf(file_url, sizeof(file_url), 
                "<li><a href=\"/download?file=%s\">%s</a> (%s)</li>", 
                entry->d_name, entry->d_name, size_str);
                
        httpd_resp_sendstr_chunk(req, file_url);
    }
    
    httpd_resp_sendstr_chunk(req, "</ul>");
    httpd_resp_sendstr_chunk(req, "<p><small>ESP32 Bluetooth Packet Capture Server</small></p>");
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    
    closedir(dir);
    esp_vfs_spiffs_unregister(NULL);
    
    return ESP_OK;
}

/* File download handler: streams a file from SPIFFS */
static esp_err_t file_download_handler(httpd_req_t *req) {
    char filepath[512]; // Large enough for any path
    char filename[256] = {0}; // Large enough for max filename
    
    // Get filename from query
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK &&
            httpd_query_key_value(buf, "file", filename, sizeof(filename)) == ESP_OK) {
            // Filename found, continue
            
            // Additional security check for filename length
            if (strlen(filename) > 200) {
                free(buf);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Filename too long");
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad filename");
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Filename required");
        return ESP_FAIL;
    }
    
    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SPIFFS mount failed");
        return ESP_FAIL;
    }
    
    // Path safety check to prevent directory traversal
    if (strchr(filename, '/') != NULL) {
        esp_vfs_spiffs_unregister(NULL);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }
    
    snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);
    
    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        esp_vfs_spiffs_unregister(NULL);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    
    // Set appropriate content type based on file extension
    httpd_resp_set_type(req, "application/octet-stream");
    
    // Add Content-Disposition header for download
    char content_disp[512]; // Large enough for any filename
    snprintf(content_disp, sizeof(content_disp), "attachment; filename=\"%s\"", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disp);
    
    // Read file in chunks and send
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, bytes_read);
    }
    
    // Send empty chunk to signal end
    httpd_resp_send_chunk(req, NULL, 0);
    
    fclose(file);
    esp_vfs_spiffs_unregister(NULL);
    
    ESP_LOGI(TAG, "File %s downloaded successfully", filename);
    return ESP_OK;
}

/* Start web server for file downloading */
httpd_handle_t start_web_server(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;
    config.stack_size = 8192;  // Increased stack for file operations
    
    ESP_LOGI(TAG, "Starting web server on port: %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Define URI handlers
        httpd_uri_t files_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = file_list_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &files_uri);
        
        httpd_uri_t download_uri = {
            .uri = "/download",
            .method = HTTP_GET,
            .handler = file_download_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &download_uri);
        
        ESP_LOGI(TAG, "Web server started successfully");
        return server;
    }
    
    ESP_LOGE(TAG, "Error starting web server!");
    return NULL;
}

/* Start WiFi in AP mode to allow file download */
esp_err_t start_wifi_ap(void) {
    ESP_LOGI(TAG, "Starting WiFi Access Point for file download");
    
    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default AP netif
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default AP netif");
        return ESP_FAIL;
    }
    
    // Explicitly set the IP address to 192.168.4.1
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif); // Stop DHCP server
    esp_netif_set_ip_info(ap_netif, &ip_info); // Set IP address
    esp_netif_dhcps_start(ap_netif); // Start DHCP server with new IP
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Configure as access point
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32_BT_PACKETS",
            .ssid_len = strlen("ESP32_BT_PACKETS"),
            .password = "password123",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Verify IP address was set correctly
    esp_netif_ip_info_t actual_ip;
    esp_netif_get_ip_info(ap_netif, &actual_ip);
    ESP_LOGI(TAG, "AP IP address: " IPSTR, IP2STR(&actual_ip.ip));
    ESP_LOGI(TAG, "AP Gateway: " IPSTR, IP2STR(&actual_ip.gw));
    ESP_LOGI(TAG, "AP Netmask: " IPSTR, IP2STR(&actual_ip.netmask));
    
    ESP_LOGI(TAG, "WiFi AP started with SSID: %s, password: %s", 
            wifi_config.ap.ssid, wifi_config.ap.password);
    ESP_LOGI(TAG, "You can now connect to this AP and visit http://" IPSTR " to download files", 
             IP2STR(&actual_ip.ip));
    
    return ESP_OK;
}

/* Initialize both WiFi AP and HTTP server */
esp_err_t init_file_server(void) {
    ESP_LOGI(TAG, "Initializing file server...");
    
    // Check if SPIFFS partition exists first
    size_t total = 0, used = 0;
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format SPIFFS: %s", esp_err_to_name(ret));
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "SPIFFS partition not found! Check your partition table.");
        }
        return ret;
    }
    
    // Get SPIFFS partition info to verify it's working
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition info: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS partition: total size %d bytes, used %d bytes", total, used);
    }
    
    // Now we know SPIFFS is working, can unmount it
    esp_vfs_spiffs_unregister(NULL);
    
    // Start WiFi AP
    ret = start_wifi_ap();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi AP");
        return ret;
    }
    
    // Start HTTP server
    httpd_handle_t server = start_web_server();
    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "File server initialized successfully");
    return ESP_OK;
} 