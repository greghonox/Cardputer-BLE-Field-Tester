# BLE Field Tester — Cardputer-Adv

Projeto independente para o M5Stack Cardputer-Adv (ESP32-S3). Faz varredura de anúncios Bluetooth Low Energy (BLE), permite escolher um dispositivo, testar conexão com limite de tentativas e inspecionar serviços GATT. Não tenta conexão automática com todos os aparelhos e não escreve características.

## Requisitos

- M5Stack Cardputer-Adv.
- Docker disponível para executar o build isolado via PlatformIO.

## Navegação

- `S`: iniciar varredura BLE de 5 segundos.
- Setas: selecionar dispositivo; `Enter`: abrir detalhes / iniciar conexão.
- Na tela de detalhes: `C` conectar; `G` descobrir serviços GATT; `X` desconectar.
- `Esc`: voltar/cancelar quando disponível.

O suporte é a BLE, não Bluetooth clássico. Nem todo dispositivo anuncia nome. RSSI é apenas uma indicação aproximada de sinal. Use somente com dispositivos próprios ou autorizados; a conexão pode interromper temporariamente a ligação de outro cliente. O firmware não tenta parear nem contornar controles de acesso.

## Compilar

```bash
./build-docker.sh cardputer-ble-field-tester
```

O firmware é gerado em `.pio/build/cardputer-ble-field-tester/firmware.bin`. A compilação não grava o dispositivo automaticamente. O firmware ainda precisa ser testado no hardware.
