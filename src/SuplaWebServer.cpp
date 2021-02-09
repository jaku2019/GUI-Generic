/*
  Copyright (C) krycha88

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "SuplaWebServer.h"
#include "SuplaDeviceGUI.h"
#include "SuplaWebPageConfig.h"
#include "SuplaWebPageControl.h"
#include "SuplaWebPageRelay.h"
#include "SuplaWebPageSensor.h"
#include "SuplaWebPageStatusLed.h"
#include "SuplaCommonPROGMEM.h"
#include "SuplaTemplateBoard.h"
#include "Markup.h"
#include "SuplaWebPageOther.h"

String webContentBuffer;

SuplaWebServer::SuplaWebServer() {
}

void SuplaWebServer::begin() {
  this->createWebServer();

  strcpy(this->www_username, ConfigManager->get(KEY_LOGIN)->getValue());
  strcpy(this->www_password, ConfigManager->get(KEY_LOGIN_PASS)->getValue());

  httpServer.onNotFound(std::bind(&SuplaWebServer::handleNotFound, this));
  httpServer.begin(80);
}

void SuplaWebServer::iterateAlways() {
  httpServer.handleClient();
}

void SuplaWebServer::createWebServer() {
  String path = PATH_START;
  httpServer.on(path, HTTP_GET, std::bind(&SuplaWebServer::handle, this));
  path = PATH_START;
  httpServer.on(path, std::bind(&SuplaWebServer::handleSave, this));
  path = PATH_START;
  path += PATH_REBOT;
  httpServer.on(path, std::bind(&SuplaWebServer::supla_webpage_reboot, this));
  path = PATH_START;
  path += PATH_DEVICE_SETTINGS;
  httpServer.on(path, std::bind(&SuplaWebServer::handleDeviceSettings, this));
  path = PATH_START;
  path += PATH_SAVE_BOARD;
  httpServer.on(path, std::bind(&SuplaWebServer::handleBoardSave, this));

#if defined(SUPLA_RELAY) || defined(SUPLA_ROLLERSHUTTER)
  WebPageRelay->createWebPageRelay();
#endif
#if defined(SUPLA_BUTTON) || defined(SUPLA_LIMIT_SWITCH)
  WebPageControl->createWebPageControl();
#endif
  WebPageSensor->createWebPageSensor();
#ifdef SUPLA_CONFIG
  WebPageConfig->createWebPageConfig();
#endif
#ifdef SUPLA_OTA
  httpUpdater.setup(&httpServer, this->www_username, this->www_password);
#endif

  createWebDownload();
  createWebUpload();
  createWebTools();
  createWebStatusLed();
  createWebPageOther();
}

void SuplaWebServer::handle() {
  //  Serial.println(F("HTTP_GET - metoda handle"));
  if (ConfigESP->configModeESP == NORMAL_MODE) {
    if (!httpServer.authenticate(this->www_username, this->www_password))
      return httpServer.requestAuthentication();
  }
  supla_webpage_start(0);
}

void SuplaWebServer::handleSave() {
  //  Serial.println(F("HTTP_POST - metoda handleSave"));
  if (ConfigESP->configModeESP == NORMAL_MODE) {
    if (!httpServer.authenticate(this->www_username, this->www_password))
      return httpServer.requestAuthentication();
  }

  if (strcmp(httpServer.arg(PATH_REBOT).c_str(), "1") == 0) {
    ConfigESP->rebootESP();
    return;
  }

  ConfigManager->set(KEY_WIFI_SSID, httpServer.arg(INPUT_WIFI_SSID).c_str());
  ConfigManager->set(KEY_WIFI_PASS, httpServer.arg(INPUT_WIFI_PASS).c_str());
  ConfigManager->set(KEY_SUPLA_SERVER, httpServer.arg(INPUT_SERVER).c_str());
  ConfigManager->set(KEY_SUPLA_EMAIL, httpServer.arg(INPUT_EMAIL).c_str());
  ConfigManager->set(KEY_HOST_NAME, httpServer.arg(INPUT_HOSTNAME).c_str());
  ConfigManager->set(KEY_LOGIN, httpServer.arg(INPUT_MODUL_LOGIN).c_str());
  ConfigManager->set(KEY_LOGIN_PASS, httpServer.arg(INPUT_MODUL_PASS).c_str());

#ifdef SUPLA_ROLLERSHUTTER
  if (strcmp(WebServer->httpServer.arg(INPUT_ROLLERSHUTTER).c_str(), "") != 0) {
    ConfigManager->set(KEY_MAX_ROLLERSHUTTER, httpServer.arg(INPUT_ROLLERSHUTTER).c_str());
  }
#endif

  switch (ConfigManager->save()) {
    case E_CONFIG_OK:
      //      Serial.println(F("E_CONFIG_OK: Dane zapisane"));
      if (ConfigESP->configModeESP == NORMAL_MODE) {
        supla_webpage_start(1);
        ConfigESP->rebootESP();
      }
      else {
        supla_webpage_start(7);
      }
      break;

    case E_CONFIG_FILE_OPEN:
      //      Serial.println(F("E_CONFIG_FILE_OPEN: Couldn't open file"));
      supla_webpage_start(4);
      break;
  }
}

void SuplaWebServer::handleDeviceSettings() {
  if (ConfigESP->configModeESP == NORMAL_MODE) {
    if (!httpServer.authenticate(www_username, www_password))
      return httpServer.requestAuthentication();
  }
  deviceSettings(0);
}

void SuplaWebServer::supla_webpage_start(int save) {
  webContentBuffer += SuplaSaveResult(save);
  webContentBuffer += SuplaJavaScript();

  addForm(webContentBuffer, F("post"));
  addFormHeader(webContentBuffer, S_SETTING_WIFI_SSID);
  addTextBox(webContentBuffer, INPUT_WIFI_SSID, S_WIFI_SSID, KEY_WIFI_SSID, 0, MAX_SSID, true);
  addTextBoxPassword(webContentBuffer, INPUT_WIFI_PASS, S_WIFI_PASS, KEY_WIFI_PASS, MIN_PASSWORD, MAX_PASSWORD, true);
  addTextBox(webContentBuffer, INPUT_HOSTNAME, S_HOST_NAME, KEY_HOST_NAME, 0, MAX_HOSTNAME, true);
  addFormHeaderEnd(webContentBuffer);

  addFormHeader(webContentBuffer, S_SETTING_SUPLA);
  addTextBox(webContentBuffer, INPUT_SERVER, S_SUPLA_SERVER, KEY_SUPLA_SERVER, DEFAULT_SERVER, 0, MAX_SUPLA_SERVER, true);
  addTextBox(webContentBuffer, INPUT_EMAIL, S_SUPLA_EMAIL, KEY_SUPLA_EMAIL, DEFAULT_EMAIL, 0, MAX_EMAIL, true);
  addFormHeaderEnd(webContentBuffer);

  addFormHeader(webContentBuffer, S_SETTING_ADMIN);
  addTextBox(webContentBuffer, INPUT_MODUL_LOGIN, S_LOGIN, KEY_LOGIN, 0, MAX_MLOGIN, true);
  addTextBoxPassword(webContentBuffer, INPUT_MODUL_PASS, S_LOGIN_PASS, KEY_LOGIN_PASS, MIN_PASSWORD, MAX_MPASSWORD, true);
  addFormHeaderEnd(webContentBuffer);

#ifdef SUPLA_ROLLERSHUTTER
  uint8_t maxrollershutter = ConfigManager->get(KEY_MAX_RELAY)->getValueInt();
  if (maxrollershutter >= 2) {
    addFormHeader(webContentBuffer, S_ROLLERSHUTTERS);
    addNumberBox(webContentBuffer, INPUT_ROLLERSHUTTER, S_QUANTITY, KEY_MAX_ROLLERSHUTTER, (maxrollershutter / 2));
    addFormHeaderEnd(webContentBuffer);
  }
#endif

#ifdef SUPLA_DS18B20
  WebPageSensor->showDS18B20(true);
#endif

  addButtonSubmit(webContentBuffer, S_SAVE);
  addFormEnd(webContentBuffer);

  addButton(webContentBuffer, S_DEVICE_SETTINGS, PATH_DEVICE_SETTINGS);
  addButton(webContentBuffer, F("Narzędzia"), PATH_TOOLS);

  WebServer->sendContent();
}

void SuplaWebServer::supla_webpage_reboot() {
  if (ConfigESP->configModeESP == NORMAL_MODE) {
    if (!httpServer.authenticate(www_username, www_password))
      return httpServer.requestAuthentication();
  }
  supla_webpage_start(2);
  ConfigESP->rebootESP();
}

void SuplaWebServer::deviceSettings(int save) {
  webContentBuffer += SuplaSaveResult(save);
  webContentBuffer += SuplaJavaScript(PATH_DEVICE_SETTINGS);

  addForm(webContentBuffer, F("post"), PATH_SAVE_BOARD);
  addFormHeader(webContentBuffer, S_TEMPLATE_BOARD);
  uint8_t selected = ConfigManager->get(KEY_BOARD)->getValueInt();
  addListBox(webContentBuffer, INPUT_BOARD, S_TYPE, BOARD_P, MAX_MODULE, selected);
  addFormHeaderEnd(webContentBuffer);
  addButtonSubmit(webContentBuffer, S_SAVE);
  addFormEnd(webContentBuffer);

  addFormHeader(webContentBuffer, S_DEVICE_SETTINGS);
#if defined(SUPLA_RELAY) || defined(SUPLA_ROLLERSHUTTER)
  addButton(webContentBuffer, S_RELAYS, PATH_RELAY);
#endif

#ifdef SUPLA_BUTTON
  addButton(webContentBuffer, S_BUTTONS, PATH_CONTROL);
#endif

#ifdef SUPLA_LIMIT_SWITCH
  addButton(webContentBuffer, F("KONTAKTRON"), PATH_SWITCH);
#endif

  addButton(webContentBuffer, F("LED"), PATH_LED);

#if defined(SUPLA_DS18B20) || defined(SUPLA_DHT11) || defined(SUPLA_DHT22) || defined(SUPLA_SI7021_SONOFF)
  addButton(webContentBuffer, S_SENSORS_1WIRE, PATH_1WIRE);
#endif

#if defined(SUPLA_BME280) || defined(SUPLA_SI7021) || defined(SUPLA_SHT3x) || defined(SUPLA_OLED) || defined(SUPLA_MCP23017)
  addButton(webContentBuffer, S_SENSORS_I2C, PATH_I2C);
#endif

#if defined(SUPLA_MAX6675)
  addButton(webContentBuffer, S_SENSORS_SPI, PATH_SPI);
#endif

#if defined(SUPLA_HC_SR04) || defined(SUPLA_IMPULSE_COUNTER)
  addButton(webContentBuffer, S_SENSORS_OTHER, PATH_OTHER);
#endif

#ifdef SUPLA_CONFIG
  addButton(webContentBuffer, S_LED_BUTTON_CFG, PATH_CONFIG);
#endif
  addFormHeaderEnd(webContentBuffer);
  addButton(webContentBuffer, S_RETURN, "");

  WebServer->sendContent();
}

void SuplaWebServer::handleBoardSave() {
  if (ConfigESP->configModeESP == NORMAL_MODE) {
    if (!httpServer.authenticate(this->www_username, this->www_password))
      return httpServer.requestAuthentication();
  }
  String input = INPUT_BOARD;

  if (strcmp(WebServer->httpServer.arg(input).c_str(), "") != 0) {
    ConfigManager->set(KEY_BOARD, httpServer.arg(input).c_str());

    int nr;
    uint8_t key;
    for (nr = 0; nr <= 17; nr++) {
      key = KEY_GPIO + nr;
      ConfigManager->set(key, "");
    }

    chooseTemplateBoard(WebServer->httpServer.arg(input).toInt());
  }

  switch (ConfigManager->save()) {
    case E_CONFIG_OK:
      deviceSettings(1);
      break;
    case E_CONFIG_FILE_OPEN:
      deviceSettings(2);
      break;
  }
}

void SuplaWebServer::sendContent() {
  // httpServer.send(200, "text/html", "");
  // const int bufferSize = 1000;
  // String _buffer;
  //_buffer.reserve(bufferSize);
  // int bufferCounter = 0;

  httpServer.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  httpServer.sendHeader(F("Pragma"), F("no-cache"));
  httpServer.sendHeader(F("Expires"), F("-1"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.chunkedResponseModeStart(200, F("text/html"));

  httpServer.sendContent_P(HTTP_META);
  httpServer.sendContent_P(HTTP_FAVICON);
  httpServer.sendContent_P(HTTP_STYLE);
  httpServer.sendContent_P(HTTP_LOGO);

  String summary = FPSTR(HTTP_SUMMARY);

  summary.replace(F("{h}"), ConfigManager->get(KEY_HOST_NAME)->getValue());
  summary.replace(F("{s}"), ConfigESP->getLastStatusSupla());
  summary.replace(F("{v}"), Supla::Channel::reg_dev.SoftVer);
  summary.replace(F("{g}"), ConfigManager->get(KEY_SUPLA_GUID)->getValueHex(SUPLA_GUID_SIZE));
  summary.replace(F("{m}"), ConfigESP->getMacAddress(true));
  summary.replace(F("{f}"), String(ESP.getFreeHeap() / 1024.0));

  httpServer.sendContent(summary);
  httpServer.sendContent_P(HTTP_COPYRIGHT);

  // httpServer.send(200, "text/html", "");
  /*for (int i = 0; i < fileSize; i++) {
    _buffer += content[i];
    bufferCounter++;

    if (bufferCounter >= bufferSize) {
      httpServer.sendContent(_buffer);
      yield();
      bufferCounter = 0;
      _buffer = "";
    }
  }
  if (bufferCounter > 0) {
    httpServer.sendContent(_buffer);
    yield();
    bufferCounter = 0;
    _buffer = "";
  }*/

  httpServer.sendContent(webContentBuffer);
  httpServer.sendContent_P(HTTP_RBT);
  httpServer.chunkedResponseFinalize();

