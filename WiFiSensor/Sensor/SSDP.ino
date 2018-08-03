void SSDP_init(void) {
  // SSDP дескриптор
  HTTP.on("/description.xml", HTTP_GET, []() {
    SSDP.schema(HTTP.client());
  });
  //Если версия  2.0.0 закаментируйте следующую строчку
  SSDP.setDeviceType("upnp:rootdevice");
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName(SSDP_Name);
  SSDP.setSerialNumber("00001");
  SSDP.setURL("/");
  SSDP.setModelName("ESP-remote_sensor");
  SSDP.setModelNumber("000000000001");
  SSDP.setModelURL("http://www.forumhouse.ru/threads/352693/");
  SSDP.setManufacturer("Narodniy Controler");
  SSDP.setManufacturerURL("http://www.forumhouse.ru/threads/352693/");
  SSDP.begin();
}
