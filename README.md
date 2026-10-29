# BLE Field Tester — Cardputer-Adv

Projeto independente para o M5Stack Cardputer-Adv (ESP32-S3). Faz varredura de anúncios Bluetooth Low Energy (BLE), permite escolher um dispositivo, testar conexão com limite de tentativas e inspecionar serviços GATT. Não tenta conexão automática com todos os aparelhos e não escreve características.

## Requisitos

- M5Stack Cardputer-Adv.
- Docker disponível para executar o build isolado via PlatformIO.

## Telas

A interface usa fundo preto e acentos verdes no estilo Matrix. Os dispositivos detectados aparecem em **vermelho**; a seleção atual fica destacada com fundo vermelho. A busca BLE dura cerca de cinco segundos.

### 1. Lista de dispositivos

Mostra o estado da busca, quantos dispositivos foram encontrados e, para cada anúncio BLE, o nome (ou “sem nome anunciado”), a intensidade do sinal em dBm e o endereço parcialmente oculto. Use as setas para navegar, `Enter` para abrir os detalhes e `S` para buscar novamente. Se houver mais resultados do que cabem na tela, a lista é paginada conforme a tela do aparelho.

Se nenhum dispositivo aparecer, a tela informa que nada foi anunciado. Confirme que o periférico está ligado, próximo e com o BLE/anúncio ativo; um aparelho conectado a outro dispositivo pode deixar de anunciar.

### 2. Detalhes do dispositivo

Exibe nome, endereço completo, RSSI, tempo desde a última detecção e estado da conexão. Ações disponíveis:

- `Enter` ou `C`: iniciar o teste de conexão;
- `G`: descobrir serviços GATT e mostrar quantas características há em cada serviço;
- `X`: desconectar.

A inspeção é somente de descoberta: não lê nem escreve valores de características, nem envia comandos ao periférico.

### 3. Teste de conexão

Mostra o endereço do dispositivo selecionado, o número da tentativa e o estado atual. São feitas até três tentativas, com esperas progressivas entre falhas. `Enter` ou `Esc` cancela durante a espera; uma conexão pode levar alguns segundos, conforme o periférico.

### 4. Serviços GATT

Após conectar, mostra até sete serviços anunciados pelo periférico e a quantidade de características em cada serviço. Se nenhum serviço for encontrado, a tela informa isso. Pressione `Esc` para retornar.

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
