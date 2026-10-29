# BLE Field Tester — Cardputer-Adv

Projeto independente para o M5Stack Cardputer-Adv (ESP32-S3). Faz varredura de anúncios Bluetooth Low Energy (BLE), permite escolher um dispositivo, testar conexão com limite de tentativas e inspecionar serviços GATT. Não tenta conexão automática com todos os aparelhos e não escreve características.

## Requisitos

- M5Stack Cardputer-Adv.
- Docker disponível para executar o build isolado via PlatformIO.

## Telas e instruções de uso

Interface em orientação horizontal, com fundo preto e tema Matrix. O texto de informação/menus usa verde; dispositivos detectados ficam **vermelhos**; a linha selecionada fica com fundo vermelho. Os desenhos abaixo são esquemáticos: nomes, RSSI e endereços variam conforme o aparelho.

### 1. Lista de dispositivos

```text
┌──────────────────────────────────────┐
│ [ BLE FIELD TESTER ]                 │
│ ------------------------------       │
│ DISPOSITIVOS PROXIMOS                │
│ 3 encontrados                        │
│                                      │
│ > Sensor bancada          -48         │
│   AA:BB:..:12:34                     │
│   Fone BLE de teste       -67         │
│   10:20:..:30:40                     │
│   (sem nome anunciado)     -81        │
│   01:02:..:03:04                     │
│                                      │
│ Busca concluida - selecione um alvo  │
│ S buscar  Enter abrir  ^v mover      │
└──────────────────────────────────────┘
```

**Como usar:** pressione `S` para iniciar uma busca BLE de aproximadamente cinco segundos. `Esc` ou `Del` cancela. Ao terminar, a primeira linha fica selecionada; use `↑`/`↓` para mover o destaque (a linha selecionada aparece com fundo vermelho) e `Enter` para abrir os detalhes desse aparelho. Cada linha mostra nome anunciado, RSSI e endereço parcialmente oculto. Se houver mais dispositivos que linhas disponíveis, `←`/`→` leva ao item anterior/próximo bloco e o destaque acompanha a mudança. Sem resultados, confirme que o periférico está ligado, próximo e anunciando BLE; aparelhos Bluetooth clássico não aparecem. RSSI mais próximo de zero costuma indicar sinal mais forte, mas não mede distância com precisão.

### 2. Detalhes do dispositivo

```text
┌──────────────────────────────────────┐
│ [ BLE FIELD TESTER ]                 │
│ ------------------------------       │
│ DETALHE DO DISPOSITIVO               │
│ Nome: Sensor bancada                 │
│ Endereco: aa:bb:cc:dd:12:34          │
│ Sinal: -48 dBm                       │
│ Visto: 1s atras                      │
│ Estado: DESCONECTADO                 │
│                                      │
│ > C  Conectar / testar               │
│   G  Descobrir servicos GATT         │
│   X  Desconectar                     │
│                                      │
│ Enter: conectar  G: GATT  Esc: voltar│
└──────────────────────────────────────┘
```

**Como usar:** revise se selecionou o aparelho correto. `Enter` ou `C` inicia o teste de conexão; `G` tenta descobrir serviços GATT (requer conexão); `X` desconecta. `Esc` retorna à lista. Conectar pode interromper a conexão BLE de outro cliente. O programa não pareia, não lê valores e não escreve características.

### 3. Teste de conexão

```text
┌──────────────────────────────────────┐
│ [ BLE FIELD TESTER ]                 │
│ ------------------------------       │
│ TESTE DE CONEXAO                     │
│ Alvo: aa:bb..:12:34                  │
│                                      │
│ Tentativa 2/3                        │
│ Falhou; nova tentativa em 2s         │
│                                      │
│ Enter/Esc: cancelar                  │
└──────────────────────────────────────┘
```

**Como usar:** aguarde o resultado ou cancele durante o intervalo entre tentativas com `Enter` ou `Esc`. O teste faz no máximo três tentativas, com esperas crescentes. Se conectar, retorna à tela de detalhes com estado `CONECTADO`; em caso de falha, mostra o resultado e retorna aos detalhes. O tempo efetivo depende do periférico.

### 4. Serviços GATT

```text
┌──────────────────────────────────────┐
│ [ BLE FIELD TESTER ]                 │
│ ------------------------------       │
│ SERVICOS GATT                        │
│ 1. 00001800-0000-1000-8000-...       │
│    Caracteristicas: 4                │
│ 2. 00001801-0000-1000-8000-...       │
│    Caracteristicas: 1                │
│                                      │
│ Somente leitura/descoberta; sem ...  │
└──────────────────────────────────────┘
```

**Como usar:** na tela de detalhes, pressione `G` para descobrir os serviços. A tela pode mostrar até sete serviços e a quantidade de características de cada um; não lê nem altera valores. Se o aparelho não estiver conectado, ou não expuser serviços, a tela informa a condição. `Esc` volta aos detalhes e `Del` retorna à lista.

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
