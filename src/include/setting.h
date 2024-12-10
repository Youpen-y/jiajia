#if !defined(SETTING_H)
#define SETTING_H
#include <errno.h>

/* max line length */
#define MAX_LINE_LEN 256

/* max key length */
#define MAX_KEY_LEN 100

/* max value length */
#define MAX_VALUE_LEN 156

/* max options number */
#define MAX_OPTIONS_NUM 100

/* system configuration file path */
#define SYSTEM_CONF_PATH "system.conf"

/* configuration option */
typedef struct config_option {
  char key[MAX_KEY_LEN];
  char value[MAX_VALUE_LEN];
} config_option_t;

/* system mode */
enum system_mode { MEMORY_MODE, COMPUTE_MODE, HYBRID_MODE };

/* communication mode */
enum comm_type { tcp, udp, rdma };

typedef struct host {
  int id;            // host id
  char ip[16];       // host ip
  char username[32]; // host username
  char password[32]; // host password
  int homesize;
  int riofd;
  int rerrfd;
} host_t;

/* system configuration */
typedef struct setting {
  enum system_mode system_mode;         // system mode
  enum comm_type comm_type;             // communication mode
  unsigned long long global_start_addr; // global start address

  int msg_buffer_size; // message buffer size
  int msg_queue_size;  // messsage inqueue size

  int jia_pid;                              // current host id
  host_t *hosts;                            // host array
  int hostc;                                // host count
  config_option_t options[MAX_OPTIONS_NUM]; // options array
  int optionc;                              // options count
} setting_t;

extern setting_t system_setting;


/**
 * @brief Init the system setting from system.conf
 *
 * @param setting system setting object
 * @return int 0 if success, -1 if failed
 */
int init_setting(setting_t *setting);

/**
 * @brief Get the options from system.conf
 *
 * @param setting system setting object
 * @return int 0 if success, -1 if failed
 */
int get_options(setting_t *setting);

/**
 * @brief Get the hosts from .hosts specified by setting->hosts_conf_path
 *
 * @param setting system setting object
 * @return int 0 if success, -1 if failed
 * @note Execute after get_options was called
 */
int get_hosts(setting_t *setting);

/**
 * @brief Get the host id according to .jiahosts, and set the
 * system_setting.jia_pid
 *
 * @param setting system setting object
 * @return int 0 if success, -1 if failed
 */
int get_id(setting_t *setting);

/**
 * @brief Print the system setting
 *
 * @param setting system setting object
 */
void print_setting(const setting_t *setting);

/**
 * @brief free_setting -- free the setting resources
 *
 * @param setting
 */
void free_setting(setting_t *setting);


/**
 * @brief trim -- trim leading and trailing whitespace of str
 * 
 * @param str 
 */
static void trim(char* str);

#endif /* SETTING_H */
