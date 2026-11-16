
FROM debian:bookworm-slim

WORKDIR /app

RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN gcc -O2 -Wall -Wextra -pthread \
    -o roomsync_server \
    server_linux.c \
    database_linux.c \
    sqlite3.c \
    -lssl -lcrypto

RUN mkdir -p /var/data

ENV ROOMSYNC_DB_PATH=/var/data/roomsync.db

EXPOSE 8080

CMD ["./roomsync_server"]
