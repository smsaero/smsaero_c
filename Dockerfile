FROM debian:bullseye-slim

WORKDIR /app

RUN apt update && \
    apt install -y make g++ libcurl4-openssl-dev libcjson-dev libgtest-dev && \
    apt clean && \
    rm /var/lib/apt/lists/* -rf

COPY . .

RUN make && \
    cp ./bin/demo /usr/local/bin/smsaero_send
