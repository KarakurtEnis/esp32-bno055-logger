#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <math.h> 

#include <time.h>
#include <sys/time.h>
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_sntp.h"

//wifi
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

// BNO055 I2C adresi
#define BNO055_ADDRESS 0x28
#define I2C_MASTER_SCL_IO 18  // I2C SCL pini
#define I2C_MASTER_SDA_IO 17 // I2C SDA pini
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 25000

#define CALIB_STAT      0x35
#define BNO055_OPR_MODE 0x3D
#define BNO055_PWR_MODE 0x3E

#define ACC_DATA_X_LSB 0x08
#define ACC_DATA_X_MSB 0x09
#define ACC_DATA_Y_LSB 0x0A
#define ACC_DATA_Y_MSB 0x0B
#define ACC_DATA_Z_LSB 0x0C
#define ACC_DATA_Z_MSB 0x0D

#define LIA_DATA_X_LSB 0x28
#define LIA_DATA_X_MSB 0x29
#define LIA_DATA_Y_LSB 0x2A
#define LIA_DATA_Y_MSB 0x2B
#define LIA_DATA_Z_LSB 0x2C
#define LIA_DATA_Z_MSB 0x2D

#define GYR_DATA_X_LSB 0x14
#define GYR_DATA_X_MSB 0x15
#define GYR_DATA_Y_LSB 0x16
#define GYR_DATA_Y_MSB 0x17
#define GYR_DATA_Z_LSB 0x18
#define GYR_DATA_Z_MSB 0x19

#define EUL_DATA_X_LSB 0x1A
#define EUL_DATA_X_MSB 0x1B
#define EUL_DATA_Y_LSB 0x1C
#define EUL_DATA_Y_MSB 0x1D
#define EUL_DATA_Z_LSB 0x1E
#define EUL_DATA_Z_MSB 0x1F

static const char *BTAG = "BNO055";
static const char *TAG = "example";

#define EXAMPLE_MAX_CHAR_SIZE 256
#define MOUNT_POINT "/sdcard"
#define MOUNT "/sdcard/data"


#define EXAMPLE_ESP_WIFI_SSID      "Qualsem"
#define EXAMPLE_ESP_WIFI_PASS      "Q01S23E08m"

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static const char *CTAG = "wifi station";

