#include <M5Cardputer.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <esp_bt_device.h>
#include <algorithm>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
constexpr uint32_t SCAN_SECONDS = 5;
constexpr uint8_t MAX_CONNECT_ATTEMPTS = 3;
constexpr uint32_t CONNECT_TIMEOUT_MS = 8000;
constexpr uint32_t RETRY_DELAYS_MS[MAX_CONNECT_ATTEMPTS] = {1000, 2000, 4000};
constexpr uint16_t COLOR_BG = TFT_BLACK;
constexpr uint16_t COLOR_MAIN = TFT_GREEN;
constexpr uint16_t COLOR_FOUND = TFT_RED;
constexpr uint16_t COLOR_DIM = TFT_DARKGREEN;
constexpr uint16_t COLOR_WARN = TFT_YELLOW;
constexpr uint16_t COLOR_ERROR = TFT_RED;
constexpr uint8_t PAGE_SIZE = 6;

struct DeviceEntry {
  String address;
  String name;
  String manufacturer;
  String advertisedServices;
  String osHint;
  int rssi = -127;
  uint32_t lastSeen = 0;
};

std::vector<DeviceEntry> devices;
SemaphoreHandle_t devicesMutex = nullptr;
volatile bool scanResultsDirty = false;
uint32_t lastScanScreenDrawAt = 0;
uint32_t scanDeadlineAt = 0;
BLEScan *scanner = nullptr;
BLEClient *client = nullptr;
enum class Screen { DeviceList, DeviceDetail, GattServices };
Screen currentScreen = Screen::DeviceList;
uint8_t selectedIndex = 0;
uint8_t pageOffset = 0;
bool scanning = false;
bool connected = false;
bool servicesReady = false;
bool autoReconnect = false;
uint8_t connectAttempts = 0;
uint32_t retryAt = 0;
String statusMessage = "Pronto para buscar";
String selectedAddress;

std::string manufacturerData(BLEAdvertisedDevice &advertised) {
  if (advertised.haveManufacturerData()) return advertised.getManufacturerData();
  return std::string();
}

String manufacturerFromData(const std::string &data) {
  if (data.size() < 2) return "";
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data.data());
  const uint16_t companyId = bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8);
  switch (companyId) {
    case 0x004C: return "Apple";
    case 0x0006: return "Microsoft";
    case 0x000F: return "Broadcom";
    case 0x0075: return "Samsung Electronics";
    case 0x0157: return "Huami/Amazfit";
    case 0x00E0: return "Google";
    case 0x0059: return "Nordic Semiconductor";
    case 0x038F: return "Xiaomi";
    case 0x02E5: return "Espressif";
    default: return "ID 0x" + String(companyId, HEX);
  }
}

String operatingSystemHint(const String &name) {
  String text = name;
  text.toLowerCase();
  if (text.indexOf("iphone") >= 0 || text.indexOf("ipad") >= 0)
    return "Possivel iOS; incerto";
  if (text.indexOf("android") >= 0 || text.indexOf("pixel") >= 0)
    return "Possivel Android; incerto";
  if (text.indexOf("windows") >= 0)
    return "Possivel Windows; incerto";
  return "SO nao identificavel por BLE";
}

String serviceHints(BLEAdvertisedDevice &advertised) {
  String result;
  const uint8_t count = advertised.getServiceUUIDCount();
  for (uint8_t i = 0; i < count && i < 3; ++i) {
    if (!result.isEmpty()) result += ",";
    String uuid = advertised.getServiceUUID(i).toString().c_str();
    if (uuid.length() == 36 && uuid.startsWith("0000") && uuid.endsWith("-0000-1000-8000-00805f9b34fb"))
      uuid = uuid.substring(4, 8);
    result += uuid;
  }
  if (count > 3) result += ",...";
  return result;
}

String visibleOrDash(const String &value) {
  return value.isEmpty() ? "-" : value;
}

