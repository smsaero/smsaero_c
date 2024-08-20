# SmsAero C Api client

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Library for sending SMS messages using the SmsAero API. Written in C.

## Usage example:

Get credentials from account settings page: https://smsaero.ru/cabinet/settings/apikey/

```c
#include <stdio.h>
#include <stdlib.h>
#include "smsaero.h"


#define SMSAERO_EMAIL "your email"
#define SMSAERO_API_KEY "your api key"


int main() {
    SmsAero *sms_aero = init_sms_aero(SMSAERO_EMAIL, SMSAERO_API_KEY, NULL);
    if (sms_aero == NULL) {
        fprintf(stderr, "Failed to initialize SmsAero\n");
        return EXIT_FAILURE;
    }

    SmsAeroError *error = NULL;
    cJSON *result = send_sms(sms_aero, "70000000000", "Hello, world!", NULL, NULL, &error);
    if (error) {
        fprintf(stderr, "SmsAero error: %s\n", error->message);
        free_error(error);
    } else {
        char *result_str = cJSON_Print(result);
        printf("%s\n", result_str);
        free(result_str);
        cJSON_Delete(result);
    }

    cleanup_sms_aero(sms_aero);
}
```

## Install required libraries:

* libcurl
* cJSON

```bash
sudo apt-get install -y libcurl4-openssl-dev libcjson-dev libgtest-dev
```


## Build example:

```bash
git clone https://github.com/smsaero/smsaero_c.git
cd smsaero_c
make
```

## Run example:

```bash
SMSAERO_EMAIL="your email"
SMSAERO_API_KEY="your api key"

./bin/demo -u "$SMSAERO_EMAIL" -t "$SMSAERO_API_KEY" -n 70000000000 -m 'Hello, World!' | jq .
```

## Run on Docker:

```bash
docker pull 'smsaero/smsaero_c:latest'
docker run -it --rm 'smsaero/smsaero_c:latest' smsaero_send -e "your email" -t "your api key" -n 79038805678 -m 'Hello, World!'
```

## License

```
MIT License
```
