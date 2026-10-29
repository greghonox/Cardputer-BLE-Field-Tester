# BLE Field Tester — Cardputer-Adv

Projeto independente para o M5Stack Cardputer-Adv (ESP32-S3). Faz varredura de anúncios Bluetooth Low Energy (BLE), permite escolher um dispositivo, testar conexão com limite de tentativas e inspecionar serviços GATT. O menu `A` tenta conectar sequencialmente aos aparelhos encontrados e mantém a primeira conexão bem-sucedida.

Versão exibida no topo de todas as telas: `v1.5.0`.

A interface usa tema verde escuro, seleção verde e cabeçalho/rodapé fixos com atalhos de navegação. A tela de detalhes aproveita a área central para nome, endereço, sinal, fabricante, pistas de SO/serviços e estado da conexão.

## Requisitos

- M5Stack Cardputer-Adv.
- Docker disponível para executar o build isolado via PlatformIO.

## Teste em lote (`A`)

A ação `A` percorre a lista na ordem e apresenta o alvo atual pelo nome anunciado ou, se não houver nome, pelo endereço BLE. O endereço completo também aparece como ID, para distinguir aparelhos com nomes iguais. O lote tenta cada dispositivo uma vez e segue ao próximo quando a tentativa falha; registra o êxito e segue ao próximo alvo, encerrando a conexão após cada tentativa; não para no primeiro êxito. Ao final, mostra quantos dispositivos aceitaram conexão e seleciona o último conectado. Se todos falharem, informa quantos foram testados. `Esc`, `Enter` ou `Del` cancela durante a pausa entre alvos. O tempo limite é gerenciado pela biblioteca BLE do ESP32. Conectar pode interromper temporariamente conexões BLE existentes; use somente com dispositivos próprios ou autorizados.

## Telas e instruções de uso

Interface em orientação horizontal, com fundo verde muito escuro e texto em tons de verde. O cabeçalho e rodapé permanecem fixos; o rodapé mostra os atalhos disponíveis. Dispositivos detectados ficam em verde e a linha selecionada usa fundo verde médio. Os desenhos abaixo são esquemáticos: nomes, RSSI e endereços variam conforme o aparelho.

### 1. Lista de dispositivos

```text
┌──────────────────────────────────────┐
│ [ BLE FIELD TESTER ]          v1.5.0  │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━         │
│ DISPOSITIVOS PROXIMOS                │
│ 3 encontrados                        │
│                                      │
│ > Sensor bancada          -48         │
│ aa:bb:cc:dd:ee:ff              │
│   Fone BLE de teste       -67         │
│ 10:20:30:40:50:60                  │
│   (sem nome anunciado)     -81        │
│ 01:02:03:04:05:06                  │
│                                      │
│ Busca concluida - selecione um alvo  │
│ S buscar  A sequencial ate conectar │
└──────────────────────────────────────┘
```

**Como usar:** pressione `S` para iniciar uma busca BLE de aproximadamente cinco segundos. `Esc` ou `Del` cancela. Ao terminar, a primeira linha fica selecionada; use `↑`/`↓` para mover o destaque (a linha selecionada aparece com fundo verde médio) e `Enter` para abrir os detalhes desse aparelho. Pressione `A` para testar a conexão de todos os dispositivos encontrados em sequência. O lote tenta cada alvo uma vez, desconecta após cada êxito e continua até o fim; ao final, informa quantos aceitaram conexão e seleciona o último conectado. Cada linha mostra nome anunciado, RSSI e endereço BLE completo. Se houver mais dispositivos que linhas disponíveis, `←`/`→` leva ao item anterior/próximo bloco e o destaque acompanha a mudança. Sem resultados, confirme que o periférico está ligado, próximo e anunciando BLE; aparelhos Bluetooth clássico não aparecem. RSSI mais próximo de zero costuma indicar sinal mais forte, mas não mede distância com precisão.

**Teste em lote (`A`):** mostra o alvo atual pelo nome anunciado, ou pelo endereço quando o nome não está disponível, e exibe também o endereço completo como ID. Tenta cada dispositivo uma vez, desconecta após cada conexão bem-sucedida e continua até o fim; após uma falha, segue ao próximo alvo. `Esc`, `Enter` ou `Del` cancela durante a pausa entre alvos. Ao final, informa quantos foram testados e quantos aceitaram conexão, selecionando o último conectado. O timeout é gerenciado pela biblioteca BLE do ESP32. O teste pode interromper temporariamente conexões BLE existentes; use apenas com dispositivos próprios ou autorizados. O firmware não pareia, lê ou altera características.

