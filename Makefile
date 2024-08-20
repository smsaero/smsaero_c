CXX = cc
CFLAGS = -Wall
INCLUDE = -Iinclude -I/usr/local/include -I/usr/include -I/opt/homebrew/include
LDFLAGS = -L/usr/local/lib -L/opt/homebrew/lib -I/usr/lib
LDLIBS = -lcurl -lcjson

SOURCES = src/demo.c src/smsaero.c
OUT = bin/demo

all: clean build

build: $(SOURCES)
	@$(CXX) -o $(OUT) $(INCLUDE) $(LDFLAGS) $(CFLAGS) $(SOURCES) $(LDLIBS)

clean:
	@rm -rf ./bin/demo

docker-build-and-push:
	@docker buildx create --name smsaero_c --use || docker buildx use smsaero_c
	@docker buildx build --platform linux/amd64,linux/arm64 -t 'smsaero/smsaero_c:latest' . -f Dockerfile --push
	@docker buildx rm smsaero_c
