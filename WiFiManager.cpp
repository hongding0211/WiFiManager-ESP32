//
// Created by Hong on 2021/4/11.
//

#include <WiFiManager.h>

WiFiManager::WiFiManager() {
    macAddr = WiFi.macAddress();
    macAddrPrefix = macAddr.substring(0, 2) + macAddr.substring(3, 5);
    apSSID = "ESP-" + macAddrPrefix;
}

void WiFiManager::specifyWiFi(String ssid, String password) {
    this->ssid = ssid;
    this->password = password;
    wifiSpecified = true;
}

bool WiFiManager::connect(bool autoConnect) {
    Serial.print("");
    if (autoConnect) {
        Serial.print("Auto connect");
        WiFi.begin();
    } else {
        Serial.print("Connecting to " + ssid);
        WiFi.begin(ssid.c_str(), password.c_str());
    }
    if (onConnectHandler)
        onConnectHandler();
    int cnt = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        // Serial.print(WiFi.status());
        if (++cnt > maxRetryTimes)
            break;
    }
    Serial.println("");

    if (WiFi.status() == WL_CONNECTED) {
        connectStatus = true;
        ipAddr = WiFi.localIP().toString();
        if (autoConnect)
            ssid = WiFi.SSID();
        Serial.println("Connected to " + ssid);
        Serial.println("IP: " + ipAddr);
    } else {
        connectStatus = false;
        Serial.println("Connect fail.");
    }
    if (onAfterConnectHandler)
        onAfterConnectHandler(isConnected());
    return isConnected();
}

bool WiFiManager::isConnected() {
    return connectStatus;
}

const String &WiFiManager::getIpAddr() const {
    return ipAddr;
}

void WiFiManager::scan() {
    Serial.println("");
    Serial.println("Scan network...");
    if (onScanHandler)
        onScanHandler();
    int cnt = WiFi.scanNetworks();
    Serial.println("Scan OK");
    networks.clear();
    for (int i = 0; i < cnt; i++) {
        if (WiFi.SSID(i) == "")
            continue;
        networks.push_back(WiFi.SSID(i));
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" ");
        Serial.print(WiFi.RSSI(i));
        Serial.print(" ");
        Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
        delay(10);
    }
    if (onAfterScanHandler)
        onAfterScanHandler();
}

void WiFiManager::runServer() {
    MDNS.begin(("ESP-" + macAddrPrefix).c_str());
    /****** index.html ******/
    server.on("/", [&] {
        server.send(200, "text/html", indexHtml);
    });
    /****** page.html ******/
    server.on("/wificonfig", [&] {
        server.send(200, "text/html", connectHtml);
    });
    /****** update.html *****/
    server.on("/update", [&] {
        server.send(200, "text/html", updateHtml);
    });
    /****** Get net info ******/
    server.on("/getnetinfo", [&] {
        String isCon;
        isCon = connectStatus ? "true" : "false";
        String res = "{\"ssid\": \"" + ssid
                     + "\",\"ipaddr\": \"" + ipAddr
                     + "\",\"macaddr\": \"" + macAddr
                     + "\",\"isConnect\": " + isCon
                     + "}";
        server.send(200, "text/plain", res);
    });
    /****** Get SSIDs ******/
    server.on("/getssids", [&] {
        scan();
        String res = "[";
        for (auto &network:networks) {
            res += "\"" + network + "\",";
        }
        res[res.length() - 1] = ']';
        server.send(200, "text/plain", res);
    });
    /****** Connect *****/
    server.on("/connect", [&] {
        if (server.args() != 2) {
            server.send(400, "text/plain", "ERROR");
            return;
        }
        String ssidBackup = ssid;
        String passwordBackup = password;
        ssid = server.arg(0);
        password = server.arg(1);
        // 如果是已经连接后想要重新连接，由于不是处在 AP 模式下
        // 这将导致连接 WiFi 时，之前的 HTTP 通信会中断
        // 并且连接 WiFi 后，IP 也会发生变化，所以是不能返回结果的
        // 因此这样的情况，会先直接跳转至空白页，用户在后续自行检查连接
        if (isConnected()) {
            server.send(201, "text/plain", "UNSURE");
            WiFi.disconnect();
            connectStatus = false;
            delay(1000);
        }
        if (connect()) {
            server.send(200, "text/plain", "OK");
            // AP mode only stays for 5 mintues
            ticker.once(300, [] {
                Serial.println("AP shut.");
                WiFi.mode(WIFI_STA);
            });
            ticker.active();
        } else {
            server.send(400, "text/plain", "ERROR");
            ssid = ssidBackup;
            password = passwordBackup;
        }
    });
    /****** Upload *****/
    server.on("/upload", HTTP_POST, [&] {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }, [&] {
        HTTPUpload &upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.setDebugOutput(true);
            Serial.printf("Update: %s\n", upload.filename.c_str());
            if (!Update.begin()) { //start with max available size
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
                Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
            Serial.setDebugOutput(false);
        } else {
            Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
        }
    });
    server.begin(80);
}

void WiFiManager::start() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    runServer();
    bool res;
    if (wifiSpecified)
        res = connect();
    else
        res = connect(true);
    if (res)
        return;
    /****** Open soft AP ******/
    Serial.println("Open AP");
    ipAddr = apIP.toString();
    ssid = apSSID;
    WiFi.softAPConfig(apIP, apGateway, apSubMask);
    WiFi.softAP(apSSID.c_str());
    if (onAPOpenHandler)
        onAPOpenHandler(apSSID, apIP.toString());
}

void WiFiManager::run() {
    server.handleClient();
}

void WiFiManager::setOnConnectHandler(const std::function<void(void)> &onConnectHandler) {
    WiFiManager::onConnectHandler = onConnectHandler;
}

void WiFiManager::setOnAfterConnectHandler(const std::function<void(bool)> &onAfterConnectHandler) {
    WiFiManager::onAfterConnectHandler = onAfterConnectHandler;
}

void WiFiManager::setOnScanHandler(const std::function<void(void)> &onScanHandler) {
    WiFiManager::onScanHandler = onScanHandler;
}

void WiFiManager::setOnAfterScanHandler(const std::function<void(void)> &onAfterScanHandler) {
    WiFiManager::onAfterScanHandler = onAfterScanHandler;
}

void WiFiManager::setOnApOpenHandler(const std::function<void(String, String)> &onApOpenHandler) {
    onAPOpenHandler = onApOpenHandler;
}
