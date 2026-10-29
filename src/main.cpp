#include <M5Cardputer.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <algorithm>
#include <vector>

namespace {
constexpr uint32_t SCAN_SECONDS = 5;
constexpr uint8_t MAX_CONNECT_ATTEMPTS = 3;
constexpr uint32_t CONNECT_TIMEOUT_MS = 8000;
constexpr uint32_t RETRY_DELAYS_MS[MAX_CONNECT_ATTEMPTS] = {1000, 2000, 4000};
constexpr uint16_t COLOR_BG = TFT_BLACK;
constexpr uint16_t COLOR_MAIN = TFT_GREEN;
constexpr uint16_t COLOR_DIM = TFT_DARKGREEN;
constexpr uint16_t COLOR_WARN = TFT_YELLOW;
constexpr uint16_t COLOR_ERROR = TFT_RED;
constexpr uint8_t PAGE_SIZE = 6;

struct DeviceEntry {
  String address;
  String name;
  int rssi = -127;
  uint32_t lastSeen = 0;
};

std::vector<DeviceEntry> devices;
BLEScan *scanner = nullptr;
BLEClient *client = nullptr;
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

String shortAddress(const String &address) {
  if (address.length() <= 11) return address;
  return address.substring(0, 6) + ".." + address.substring(address.length() - 5);
}

void drawScanScreen() {
  drawHeader(scanning ? "BUSCA BLE ATIVA" : "DISPOSITIVOS PROXIMOS");
  auto &d = M5Cardputer.Display;
  d.setTextColor(scanning ? COLOR_WARN : COLOR_DIM, COLOR_BG);
  d.drawString(scanning ? "Escaneando anuncios..." : String(devices.size()) + " encontrados", 8, 46);
  const uint8_t begin = pageOffset;
  const uint8_t end = std::min<size_t>(devices.size(), begin + PAGE_SIZE);
  for (uint8_t i = begin; i < end; ++i) {
    const DeviceEntry &entry = devices[i];
    const bool selected = i == selectedIndex;
    d.setTextColor(selected ? COLOR_BG : COLOR_MAIN, selected ? COLOR_MAIN : COLOR_BG);
    String name = entry.name.isEmpty() ? "(sem nome anunciado)" : entry.name;
    if (name.length() > 22) name = name.substring(0, 21) + "~";
    char row[48];
    snprintf(row, sizeof(row), "%c %-22s %4d", selected ? '>' : ' ', name.c_str(), entry.rssi);
    d.drawString(row, 8, 64 + (i - begin) * 18);
    d.setTextColor(COLOR_DIM, COLOR_BG);
    d.drawString(shortAddress(entry.address), 22, 76 + (i - begin) * 18);
  }
  d.setTextColor(COLOR_WARN, COLOR_BG);
  d.drawString(statusMessage, 8, 181);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("S buscar  Enter abrir  ^v mover", 8, 202);
  d.drawString("< > pagina  Esc cancelar/voltar", 8, 216);
}

void drawDetailScreen() {
  drawHeader("DETALHE DO DISPOSITIVO");
  auto &d = M5Cardputer.Display;
  if (devices.empty() || selectedIndex >= devices.size()) return;
  const DeviceEntry &entry = devices[selectedIndex];
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  d.drawString(entry.name.isEmpty() ? "Nome: (nao anunciado)" : "Nome: " + entry.name, 8, 51);
  d.drawString("Endereco: " + entry.address, 8, 69);
  d.drawString("Sinal: " + String(entry.rssi) + " dBm", 8, 87);
  d.drawString("Visto: " + String((millis() - entry.lastSeen) / 1000) + "s atras", 8, 105);
  d.setTextColor(connected ? COLOR_MAIN : COLOR_WARN, COLOR_BG);
  d.drawString(connected ? "Estado: CONECTADO" : "Estado: DESCONECTADO", 8, 127);
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  d.drawString("> C  Conectar / testar", 8, 151);
  d.drawString("  G  Descobrir servicos GATT", 8, 169);
  d.drawString("  X  Desconectar", 8, 187);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("Enter: conectar  G: GATT  Esc: voltar", 8, 216);
}

void drawTestScreen(const String &message, uint8_t attempt) {
  drawHeader("TESTE DE CONEXAO");
  auto &d = M5Cardputer.Display;
  d.setTextColor(COLOR_MAIN, COLOR_BG);
  d.drawString("Alvo: " + shortAddress(selectedAddress), 8, 58);
  d.setTextColor(COLOR_WARN, COLOR_BG);
  d.drawString("Tentativa " + String(attempt) + "/" + String(MAX_CONNECT_ATTEMPTS), 8, 82);
  d.drawString(message, 8, 104);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.drawString("Enter/Esc: cancelar", 8, 216);
}

void drawServiceList() {
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
  auto found = std::find_if(devices.begin(), devices.end(), [&](const DeviceEntry &item) {
    return item.address == address;
  });
  if (found == devices.end()) {
    DeviceEntry entry;
    entry.address = address;
    entry.name = advertised.haveName() ? String(advertised.getName().c_str()) : "";
    entry.rssi = advertised.getRSSI();
    entry.lastSeen = millis();
    devices.push_back(entry);
    statusMessage = "Novo dispositivo detectado";
  } else {
    if (advertised.haveName()) found->name = advertised.getName().c_str();
    found->rssi = advertised.getRSSI();
    found->lastSeen = millis();
  }
}

class DeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
 public:
  void onResult(BLEAdvertisedDevice advertised) override { onDeviceFound(advertised); }
};

