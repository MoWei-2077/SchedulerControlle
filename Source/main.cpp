#include "MWH.hpp"

MWH_Frame MWH;
const char* cpusetEventPath = "/dev/cpuset/top-app";
const char* conf_path = "/data/module_switch.ini";
std::vector<std::string> allowList = MWH.readAllowList(); 

std::string GetTopApp() {
    char buffer[128]; 
    const char* cmd = "am stack list | awk '/taskId/&&!/unknown/{print $2}' | awk 'NR==1 || NR==2' | awk -F '/' '{print $1}'";
    
    size_t maxLen = sizeof(buffer);
    MWH.popenRead(cmd, buffer, maxLen);
    
    std::string result(buffer);
    size_t pos = result.find('\n');
    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }
    
    return result;
}

void Updata_List(){
    allowList = MWH.readAllowList();
    for (const auto& item : allowList) {
        MWH.log("排除列表已更新: " + item);
    }
}

bool CheckConfPath(){
    return (!access(conf_path, F_OK));
}

void getConfig(){
    while (true){
        MWH.InotifyMain(MWH.list_path.c_str(), IN_MODIFY);
        Updata_List();
    }
}
int main(){ 
    MWH.Init();
    MWH.clear_log();
    if (!CheckConfPath()){
        MWH.log("配置文件不存在,进程已退出!!!");
        exit(-1);
    }
    MWH.log("调度控制器成功启动!");
    MWH.readAndParseConfig(); 
    std::thread(getConfig).detach();
    MWH.setParamPath(MWH.getParamPath());
    MWH.setOnValue(MWH.getOnValue());
    MWH.setOffValue(MWH.getOffValue());
    Updata_List();
    while (true){
        MWH.InotifyMain(cpusetEventPath, IN_MODIFY);
        std::string topApp = GetTopApp();
        if (std::find(allowList.begin(), allowList.end(), topApp) != allowList.end()) {
            MWH.WriteFile(MWH.getParamPath(), MWH.getOffValue());
        } else {
            MWH.WriteFile(MWH.getParamPath(), MWH.getOnValue());
        }     
    }
    return 0;
}