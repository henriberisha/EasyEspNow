// #include "mesh_now.h"
#include "MeshNowEsp.h"

int CURRENT_LOG_LEVEL = LOG_VERBOSE; // need to set the log level, otherwise will have issues
constexpr auto MAIN_TAG = "MAIN";    // need to set a tag

esp_err_t ret;

MeshNowEsp mesh;

void setup()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println("Hello World!");
    MONITOR(MAIN_TAG, "Hello World & Mesh");

    mesh.printtest();
    // Serial.println(mesh.getEspNowVersion());

    WiFi.mode(WIFI_MODE_STA);

    Serial.printf("Starting WiFi channel: %d\n", WiFi.channel());

    Serial.println(mesh.begin(9, WIFI_IF_STA));

    Serial.println(mesh.getEspNowVersion());
}

void loop()
{
    // Your code here
}