#ifdef DEBUG_MODE
  Serial.printf_P(PSTR("Content size=%d\n"), webContentBuffer.length());
  Serial.printf_P(PSTR("Sent INDEX...Free mem=%d\n"), ESP.getFreeHeap());
  checkRAM();
#endif
  webContentBuffer = "";
}

void SuplaWebServer::handleNotFound() {
  httpServer.sendHeader("Location", "/", true);
  // httpServer.send(302, "text/plane", "");

  supla_webpage_reboot();
}

bool SuplaWebServer::saveGPIO(const String& _input, uint8_t function, uint8_t nr, const String& input_max) {
  uint8_t current_value, key;
  String input;
  input = _input;

  if (nr != 0) {
    input += nr;
  }
  else {
    nr = 1;
  }

  if (strcmp(WebServer->httpServer.arg(input).c_str(), "") == 0) {
    return true;
  }

  key = KEY_GPIO + WebServer->httpServer.arg(input).toInt();

  if (ConfigESP->getGpio(nr, function) != WebServer->httpServer.arg(input).toInt() || WebServer->httpServer.arg(input).toInt() == OFF_GPIO) {
    ConfigESP->clearGpio(ConfigESP->getGpio(nr, function), function);
  }

  if (WebServer->httpServer.arg(input).toInt() != OFF_GPIO) {
    if (ConfigManager->get(key)->getElement(FUNCTION).toInt() == FUNCTION_OFF) {
      ConfigESP->setGpio(WebServer->httpServer.arg(input).toInt(), nr, function, 1);
    }
    else if (ConfigESP->getGpio(nr, function) == WebServer->httpServer.arg(input).toInt() &&
             ConfigManager->get(key)->getElement(FUNCTION).toInt() == function) {
      ConfigESP->setGpio(WebServer->httpServer.arg(input).toInt(), nr, function, ConfigESP->getLevel(nr, function));
    }
    else if (function == FUNCTION_CFG_BUTTON) {
      ConfigESP->setGpio(WebServer->httpServer.arg(input).toInt(), FUNCTION_CFG_BUTTON);
    }
    else {
      return false;
    }
  }

  if (input_max != "\n") {
    current_value = WebServer->httpServer.arg(input_max).toInt();
    if (ConfigManager->get(key)->getElement(NR).toInt() > current_value) {
      ConfigESP->clearGpio(ConfigESP->getGpio(nr, function), function);
    }
  }
  return true;
}

