# WiFiManager for ESP32

**之前在ESP8266上写过一个很简陋的类似功能。这个就把界面做的好看点，并且把 WiFi 配置和固件升级都整合在一起，可以直接调用。**

### 功能
- 网络状态显示
- 网络配置（初次配置联网 / 连接新的 WiFi）
- 固件上传升级
- 适配深色模式
- 重启记忆重连

<img src="https://raw.githubusercontent.com/HongDing97/imgs/main/20210509003650.jpg" style="zoom:15%;" />
<img src="https://raw.githubusercontent.com/HongDing97/imgs/main/20210509003534.png" style="zoom:15%;" />
<img src="https://raw.githubusercontent.com/HongDing97/imgs/main/20210509003544.png" style="zoom:15%;" />

### Quick Start
```cpp
#include <Arduino.h>
#include <WiFiManager.h>

WiFiManager wifiManager;

void setup() {
// write your initialization code here    
    Serial.begin(115200);
    wifiManager.start();
}

void loop() {
// write your code here
    wifiManager.run();
}
```

### 初次启动配置

ESP32 支持自动连接，可以记忆上次的 WiFi 实现自动重连。

模块启动时会先尝试连接上一次的节点，如果失败的话则会打开一个名为 ESP-XXXX 的热点。接着连接热点访问 192.168.4.1 就会出现上面的配置界面。设备连接后也可以直接通过 ESP 被分配的 IP 来访问。

> onApOpenHandler(String ssid, String ip)  在AP打开时会被调用，可以通过它实现其他交互

### 固件上传

只要将编译好的 *.bin 文件传入即可，这样就可以精简下载外围电路，也方便升级。

![](https://raw.githubusercontent.com/HongDing97/imgs/main/20210509005623.png)

### 其他功能

- 有 5 个回调函数，通过 setter 设置后可以与其他类进行交互

- 如果想把 SSID 和密码写死，可以直接指明

  ```cpp
  void setup() {
  // write your initialization code here
      Serial.begin(115200);
      wifiManager.specifyWiFi("ssid", "password");
      wifiManager.start();
  }
  ```

  
