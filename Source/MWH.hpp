#pragma once

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <unordered_set>
#include "INIreader.hpp"
#include <thread>
class MWH_Frame {
private:
    INIReader reader;
    std::string ParamPath;
    std::string onValue;
    std::string offValue;
    const std::string log_path = "/sdcard/Android/MW_Controller/日志.txt";
public:
    const std::string list_path = "/sdcard/Android/MW_Controller/排除名单.txt";
    MWH_Frame() : reader("/data/module_switch.ini"), ParamPath(""), onValue(""), offValue("") {}
    void setParamPath(const std::string& path) { ParamPath = path; }
    void setOnValue(const std::string& value) { onValue = value; }
    void setOffValue(const std::string& value) { offValue = value; }

    std::string getParamPath() const { return ParamPath; }
    std::string getOnValue() const { return onValue; }
    std::string getOffValue() const { return offValue; }

    int InotifyMain(const char* dir_name, uint32_t mask) {
        int fd = inotify_init();
        if (fd < 0) {
            printf("Failed to initialize inotify.\n");
            return -1;
        }
 
       int wd = inotify_add_watch(fd, dir_name, mask);
        if (wd < 0) {
            printf("Failed to add watch for directory: %s \n", dir_name);
            close(fd);
            return -1;
        }

        const int buflen = sizeof(struct inotify_event) + NAME_MAX + 1;
        char buf[buflen];
        fd_set readfds;

        while (true) {
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            int iRet = select(fd + 1, &readfds, nullptr, nullptr, nullptr);
            if (iRet < 0) {
                break;
            }

            int len = read(fd, buf, buflen);
            if (len < 0) {
                printf ("Failed to read inotify events.\n");
                break;
            }

            const struct inotify_event* event = reinterpret_cast<const struct inotify_event*>(buf);
            if (event->mask & mask) {
                break; 
            }
        }

        inotify_rm_watch(fd, wd);
        close(fd);

        return 0;
    }
    void Init() {
        char buf[256] = { 0 };
        if (popenRead("pidof CpufreqController", buf, sizeof(buf)) == 0) {
            log("进程检测失败");
            exit(-1);
        }

        auto ptr = strchr(buf, ' ');
        if (ptr) { // "pidNum1 pidNum2 ..."  如果存在多个pid就退出
            char tips[256];
            auto len = snprintf(tips, sizeof(tips),
                "警告: CpufreqController已经在运行 (pid: %s), 当前进程(pid:%d)即将退出", buf, getpid());
            printf("\n!!! \n!!! %s\n!!!\n\n", tips);
            printf(tips, len);
            exit(-2);
        }
    }

    size_t popenRead(const char* cmd, char* buf, const size_t maxLen) {
        auto fp = popen(cmd, "r");
        if (!fp) return 0;
        auto readLen = fread(buf, 1, maxLen, fp);
        pclose(fp);
        return readLen;
    }

    void log(const std::string& message) {
        std::time_t now = std::time(nullptr);
        std::tm* local_time = std::localtime(&now);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", local_time);

        std::ofstream logfile(log_path, std::ios_base::app);
        if (logfile.is_open()) {
            logfile << time_str << " " << message << std::endl;
            logfile.close();
        }
    }

    void clear_log() {
        std::ofstream ofs;
        ofs.open(log_path, std::ofstream::out | std::ofstream::trunc);
        ofs.close();
    }
    std::vector<std::string> readAllowList() {
        std::vector<std::string> allowList;
        std::ifstream file(list_path);

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line[0] != '#') {
                allowList.push_back(line);
            }
        }

        file.close();
        return allowList;
    }
    void readAndParseConfig() {
        INIReader reader("/data/module_switch.ini");

       if (reader.ParseError() < 0) {
            log("警告:请检查您的配置文件是否存在");
            exit(0);
        }

        ParamPath = reader.Get("module_switch", "param_path", "");
        onValue = reader.Get("module_switch", "on_value", "");
        offValue = reader.Get("module_switch", "off_value", "");
    }
    void WriteFile(const std::string& filePath, const std::string& content) noexcept {
        int fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK);
        chmod (filePath.c_str(), 0666);
        write(fd, content.data(), content.size());
        close(fd);
    }
};