bool SuplaWebServer::saveGpioMCP23017(const String& _input, uint8_t function, uint8_t nr, const String& input_max) {
  uint8_t key, address, addressInput, gpio, gpioInput, functionElementInput;
  String input = _input + nr;

  if (strcmp(WebServer->httpServer.arg(input).c_str(), "") == 0) {
    return true;
  }

  addressInput = WebServer->httpServer.arg(INPUT_ADRESS_MCP23017).toInt();

  gpioInput = WebServer->httpServer.arg(input).toInt();
  key = KEY_GPIO + gpioInput;
  functionElementInput = ConfigManager->get(key)->getElement(ConfigESP->getFunctionMCP23017(addressInput)).toInt();
  gpio = ConfigESP->getGpioMCP23017(nr, function);
  if (addressInput == OFF_MCP23017) {
    ConfigESP->clearGpioMCP23017(gpio, nr, function);
    return true;
  }

  if (gpioInput != OFF_GPIO) {
    if (functionElementInput == FUNCTION_OFF) {
      ConfigESP->setGpioMCP23017(gpioInput, addressInput, nr, function, 1, 0);
    }
    else if (gpio == gpioInput && functionElementInput == function) {
      ConfigESP->setGpioMCP23017(gpioInput, addressInput, nr, function, ConfigESP->getLevel(nr, function), ConfigESP->getMemory(nr, function));
    }
    else {
      return false;
    }
  }
  return true;
}