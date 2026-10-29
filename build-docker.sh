#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="m5stack-cardputer-platformio:local"

if ! command -v docker >/dev/null 2>&1; then
  printf '%s\n' 'Erro: Docker CLI não está instalado.' >&2
  exit 127
fi

if ! docker info >/dev/null 2>&1; then
  printf '%s\n' 'Erro: não foi possível acessar o daemon Docker.' >&2
  printf '%s\n' 'Inicie/autorize o Docker no seu ambiente e execute este script novamente.' >&2
  printf '%s\n' 'O script não altera permissões nem configura o daemon.' >&2
  exit 1
fi

printf 'Imagem: %s\nProjeto montado em: %s\n' "$IMAGE" "$PROJECT_DIR"
printf '%s\n' 'Iniciando compilação PlatformIO (a primeira execução baixa dependências)...'

docker build --tag "$IMAGE" --file "$PROJECT_DIR/Dockerfile" "$PROJECT_DIR"

docker run --rm \
  --user "$(id -u):$(id -g)" \
  --env HOME=/tmp \
  --env PLATFORMIO_CORE_DIR=/workspace/.pio-home \
  --volume "$PROJECT_DIR:/workspace" \
  --workdir /workspace \
  "$IMAGE" \
  run -e "${1:-cardputer-adv}"

FIRMWARE="$PROJECT_DIR/.pio/build/${1:-cardputer-adv}/firmware.bin"
if [[ ! -s "$FIRMWARE" ]]; then
  printf 'Erro: compilação terminou, mas não encontrei um firmware válido em %s\n' "$FIRMWARE" >&2
  exit 1
fi

printf '\nCompilação concluída. Firmware: %s\n' "$FIRMWARE"