void drawScanScreen();
void drawDetailScreen();
void drawTestScreen(const String &message, uint8_t attempt);
void disconnectDevice();
void runConnectionTest();
void runBulkConnectionTest();
bool tryConnectAddress(const String &addressText);

void drawHeader(const char *title) {
  auto &d = M5Cardputer.Display;
  d.fillScreen(COLOR_BG);
  d.setTextDatum(top_left);
  d.setTextSize(1);
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  d.drawString("[ BLE FIELD TESTER ]", 8, 5);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("------------------------------", 8, 18);
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  d.drawString(title, 8, 30);
}

uint8_t visibleDeviceRows() {
  const int available = M5Cardputer.Display.height() - 60 - 30;
  return std::max<int>(1, std::min<int>(PAGE_SIZE, available / 28));
}

void drawScanScreen() {
  currentScreen = Screen::DeviceList;
  drawHeader(scanning ? "BUSCA BLE ATIVA" : "DISPOSITIVOS PROXIMOS");
  auto &d = M5Cardputer.Display;
  std::vector<DeviceEntry> snapshot;
  if (devicesMutex && xSemaphoreTake(devicesMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    snapshot = devices;
    xSemaphoreGive(devicesMutex);
  }
  const size_t deviceCount = snapshot.size();
  const uint8_t visibleRows = visibleDeviceRows();
  if (deviceCount > 0) {
    selectedIndex = std::min<size_t>(selectedIndex, deviceCount - 1);
    if (selectedIndex < pageOffset) pageOffset = selectedIndex;
    if (selectedIndex >= pageOffset + visibleRows) pageOffset = selectedIndex - visibleRows + 1;
  }
  d.setTextColor(scanning ? COLOR_WARN : COLOR_DIM, COLOR_BG);
  d.drawString(scanning ? "Escaneando anuncios..." : String(deviceCount) + " encontrados", 8, 46);
  const int screenHeight = d.height();
  const int listTop = 60;
  const int footerY = screenHeight - 30;
  const int rowStep = 28;
  const uint8_t begin = pageOffset;
  const uint8_t end = std::min<size_t>(deviceCount, begin + visibleRows);
  for (uint8_t i = begin; i < end; ++i) {
    const DeviceEntry &entry = snapshot[i];
    const bool selected = i == selectedIndex;
    d.setTextColor(selected ? COLOR_BG : COLOR_FOUND, selected ? COLOR_FOUND : COLOR_BG);
    String name = entry.name.isEmpty() ? "(sem nome anunciado)" : entry.name;
    if (name.length() > 22) name = name.substring(0, 21) + "~";
    char row[48];
    snprintf(row, sizeof(row), "%c %-22s %4d", selected ? '>' : ' ', name.c_str(), entry.rssi);
    const int y = listTop + (i - begin) * rowStep;
    d.drawString(row, 8, y);
    d.setTextColor(COLOR_DIM, COLOR_BG);
    d.drawString(entry.address, 22, y + 12);
  }
  if (deviceCount > visibleRows) {
    d.setTextColor(COLOR_WARN, COLOR_BG);
    d.drawString("Pg " + String(pageOffset / visibleRows + 1) + "/" +
                     String((deviceCount + visibleRows - 1) / visibleRows),
                 d.width() - 48, 46);
  }
  d.setTextColor(COLOR_WARN, COLOR_BG);
  d.drawString(statusMessage, 8, footerY);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("S buscar  A todos  Enter detalles", 8, screenHeight - 16);
}

void drawDetailScreen() {
  currentScreen = Screen::DeviceDetail;
  drawHeader("DETALHE DO DISPOSITIVO");
  auto &d = M5Cardputer.Display;
  if (!devicesMutex || xSemaphoreTake(devicesMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
  if (devices.empty() || selectedIndex >= devices.size()) {
    xSemaphoreGive(devicesMutex);
    return;
  }
  const DeviceEntry entry = devices[selectedIndex];
  xSemaphoreGive(devicesMutex);
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  String name = entry.name.isEmpty() ? "Nome: (nao anunciado)" : "Nome: " + entry.name;
  if (name.length() > 36) name = name.substring(0, 35) + "~";
  d.drawString(name, 8, 51);
  d.drawString("Endereco: " + entry.address, 8, 69);
  d.drawString("Sinal: " + String(entry.rssi) + " dBm", 8, 87);
  d.drawString("Visto: " + String((millis() - entry.lastSeen) / 1000) + "s atras", 8, 105);
  d.setTextColor(COLOR_WARN, COLOR_BG);
  String manufacturerLine = "Fab: " + visibleOrDash(entry.manufacturer);
  if (manufacturerLine.length() > 38) manufacturerLine = manufacturerLine.substring(0, 37) + "~";
  d.drawString(manufacturerLine, 8, 123);
  String osLine = entry.osHint;
  if (osLine.length() > 38) osLine = osLine.substring(0, 37) + "~";
  d.drawString("SO: " + osLine, 8, 141);
  String serviceLine = "Svc: " + visibleOrDash(entry.advertisedServices);
  if (serviceLine.length() > 38) serviceLine = serviceLine.substring(0, 37) + "~";
  d.drawString(serviceLine, 8, 159);
  d.setTextColor(connected ? COLOR_MAIN : COLOR_WARN, COLOR_BG);
  d.drawString(connected ? "Estado: CONECTADO" : "Estado: DESCONECTADO", 8, 177);
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  d.drawString("> C  Conectar / testar", 8, 195);
  d.drawString("  G  Descobrir servicos GATT", 8, 213);
  d.drawString("X desconectar", 8, 231);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("Esc: voltar", 8, 249);
}

void drawTestScreen(const String &message, uint8_t attempt) {
  drawHeader("TESTE DE CONEXAO");
  auto &d = M5Cardputer.Display;
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  d.drawString("Alvo: " + selectedAddress, 8, 58);
  d.setTextColor(COLOR_WARN, COLOR_BG);
  d.drawString("Tentativa " + String(attempt) + "/" + String(MAX_CONNECT_ATTEMPTS), 8, 82);
  d.drawString(message, 8, 104);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("Enter/Esc: cancelar", 8, 216);
}

void drawServiceList() {
  currentScreen = Screen::GattServices;
  drawHeader("SERVICOS GATT");
  auto &d = M5Cardputer.Display;
  if (!connected || !client) {
    d.setTextColor(COLOR_WARN, COLOR_BG);
    d.drawString("Conecte primeiro ao dispositivo", 8, 55);
  } else {
    auto *services = client->getServices();
    if (!services || services->empty()) {
      d.setTextColor(COLOR_WARN, COLOR_BG);
      d.drawString("Nenhum servico GATT encontrado", 8, 55);
    } else {
      uint8_t row = 0;
      for (auto it = services->begin(); it != services->end() && row < 7; ++it, ++row) {
        d.setTextColor(COLOR_MAIN, COLOR_BG);
        d.drawString(String(row + 1) + ". " + String(it->first.c_str()), 8, 52 + row * 20);
        auto *characteristics = it->second->getCharacteristics();
        d.setTextColor(COLOR_DIM, COLOR_BG);
        d.drawString("   Caracteristicas: " + String(characteristics ? characteristics->size() : 0), 8, 63 + row * 20);
      }
      servicesReady = true;
    }
  }
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("Somente leitura/descoberta; sem escritas", 8, 216);
}

void onDeviceFound(BLEAdvertisedDevice advertised) {
  const String address = advertised.getAddress().toString().c_str();
  if (!devicesMutex || xSemaphoreTake(devicesMutex, 0) != pdTRUE) return;
  auto found = std::find_if(devices.begin(), devices.end(), [&](const DeviceEntry &item) {
    return item.address == address;
  });
  const String name = advertised.haveName() ? String(advertised.getName().c_str()) : "";
  const String manufacturer = manufacturerFromData(manufacturerData(advertised));
  const String services = serviceHints(advertised);
  if (found == devices.end()) {
    DeviceEntry entry;
    entry.address = address;
    entry.name = name;
    entry.manufacturer = manufacturer;
    entry.advertisedServices = services;
    entry.osHint = operatingSystemHint(name);
    entry.rssi = advertised.getRSSI();
    entry.lastSeen = millis();
    devices.push_back(entry);
    scanResultsDirty = true;
  } else {
    if (!name.isEmpty()) found->name = name;
    if (!manufacturer.isEmpty()) found->manufacturer = manufacturer;
    if (!services.isEmpty()) found->advertisedServices = services;
    found->osHint = operatingSystemHint(found->name);
    found->rssi = advertised.getRSSI();
    found->lastSeen = millis();
    scanResultsDirty = true;
  }
  xSemaphoreGive(devicesMutex);
}

class DeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
 public:
  void onResult(BLEAdvertisedDevice advertised) override { onDeviceFound(advertised); }
};

bool tryConnectAddress(const String &addressText) {
  BLEAddress address(addressText.c_str());
  if (!client) client = BLEDevice::createClient();
  return client->connect(address);
}

bool tryConnect() {
  if (selectedIndex >= devices.size()) return false;
  selectedAddress = devices[selectedIndex].address;
  BLEAddress address(selectedAddress.c_str());
  if (!client) client = BLEDevice::createClient();
  return client->connect(address);
}

void drawBulkTestScreen(const String &targetName, uint8_t current, uint8_t total, const String &message) {
  drawHeader("TESTE EM LOTE");
  auto &d = M5Cardputer.Display;
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  d.drawString("Alvo " + String(current) + "/" + String(total), 8, 58);
  String name = targetName.isEmpty() ? "(sem nome)" : targetName;
  if (name.length() > 36) name = name.substring(0, 35) + "~";
  d.drawString(name, 8, 80);
  d.setTextColor(COLOR_WARN, COLOR_BG);
  d.drawString(message, 8, 108);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("Esc: cancelar lote", 8, 216);
}

bool isCancelPressed() {
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return false;
  const auto keys = M5Cardputer.Keyboard.keysState();
  return keys.enter || keys.del || keys.esc;
}

void runConnectionTest();
void runBulkConnectionTest();
void disconnectDevice();

void runConnectionTest() {
  if (devices.empty() || selectedIndex >= devices.size()) return;
  selectedAddress = devices[selectedIndex].address;
  connectAttempts = 0;
  autoReconnect = true;
  while (connectAttempts < MAX_CONNECT_ATTEMPTS && autoReconnect) {
    ++connectAttempts;
    drawTestScreen("Conectando...", connectAttempts);
    if (tryConnect()) {
      connected = true;
      autoReconnect = false;
      statusMessage = "Conexao BLE estabelecida";
      drawTestScreen("CONECTADO", connectAttempts);
      delay(900);
      drawDetailScreen();
      return;
    }
    if (connectAttempts >= MAX_CONNECT_ATTEMPTS) break;
    const uint32_t waitMs = RETRY_DELAYS_MS[connectAttempts - 1];
    drawTestScreen("Falhou; nova tentativa em " + String(waitMs / 1000) + "s", connectAttempts);
    const uint32_t start = millis();
    while (millis() - start < waitMs) {
      if (isCancelPressed()) { autoReconnect = false; break; }
      delay(10);
    }
  }
  statusMessage = autoReconnect ? "Falha apos 3 tentativas" : "Teste cancelado";
  autoReconnect = false;
  drawTestScreen(statusMessage, connectAttempts);
  delay(800);
  drawDetailScreen();
}

void runBulkConnectionTest() {
  std::vector<DeviceEntry> targets;
  if (!devicesMutex || xSemaphoreTake(devicesMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    statusMessage = "Lista ocupada; tente novamente";
    drawScanScreen();
    return;
  }
  targets = devices;
  xSemaphoreGive(devicesMutex);
  if (targets.empty()) {
    statusMessage = "Nenhum dispositivo para testar";
    drawScanScreen();
    return;
  }

  if (connected) disconnectDevice();
  uint8_t successfulConnections = 0;
  size_t attemptedTargets = 0;
  bool cancelled = false;
  for (size_t index = 0; index < targets.size(); ++index) {
    selectedIndex = index;
    selectedAddress = targets[index].address;
    ++attemptedTargets;
    bool connectedThisTarget = false;
    for (uint8_t attempt = 1; attempt <= MAX_CONNECT_ATTEMPTS; ++attempt) {
      drawBulkTestScreen(targets[index].name, index + 1, targets.size(),
                         "Conectando " + String(attempt) + "/" + String(MAX_CONNECT_ATTEMPTS));
      if (tryConnectAddress(selectedAddress)) {
        connectedThisTarget = true;
        ++successfulConnections;
        drawBulkTestScreen(targets[index].name, index + 1, targets.size(), "Conectou; desconectando...");
        delay(500);
        client->disconnect();
        connected = false;
        servicesReady = false;
        break;
      }
      connected = false;
      if (attempt < MAX_CONNECT_ATTEMPTS) {
        const uint32_t waitMs = RETRY_DELAYS_MS[attempt - 1];
        drawBulkTestScreen(targets[index].name, index + 1, targets.size(),
                           "Falhou; tenta de novo em " + String(waitMs / 1000) + "s");
        const uint32_t startedAt = millis();
        while (millis() - startedAt < waitMs) {
          if (isCancelPressed()) { cancelled = true; break; }
          delay(10);
        }
        if (cancelled) break;
      }
    }
    if (cancelled) break;
  }

  if (client && client->isConnected()) client->disconnect();
  connected = false;
  servicesReady = false;
  statusMessage = cancelled
      ? "Lote cancelado: " + String(successfulConnections) + " conectaram"
      : String(successfulConnections) + "/" + String(targets.size()) + " conectaram";
  drawHeader("RESULTADO DO TESTE");
  auto &d = M5Cardputer.Display;
  d.setTextColor(COLOR_WARN, COLOR_BG);
  d.drawString(statusMessage, 8, 70);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("Testados: " + String(attemptedTargets) + "/" + String(targets.size()), 8, 98);
  d.drawString("Enter/Esc: voltar a lista", 8, 216);
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      const auto keys = M5Cardputer.Keyboard.keysState();
      if (keys.enter || keys.esc || keys.del) break;
    }
    delay(10);
  }
  drawScanScreen();
}

void disconnectDevice() {
  autoReconnect = false;
  if (client && client->isConnected()) client->disconnect();
  connected = false;
  servicesReady = false;
  statusMessage = "Desconectado";
}

void startScan() {
  if (scanning) return;
  if (connected) disconnectDevice();
  if (devicesMutex && xSemaphoreTake(devicesMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    scanner->clearResults();
    devices.clear();
    xSemaphoreGive(devicesMutex);
  } else {
    statusMessage = "Lista ocupada; tente novamente";
    drawScanScreen();
    return;
  }
  selectedIndex = 0;
  pageOffset = 0;
  scanResultsDirty = false;
  scanning = true;
  scanDeadlineAt = millis() + (SCAN_SECONDS + 1) * 1000UL;
  statusMessage = "Busca BLE em andamento";
  lastScanScreenDrawAt = 0;
  drawScanScreen();
  const bool started = scanner->start(SCAN_SECONDS, nullptr, false);
  if (!started) {
    scanning = false;
    statusMessage = "Falha ao iniciar BLE; reinicie";
    drawScanScreen();
    return;
  }

  const uint32_t start = millis();
  lastScanScreenDrawAt = start;
  scanResultsDirty = false;
  while (static_cast<int32_t>(millis() - scanDeadlineAt) < 0) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      const auto keys = M5Cardputer.Keyboard.keysState();
      if (keys.esc || keys.del) {
        scanner->stop();
        break;
      }
    }
    const uint32_t now = millis();
    if (scanResultsDirty && now - lastScanScreenDrawAt >= 250) {
      scanResultsDirty = false;
      lastScanScreenDrawAt = now;
      drawScanScreen();
    }
    delay(10);
  }
  scanner->stop();
  scanning = false;
  statusMessage = devices.empty()
      ? "Nada anunciado: ative BLE no aparelho"
      : "Busca concluida - selecione um alvo";
  drawScanScreen();
}