### 2. Detalhes do dispositivo

```text
┌──────────────────────────────────────┐
│ DETALHE DO DISPOSITIVO               │
│ Nome: Sensor ou (nao anunciado)      │
│ Endereco: aa:bb:cc:dd:ee:ff          │
│ Sinal: -48 dBm                       │
│ Visto: 1s atras                      │
│ Fab: Apple / Samsung / ID            │
│ SO: possivel; nao confirmado         │
│ Svc: UUID BLE anunciado              │
│                                    │
│                                      │
│ Estado: DESCONECTADO                 │
│ C conectar  G servicos  X desconectar│
└──────────────────────────────────────┘
```

Na tela de detalhes, além do nome/endereço/RSSI, mostramos fabricante inferido pelo Company ID no anúncio, alguns UUIDs de serviços anunciados e uma pista heurística de SO. O SO é apenas uma hipótese; BLE não permite identificar com certeza o sistema operacional e muitos anúncios não carregam essas informações. `G` continua descobrindo serviços GATT apenas após conectar. O cabeçalho verde escuro, título e rodapé de atalhos permanecem fixos; os dados do aparelho ocupam a área central.


```text
┌──────────────────────────────────────┐
│ [ BLE FIELD TESTER ]          v1.5.0  │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━         │
│ DETALHE DO DISPOSITIVO               │
│ Nome: Sensor bancada                 │
│ Endereco: aa:bb:cc:dd:12:34          │
│ Sinal: -48 dBm                       │
│ Visto: 1s atras                      │
│                                    │
│ Estado: DESCONECTADO                 │
│                                      │
│ > C  Conectar / testar               │
│   G  Descobrir servicos GATT         │
│   X  Desconectar                     │
│                                      │
│ Enter: conectar  G: GATT  Esc: voltar│
└──────────────────────────────────────┘
```

**Como usar:** revise se selecionou o aparelho correto. `Enter` ou `C` inicia o teste de conexão; `G` tenta descobrir serviços GATT (requer conexão); `X` desconecta. `Esc` retorna à lista. Conectar pode interromper a conexão BLE de outro cliente. O programa não pareia, não lê valores e não escreve características. O rodapé fixo apresenta as ações disponíveis; o conteúdo da tela muda para detalhes, teste e lista de serviços.

### 3. Teste de conexão individual

```text
┌──────────────────────────────────────┐
│ [ BLE FIELD TESTER ]          v1.5.0  │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━         │
│ TESTE EM LOTE                         │
│ Alvo 1/3                              │
│ Sensor bancada                        │
│ ID: aa:bb:cc:dd:ee:ff                 │
│ Testando alvo...                      │
│                                       │
│ ESC cancela lote                      │
└──────────────────────────────────────┘
```

**Como usar:** aguarde o resultado. O teste faz até três tentativas sem espera adicional; o timeout é gerenciado pela biblioteca BLE do ESP32 e pode variar conforme o periférico. Se conectar, retorna aos detalhes com estado `CONECTADO`; se falhar, informa o resultado e retorna aos detalhes.

### 4. Resultado do teste em lote

Se todos os dispositivos falharem, a tela informa o total testado. Quando algum aceita a conexão, o aparelho correspondente é selecionado e seus detalhes ficam visíveis com o estado conectado.

### 5. Serviços GATT

```text
┌──────────────────────────────────────┐
│ [ BLE FIELD TESTER ]          v1.5.0  │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━         │
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
- `A`: testar dispositivos em sequência; para no primeiro êxito e mantém a conexão ativa.
- Setas: selecionar dispositivo; `Enter`: abrir detalhes / iniciar conexão.
- Na tela de detalhes: `C` conectar; `G` descobrir serviços GATT; `X` desconectar.
- `Esc`: voltar/cancelar quando disponível.

O suporte é a BLE, não Bluetooth clássico. Nem todo dispositivo anuncia nome. RSSI é apenas uma indicação aproximada de sinal. Use somente com dispositivos próprios ou autorizados; a conexão pode interromper temporariamente a ligação de outro cliente. O firmware não tenta parear nem contornar controles de acesso.

## Compilar

```bash
./build-docker.sh cardputer-ble-field-tester
```

O firmware é gerado em `.pio/build/cardputer-ble-field-tester/firmware.bin`. A compilação não grava o dispositivo automaticamente. O firmware ainda precisa ser testado no hardware.
