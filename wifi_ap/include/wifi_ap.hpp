#ifndef WIFI_AP_HPP
#define WIFI_AP_HPP

#include <string>
#include <vector>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"

class WifiAP
{

public:
    typedef struct
    {
        std::string hostname;

    } client_t;

    WifiAP();
    WifiAP(std::string ssid, std::string password);
    esp_err_t initialize_ap();
    std::vector<client_t> get_connected_clients();

private:
    static const inline char TAG[] = "Wifi AP";
    std::string ap_ssid;
    std::string ap_pass;

    static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    std::vector<client_t> connected_clients;
};

#endif /* WIFI_AP_HPP */