void handleScanKeys(const Keyboard_Class::KeysState &keys) {
  if (scanning) return;
  size_t deviceCount = 0;
  bool selectionChanged = false;
  bool openDetails = false;
  if (!devicesMutex || xSemaphoreTake(devicesMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
  deviceCount = devices.size();
  const uint8_t visibleRows = visibleDeviceRows();
  if (deviceCount > 0) {
    const size_t previousIndex = selectedIndex;
    if (keys.up && selectedIndex > 0) --selectedIndex;
    if (keys.down && selectedIndex + 1 < deviceCount) ++selectedIndex;
    if (keys.right && selectedIndex + 1 < deviceCount)
      selectedIndex = std::min<size_t>(deviceCount - 1, pageOffset + visibleRows);
    if (keys.left && selectedIndex > 0)
      selectedIndex = pageOffset > 0 ? pageOffset - 1 : 0;
    if (keys.enter) openDetails = deviceCount > 0;
    if (selectedIndex < pageOffset) pageOffset = selectedIndex;
    if (selectedIndex >= pageOffset + visibleRows) pageOffset = selectedIndex - visibleRows + 1;
    selectionChanged = previousIndex != selectedIndex;
  }
  xSemaphoreGive(devicesMutex);
  if (openDetails) {
    drawDetailScreen();
    return;
  }
  if (selectionChanged) {
    drawScanScreen();
    return;
  }
  for (char key : keys.word) {
    if (key == 's' || key == 'S') { startScan(); return; }
    if (key == 'a' || key == 'A') { runBulkConnectionTest(); return; }
  }
  if (keys.enter && deviceCount > 0) drawDetailScreen();
}

void handleDetailKeys(const Keyboard_Class::KeysState &keys) {
  bool openGatt = false;
  bool connect = keys.enter;
  for (char key : keys.word) {
    if (key == 'c' || key == 'C') connect = true;
    if (key == 'g' || key == 'G') openGatt = true;
    if (key == 'x' || key == 'X') disconnectDevice();
  }
  if (connect) runConnectionTest();
  else if (openGatt) { drawServiceList(); delay(1000); }
}
}  // namespace

void setup() {
  auto config = M5.config();
  M5Cardputer.begin(config, true);
  M5Cardputer.Display.setRotation(1);
  devicesMutex = xSemaphoreCreateMutex();
  BLEDevice::init("Cardputer-BLE-Tester");
  scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new DeviceCallbacks(), false);
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(80);
  drawScanScreen();
}

void loop() {
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    const auto keys = M5Cardputer.Keyboard.keysState();
    if (keys.esc || keys.del) {
      if (currentScreen == Screen::GattServices && !devices.empty()) drawDetailScreen();
      else drawScanScreen();
    } else if (keys.enter || keys.up || keys.down || keys.left || keys.right || !keys.word.empty()) {
      if (currentScreen == Screen::DeviceList) {
        handleScanKeys(keys);
      } else if (currentScreen == Screen::DeviceDetail) {
        handleDetailKeys(keys);
      } else {
        for (char key : keys.word) {
          if (key == 'g' || key == 'G') drawServiceList();
        }
      }
    }
  }
  delay(10);
}