bool tryConnect() {
  if (selectedIndex >= devices.size()) return false;
  selectedAddress = devices[selectedIndex].address;
  BLEAddress address(selectedAddress.c_str());
  if (!client) client = BLEDevice::createClient();
  return client->connect(address);
}

bool isCancelPressed() {
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return false;
  const auto keys = M5Cardputer.Keyboard.keysState();
  return keys.enter || keys.del || keys.esc;
}

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
  devices.clear();
  selectedIndex = 0;
  pageOffset = 0;
  scanning = true;
  statusMessage = "Busca BLE em andamento";
  drawScanScreen();
  scanner->clearResults();
  scanner->start(SCAN_SECONDS, false);
  const uint32_t start = millis();
  while (millis() - start < SCAN_SECONDS * 1000UL + 2000UL) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      const auto keys = M5Cardputer.Keyboard.keysState();
      if (keys.esc || keys.del) scanner->stop();
    }
    drawScanScreen();
    delay(40);
  }
  scanner->stop();
  scanning = false;
  statusMessage = devices.empty() ? "Nenhum anuncio BLE detectado" : "Busca concluida";
  drawScanScreen();
}

void handleScanKeys(const Keyboard_Class::KeysState &keys) {
  if (keys.up && selectedIndex > 0) --selectedIndex;
  if (keys.down && selectedIndex + 1 < devices.size()) ++selectedIndex;
  if (keys.right && selectedIndex + 1 < devices.size()) selectedIndex = std::min<size_t>(devices.size() - 1, pageOffset + PAGE_SIZE);
  if (keys.left && selectedIndex > 0) selectedIndex = pageOffset > 0 ? pageOffset - 1 : 0;
  if (selectedIndex < pageOffset) pageOffset = selectedIndex;
  if (selectedIndex >= pageOffset + PAGE_SIZE) pageOffset = selectedIndex - PAGE_SIZE + 1;
  for (char key : keys.word) if (key == 's' || key == 'S') startScan();
  if (keys.enter && !devices.empty()) drawDetailScreen();
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
  BLEDevice::init("Cardputer-BLE-Tester");
  scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new DeviceCallbacks(), true);
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
      if (connected) disconnectDevice();
      drawScanScreen();
    } else if (keys.enter || keys.up || keys.down || keys.left || keys.right || !keys.word.empty()) {
      if (connected && servicesReady) handleDetailKeys(keys);
      else handleScanKeys(keys);
    }
  }
  delay(10);
}
