#include "setting.h"
#include "errno.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>

#define Maxhosts 16
#define HOST_CONF_FILE ".jiahosts"
#define ptr_from_int64(p) (void *)(unsigned long long)(p)

setting_t system_setting = {0};     // there, we can set default system configuration

void trim(char* str) {
    char* start = str;
    char* end = str + strlen(str) - 1;

    // trim leading and trailing whitespace
    while(isspace((unsigned char)*start)) start++;
    while(end > start && isspace((unsigned char)*end)) end--;

    *(end + 1) = '\0';
    memmove(str, start, end - start + 2);
}

int get_options(setting_t *setting){
    FILE *fp;

    if ((fp = fopen(SYSTEM_CONF_PATH, "r")) == NULL) {
        fprintf(stderr, "func-get_options:system config file open error\n");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LEN];
    
    while (fgets(line, MAX_LINE_LEN, fp)) {
        // remove newline characters from the end of the line
        line[strcspn(line, "\n")] = 0;

        // skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // parse the line into the config_option structure
        char *delimiter = strchr(line, '=');
        if (delimiter) {
            config_option_t option;
            // split the line into key and value
            int key_length = delimiter - line;
            strncpy(option.key, line, key_length);
            option.key[key_length] = '\0';
            strncpy(option.value, delimiter + 1, MAX_VALUE_LEN - 1);
            option.value[MAX_VALUE_LEN - 1] = '\0';

            trim(option.key);
            trim(option.value);

            // add the option to the options array
            setting->options[setting->optionc] = option;
            setting->optionc++;
        }
    }

    for(int i = 0; i < setting->optionc; i++) {
        if(strcmp(setting->options[i].key, "system_mode") == 0){
            if(strcmp(setting->options[i].value, "memory") == 0){
                setting->system_mode = MEMORY_MODE;
            } else if(strcmp(setting->options[i].value, "compute") == 0){
                setting->system_mode = COMPUTE_MODE;
            } else if(strcmp(setting->options[i].value, "hybrid") == 0){
                setting->system_mode = HYBRID_MODE;
            } else {
                printf("unknown system mode: %s\n", setting->options[i].value);
            }
        } else if(strcmp(setting->options[i].key, "comm_type") == 0){
            if(strcmp(setting->options[i].value, "tcp") == 0){
                setting->comm_type = tcp;
            } else if(strcmp(setting->options[i].value, "udp") == 0){
                setting->comm_type = udp;
            } else if(strcmp(setting->options[i].value, "rdma") == 0){
                setting->comm_type = rdma;
            }
        } else if(strcmp(setting->options[i].key, "global_start_addr") == 0){
            setting->global_start_addr = strtoull(setting->options[i].value, NULL, 0);
        } else if(strcmp(setting->options[i].key, "msg_buffer_size") == 0) {
            setting->msg_buffer_size = atoi(setting->options[i].value);
        } else {
            printf("Unknown config option: %s = %s\n", setting->options[i].key, setting->options[i].value);
            setting->optionc--;
        }
    }
    fclose(fp);
    return 0;
}

int get_hosts(setting_t *setting){
    setting->hosts = (host_t*)malloc(sizeof(host_t) * Maxhosts);

    if (setting->hosts == NULL) {
        fprintf(stderr, "func-get_hosts: malloc failed\n");
        return -1;
    }

    setting->hostc = 0;

    FILE *fp;
    fp = fopen(HOST_CONF_FILE, "r");
    if (fp == NULL) {
        fprintf(stderr, "func-get_hosts: fopen failed\n");
        return -1;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, MAX_LINE_LEN, fp)) {
        // remove newline characters from the end of the line
        line[strcspn(line, "\n")] = 0;

        // skip empty lines and comments
        if (strlen(line) == 1 || line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // parse the line into the host structure
        if (sscanf(line, "%15[0-9.] %31[^ ] %31[^ ]", \
                   setting->hosts[setting->hostc].ip, \
                   setting->hosts[setting->hostc].username, \
                   setting->hosts[setting->hostc].password) == 3) {
            setting->hosts[setting->hostc].id = setting->hostc;
            setting->hostc++;
        } else {
            log_err("func-get_hosts: invalid line: %s\n", line);
        }

        // check if we have reached the maximum number of hosts
        if (setting->hostc >= Maxhosts){
            break;
        }
    }

    fclose(fp);
    return 0;
}

int get_id(setting_t *setting){
    struct ifaddrs *ifap, *ifa;
    int found = 0;

    if (getifaddrs(&ifap) == -1) {
        fprintf(stderr, "getifaddrs: %s\n", strerror(errno));
        return -1;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        char ipstr[128];
        inet_ntop(ifa->ifa_addr->sa_family, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, ipstr, sizeof ipstr);

        for(int i = 0; i < setting->hostc; i++) {
            if(strcmp(setting->hosts[i].ip, ipstr) == 0) {
                setting->jia_pid = i;
                found = 1;
                break;
            }
        }
        if(found == 1) {
            break;
        }
    }
    
    freeifaddrs(ifap);
    return found == 1 ? 0 : -1;
}


int init_setting(setting_t *setting){
    int ret;

    ret = get_options(setting);
    if (ret!= 0) {
        fprintf(stderr, "func-get_setting: get_options failed\n");
        return -1;
    }

    ret = get_hosts(setting);
    if (ret!= 0) {
        fprintf(stderr, "func-get_setting: get_hosts failed\n");
        return -1;
    }

    ret = get_id(setting);
    if (ret!= 0) {
        fprintf(stderr, "func-get_setting: get_id failed\n");
        return -1;
    }
    return 0;
}

void print_setting(const setting_t *setting){
    printf("===============================================\n");
    printf("system setting info on host [%d]\n", system_setting.jia_pid);
    printf("jia_pid (current host id) : %d\n", setting->jia_pid);
    printf("hostc   (total host count): %d\n", setting->hostc);
    for(int i = 0; i < setting->hostc; i++) {
        printf("host %d: %s %s \t***\n", i, setting->hosts[i].ip, setting->hosts[i].username);
    }
    printf("optionc: %d\n", setting->optionc);
    for(int i = 0; i < setting->optionc; i++) {
        printf("option %d: %s = %s\n", i, setting->options[i].key, setting->options[i].value);
    }
    printf("===============================================\n");
}