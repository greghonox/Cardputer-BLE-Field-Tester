FROM python:3.11-slim

RUN apt-get update \
    && apt-get install --yes --no-install-recommends git \
    && rm -rf /var/lib/apt/lists/* \
    && pip install --no-cache-dir platformio

WORKDIR /workspace
ENTRYPOINT ["pio"]
CMD ["run"]
