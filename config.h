#include <stdlib.h>

#define CONFIG_TYPE_STRING 1
#define CONFIG_TYPE_INT    2
#define CONFIG_TYPE_RR     3

typedef struct TConfig
{
    char      *server_ip;
    uint16_t  server_port;
    char      *dns_ip;
    uint16_t  dns_port;
    uint32_t  cache_time;
    uint8_t   debug_level;
    uint8_t   multi_thread;
    char      *data;
} TConfig;

extern TConfig config;

void config_load();