char Current_Date_Time[100];

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(CTAG, "Notification of a time synchronization event");
}
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
       // if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
		//{
            esp_wifi_connect();
           // s_retry_num++;
            ESP_LOGI(CTAG, "retry to connect to the AP");
        //} else {
        //    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGI(CTAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(CTAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        //s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_DISCONNECTED,
                    &event_handler,
                    NULL,
                    NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(CTAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(CTAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(CTAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(CTAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}



void Get_current_date_time(char *date_time){
	char strftime_buf[64];
	time_t now;
	    struct tm timeinfo;
	    time(&now);
	    localtime_r(&now, &timeinfo);

	    	
	    	    setenv("TZ", "UTC-3", 1);
	    	    tzset();
	    	    localtime_r(&now, &timeinfo);

	    	    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	    	    ESP_LOGI(CTAG, "The current date/time in Turkey is: %s", strftime_buf);
                strcpy(date_time,strftime_buf);
}


static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
    sntp_init();
}
static void obtain_time(void)
{
    initialize_sntp();
    
    // Zamanın ayarlanması için bekle
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    // Zamanın başarıyla ayarlanıp ayarlanmadığını kontrol et
    while ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET || timeinfo.tm_year < (2016 - 1900)) && ++retry < retry_count) {
        ESP_LOGI(CTAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);  // Zamanı al
        localtime_r(&now, &timeinfo);  // Zamanı tm_year ile kontrol et
    }

    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGE(CTAG, "Failed to synchronize time after %d attempts.", retry);
    } else {
        ESP_LOGI(CTAG, "System time synchronized: %s", asctime(&timeinfo));
    }
}

 void Set_SystemTime_SNTP()  {

	 time_t now;
	    struct tm timeinfo;
	    time(&now);
	    localtime_r(&now, &timeinfo);
	    // Is time set? If not, tm_year will be (1970 - 1900).
	    if (timeinfo.tm_year < (2016 - 1900)) {
	        ESP_LOGI(CTAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
	        obtain_time();
	        // update 'now' variable with current time
	        time(&now);
            localtime_r(&now, &timeinfo);
	    }
}

void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

esp_err_t i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void bno055_check_calibration() {
    uint8_t calib_status;
    while (1) {
        // Kalibrasyon durumunu kontrol et
        esp_err_t ret = i2c_read(BNO055_ADDRESS, CALIB_STAT, &calib_status, 1);
        if (ret != ESP_OK) {
            ESP_LOGE(BTAG, "Failed to read calibration status");
        } else {
            ESP_LOGI(BTAG, "Calibration Status: Sys=%d Gyro=%d Accel=%d Mag=%d", 
                     (calib_status >> 6) & 0x03, 
                     (calib_status >> 4) & 0x03, 
                     (calib_status >> 2) & 0x03, 
                     calib_status & 0x03);

            if (((calib_status >> 6) & 0x03) == 3 && 
                ((calib_status >> 4) & 0x03) == 3 && 
                ((calib_status >> 2) & 0x03) == 3 && 
                (calib_status & 0x03) == 3) {
                ESP_LOGI(BTAG, "Calibration complete");
                break; // Kalibrasyon tamamlandıysa döngüden çık
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 saniyede bir kontrol et
    }
}

void bno055_read_euler_angles(int16_t *yaw, int16_t *pitch, int16_t *roll) {
    uint8_t buffer[6];
    esp_err_t ret = i2c_read(BNO055_ADDRESS, EUL_DATA_X_LSB, buffer, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(BTAG, "Failed to read Euler angles");
        return;
    }

    *yaw = (int16_t)((buffer[1] << 8) | buffer[0]);
    *pitch = (int16_t)((buffer[3] << 8) | buffer[2]);
    *roll = (int16_t)((buffer[5] << 8) | buffer[4]);

    ESP_LOGI(BTAG, "Yaw: %f, Pitch: %f, Roll: %f", *yaw / 16.0, *pitch / 16.0, *roll / 16.0);
}

void bno055_read_accel_gyro(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z,int16_t *linear_accel_x, int16_t *linear_accel_y, int16_t *linear_accel_z, int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z) {
    uint8_t gyro_buffer[6];
    uint8_t acc_buffer[6];
    uint8_t linear_acc_buffer[6];
    esp_err_t ret;

    // İvme verilerini oku
    ret = i2c_read(BNO055_ADDRESS, ACC_DATA_X_LSB, acc_buffer, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(BTAG, "Failed to read accelerometer data");
        return;
    }
    *accel_x = (int16_t)((acc_buffer[1] << 8) | acc_buffer[0]);
    *accel_y = (int16_t)((acc_buffer[3] << 8) | acc_buffer[2]);
    *accel_z = (int16_t)((acc_buffer[5] << 8) | acc_buffer[4]);


    ret = i2c_read(BNO055_ADDRESS, LIA_DATA_X_LSB, linear_acc_buffer, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(BTAG, "Failed to read linear accelerometer data");
        return;
    }
    *linear_accel_x = (int16_t)((linear_acc_buffer[1] << 8) | linear_acc_buffer[0]);
    *linear_accel_y = (int16_t)((linear_acc_buffer[3] << 8) | linear_acc_buffer[2]);
    *linear_accel_z = (int16_t)((linear_acc_buffer[5] << 8) | linear_acc_buffer[4]);

    // Jiroskop verilerini oku
    ret = i2c_read(BNO055_ADDRESS, GYR_DATA_X_LSB, gyro_buffer, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(BTAG, "Failed to read gyroscope data");
        return;
    }
    *gyro_x = (int16_t)((gyro_buffer[1] << 8) | gyro_buffer[0]);
    *gyro_y = (int16_t)((gyro_buffer[3] << 8) | gyro_buffer[2]);
    *gyro_z = (int16_t)((gyro_buffer[5] << 8) | gyro_buffer[4]);

  ESP_LOGI(BTAG, "Accel X: %f, Accel Y: %f, Accel Z: %f", 
         (double)(*accel_x) / 100.0, (double)(*accel_y) / 100.0, (double)(*accel_z) / 100.0);

ESP_LOGI(BTAG, "Linear Accel X: %f, Linear Accel Y: %f, Linear Accel Z: %f", 
         (double)(*linear_accel_x) / 100.0, (double)(*linear_accel_y) / 100.0, (double)(*linear_accel_z) / 100.0);

ESP_LOGI(BTAG, "Gyro X: %f, Gyro Y: %f, Gyro Z: %f", 
         (double)(*gyro_x) / 900.0, (double)(*gyro_y) / 900.0, (double)(*gyro_z) / 900.0);

}



static esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");   
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

void write_data_to_new_file(int file_number, const char *data)
{
    // Dinamik dosya adı oluşturma (örneğin: "00000001.txt", "00000002.txt")
    char file_name[512];
    snprintf(file_name, sizeof(file_name), MOUNT"/%08d.txt", file_number);

    // Dosyayı yazma modunda aç ("w" ile)
    FILE *f = fopen(file_name, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Veriyi dosyaya yaz
    fprintf(f, "%s\n", data);  // Veriyi dosyaya yazıyoruz
    fclose(f);

    ESP_LOGI(TAG, "Data written to file: %s", file_name);
}




void app_main() {
    esp_err_t ret;
     // I2C'yi başlat
    i2c_master_init();
    i2c_write(BNO055_ADDRESS, BNO055_OPR_MODE, 0x0C);


    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 20,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    ESP_LOGI(TAG, "Using SDMMC peripheral");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    #ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.width = 4;
#else
    slot_config.width = 1;
#endif

    #ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
    slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
    slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
    slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
    slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
#endif  // CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
#endif  // CONFIG_SOC_SDMMC_USE_GPIO_MATRIX

    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(CTAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();


    Set_SystemTime_SNTP();

      // Format FATFS
#ifdef CONFIG_EXAMPLE_FORMAT_SD_CARD
    ret = esp_vfs_fat_sdcard_format(mount_point, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
        return;
    }

    if (stat(file_foo, &st) == 0) {
        ESP_LOGI(TAG, "file still exists");
        return;
    } else {
        ESP_LOGI(TAG, "file doesnt exist, format done");
    }
#endif // CONFIG_EXAMPLE_FORMAT_SD_CARD

   
    
    int file_counter = 0;  
    int first_read = 0; 

    
    // Her saniyede bir veri oku ve kaydet
    while (1) {
        while (1) {

    
    int16_t yaw, pitch, roll;
    int16_t accel_x, accel_y, accel_z;
    int16_t linear_accel_x,linear_accel_y,linear_accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
   

    double gps_lat;
    double gps_lon;
    double gps_alt;
    double speed_n;
    double speed_e;
    double speed_f;
    double speed_l;
    double speed_u;

    double pos_accuracy = NAN;
    double vel_accuracy = NAN; 
    double navstat = NAN;
    double numsats = NAN; 
    double posmode = NAN; 
    double velmode = NAN;    
    double orimode = NAN;

    // Verileri oku
    bno055_read_euler_angles(&yaw, &pitch, &roll);
    bno055_read_accel_gyro(&accel_x, &accel_y, &accel_z,&linear_accel_x, &linear_accel_y, &linear_accel_z, &gyro_x, &gyro_y, &gyro_z);

     if (first_read == 0) {
            // İlk okuma sırasında GPS ve hız verilerini sıfırla
            gps_lat = 40.123456;  // İlk GPS lat
            gps_lon = 29.987654;  // İlk GPS lon
            gps_alt = 150.0;      // İlk GPS alt
            speed_n = 0.0;        // İlk hız x
            speed_e = 0.0;        // İlk hız y
            speed_f = 0.0; 
            speed_l = 0.0; 
            speed_u = 0.0; 
           
            
        } else {
            // Sonraki okumalarda GPS ve hız verilerini NAN yap
            gps_lat = NAN;
            gps_lon = NAN;
            gps_alt = NAN;
            speed_n = NAN;        
            speed_e = NAN;       
            speed_f = NAN; 
            speed_l = NAN; 
            speed_u = NAN; 
        }
    

    char data[EXAMPLE_MAX_CHAR_SIZE];

        // Sensör verilerini oku ve formatla
        snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f",
         gps_lat, gps_lon, gps_alt,
         (double)roll / 16.0,(double)yaw / 16.0, (double)pitch / 16.0,
         speed_n, speed_e, speed_f, speed_l, speed_u,
         (double)accel_x / 100.0, (double)accel_y / 100.0, (double)accel_z / 100.0,
         (double)linear_accel_x / 100.0, (double)linear_accel_y / 100.0, (double)linear_accel_z / 100.0,
         (double)gyro_x / 900.0, (double)gyro_y / 900.0, (double)gyro_z / 900.0,
         (double)gyro_x / 900.0, (double)gyro_y / 900.0, (double)gyro_z / 900.0,
         pos_accuracy, vel_accuracy, navstat, numsats, posmode, velmode, orimode);


        // Veriyi yeni dosyaya yaz
        write_data_to_new_file(file_counter, data);

        
        file_counter++;
        first_read++;
     

       
    const char *file_time = MOUNT_POINT"/time.txt";
    struct tm timeinfo;
    struct timeval tv;
    char strftime_buf[64];
    char time_with_milliseconds[128];
    
   
    // Şu anki zamanı al (saniye ve mikrosaniye)
    gettimeofday(&tv, NULL); // tv.tv_sec -> saniye, tv.tv_usec -> mikrosaniye

    // tv_sec'i struct tm'ye dönüştür
    localtime_r(&tv.tv_sec, &timeinfo);

    // Zamanı "Gün-Ay-Yıl Saat:Dakika:Saniye" formatında bir stringe çevir
    // Zamanı "Yıl-Ay-Gün Saat:Dakika:Saniye" formatında bir stringe çevir
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // Milisaniye (tv.tv_usec) değerini al ve stringe ekle, ardından newline ekle
    snprintf(time_with_milliseconds, sizeof(time_with_milliseconds), "%s.%06ld\n", 
            strftime_buf, tv.tv_usec); // tv.tv_usec'i milisaniyeye çevir

    // Zamanı dosyaya yaz (alt alta yazmak için dosyayı ekleme modunda aç)
    s_example_write_file(file_time, time_with_milliseconds);
    
    Get_current_date_time(Current_Date_Time);
 
    vTaskDelay(pdMS_TO_TICKS(10));
}

   
}

    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");


}
