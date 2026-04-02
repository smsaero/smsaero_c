#include "smsaero.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <stdint.h>

#define MAX_RESPONSE_SIZE (10 * 1024 * 1024)
#define DEFAULT_TIMEOUT 30L
#define DEFAULT_CONNECT_TIMEOUT 10L

static int curl_ref_count = 0;

typedef struct {
    char *data;
    size_t len;
} ResponseBuffer;

static void secure_free(char *ptr) {
    if (ptr) {
        memset(ptr, 0, strlen(ptr));
        free(ptr);
    }
}

SmsAeroError *create_error(const char *message) {
    SmsAeroError *error = malloc(sizeof(SmsAeroError));
    if (error == NULL) return NULL;
    error->message = strdup(message ? message : "unknown error");
    if (error->message == NULL) {
        free(error);
        return NULL;
    }
    return error;
}

void free_error(SmsAeroError *error) {
    if (error) {
        free(error->message);
        free(error);
    }
}

SmsAero *init_sms_aero(const char *email, const char *api_key, const char *signature) {
    if (!email || !api_key) return NULL;

    SmsAero *sms_aero = malloc(sizeof(SmsAero));
    if (sms_aero == NULL) return NULL;

    sms_aero->email = strdup(email);
    if (sms_aero->email == NULL) {
        free(sms_aero);
        return NULL;
    }

    sms_aero->api_key = strdup(api_key);
    if (sms_aero->api_key == NULL) {
        free(sms_aero->email);
        free(sms_aero);
        return NULL;
    }

    sms_aero->signature = strdup(signature ? signature : "Sms Aero");
    if (sms_aero->signature == NULL) {
        free(sms_aero->api_key);
        free(sms_aero->email);
        free(sms_aero);
        return NULL;
    }

    if (curl_ref_count == 0) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    curl_ref_count++;

    return sms_aero;
}

void cleanup_sms_aero(SmsAero *sms_aero) {
    if (sms_aero) {
        secure_free(sms_aero->email);
        secure_free(sms_aero->api_key);
        secure_free(sms_aero->signature);
        free(sms_aero);

        curl_ref_count--;
        if (curl_ref_count <= 0) {
            curl_global_cleanup();
            curl_ref_count = 0;
        }
    }
}

static size_t write_callback(const void *contents, const size_t size, const size_t nmemb, void *userp) {
    if (nmemb > 0 && size > SIZE_MAX / nmemb) return 0;
    const size_t chunk = size * nmemb;

    ResponseBuffer *buf = (ResponseBuffer *)userp;

    if (buf->len + chunk + 1 > MAX_RESPONSE_SIZE) return 0;

    char *ptr = realloc(buf->data, buf->len + chunk + 1);
    if (ptr == NULL) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    buf->data = ptr;
    memcpy(buf->data + buf->len, contents, chunk);
    buf->len += chunk;
    buf->data[buf->len] = '\0';

    return chunk;
}

cJSON *request(const SmsAero *sms_aero, const char *selector, const cJSON *data, const int page) {
    if (!sms_aero || !selector) return NULL;

    ResponseBuffer buf = {0};
    buf.data = calloc(1, 1);
    if (buf.data == NULL) return NULL;

    cJSON *json_response = NULL;
    CURL *curl = curl_easy_init();
    if (!curl) {
        free(buf.data);
        return NULL;
    }

    size_t url_cap = strlen("https://gate.smsaero.ru/v2/") + strlen(selector) + 64;
    char *url = malloc(url_cap);
    if (!url) {
        free(buf.data);
        curl_easy_cleanup(curl);
        return NULL;
    }
    snprintf(url, url_cap, "https://gate.smsaero.ru/v2/%s", selector);

    if (page >= 0) {
        char page_query[32];
        snprintf(page_query, sizeof(page_query), "?page=%d", page);
        strncat(url, page_query, url_cap - strlen(url) - 1);
    }

    size_t auth_cap = strlen(sms_aero->email) + 1 + strlen(sms_aero->api_key) + 1;
    char *auth = malloc(auth_cap);
    if (!auth) {
        free(url);
        free(buf.data);
        curl_easy_cleanup(curl);
        return NULL;
    }
    snprintf(auth, auth_cap, "%s:%s", sms_aero->email, sms_aero->api_key);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: SACClient/1.1");

    char *data_string = NULL;
    if (data != NULL) {
        data_string = cJSON_PrintUnformatted(data);
        if (data_string) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_string);
        }
    }

    curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, DEFAULT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, DEFAULT_CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    const CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
        json_response = cJSON_Parse(buf.data);
        if (json_response == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
            }
        }
    }

    curl_slist_free_all(headers);
    free(data_string);
    free(buf.data);
    free(url);
    secure_free(auth);
    curl_easy_cleanup(curl);

    return json_response;
}

static void fill_nums(cJSON *data, const char *number) {
    if (!data || !number) return;
    if (strchr(number, ',')) {
        cJSON_AddStringToObject(data, "numbers", number);
    } else {
        cJSON_AddStringToObject(data, "number", number);
    }
}

static void set_error(SmsAeroError **error, const char *message) {
    if (error) {
        *error = create_error(message);
    }
}

cJSON *send_sms(const SmsAero *sms_aero, const char *number, const char *text, const char *date_send,
                const char *callback_url, SmsAeroError **error) {
    if (!sms_aero || !number || !text) {
        set_error(error, "params `number` and `text` are required");
        return NULL;
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;

    fill_nums(data, number);
    cJSON_AddStringToObject(data, "sign", sms_aero->signature);
    cJSON_AddStringToObject(data, "text", text);
    if (callback_url) {
        cJSON_AddStringToObject(data, "callbackUrl", callback_url);
    }

    if (date_send && strlen(date_send) > 0) {
        struct tm tm = {0};
        tm.tm_isdst = -1;
        if (!strptime(date_send, "%Y-%m-%d %H:%M:%S", &tm)) {
            set_error(error, "param `date` is not in correct format");
            cJSON_Delete(data);
            return NULL;
        }
        time_t epoch = mktime(&tm);
        cJSON_AddNumberToObject(data, "dateSend", (double)epoch);
    }

    cJSON *response = request(sms_aero, "sms/send", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *sms_status(const SmsAero *sms_aero, const int sms_id) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddNumberToObject(data, "id", sms_id);
    cJSON *response = request(sms_aero, "sms/status", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *sms_list(const SmsAero *sms_aero, const char *number, const char *text, const int page) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    if (number) {
        fill_nums(data, number);
    }
    if (text) {
        cJSON_AddStringToObject(data, "text", text);
    }
    cJSON *response = request(sms_aero, "sms/list", data, page);
    cJSON_Delete(data);
    return response;
}

cJSON *balance(const SmsAero *sms_aero) {
    return request(sms_aero, "balance", NULL, -1);
}

cJSON *balance_add(const SmsAero *sms_aero, const float amount, const int card_id) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddNumberToObject(data, "sum", (double)amount);
    cJSON_AddNumberToObject(data, "cardId", card_id);
    cJSON *response = request(sms_aero, "balance/add", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *cards(const SmsAero *sms_aero) {
    return request(sms_aero, "cards", NULL, -1);
}

cJSON *tariffs(const SmsAero *sms_aero) {
    return request(sms_aero, "tariffs", NULL, -1);
}

cJSON *sign_list(const SmsAero *sms_aero, const int page) {
    return request(sms_aero, "sign/list", NULL, page);
}

cJSON *group_add(const SmsAero *sms_aero, const char *name) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddStringToObject(data, "name", name);
    cJSON *response = request(sms_aero, "group/add", data, -1);
    cJSON_Delete(data);
    return response;
}

static bool extract_success(cJSON *response) {
    if (!response) return false;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(response, "success");
    bool result = (item != NULL && cJSON_IsBool(item) && cJSON_IsTrue(item));
    cJSON_Delete(response);
    return result;
}

bool group_delete(const SmsAero *sms_aero, const int group_id) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return false;
    cJSON_AddNumberToObject(data, "id", group_id);
    cJSON *response = request(sms_aero, "group/delete", data, -1);
    cJSON_Delete(data);
    return extract_success(response);
}

bool group_delete_all(const SmsAero *sms_aero) {
    cJSON *response = request(sms_aero, "group/delete-all", NULL, -1);
    return extract_success(response);
}

cJSON *group_list(const SmsAero *sms_aero, const int page) {
    return request(sms_aero, "group/list", NULL, page);
}

cJSON *contact_add(const SmsAero *sms_aero, const char *number, const int group_id, const char *birthday, const char *sex,
                   const char *last_name, const char *first_name, const char *surname, const char *param1,
                   const char *param2, const char *param3) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddStringToObject(data, "number", number);
    if (group_id != -1) {
        cJSON_AddNumberToObject(data, "groupId", group_id);
    }
    if (birthday) cJSON_AddStringToObject(data, "birthday", birthday);
    if (sex) cJSON_AddStringToObject(data, "sex", sex);
    if (last_name) cJSON_AddStringToObject(data, "lname", last_name);
    if (first_name) cJSON_AddStringToObject(data, "fname", first_name);
    if (surname) cJSON_AddStringToObject(data, "sname", surname);
    if (param1) cJSON_AddStringToObject(data, "param1", param1);
    if (param2) cJSON_AddStringToObject(data, "param2", param2);
    if (param3) cJSON_AddStringToObject(data, "param3", param3);
    cJSON *response = request(sms_aero, "contact/add", data, -1);
    cJSON_Delete(data);
    return response;
}

bool contact_delete(const SmsAero *sms_aero, const int contact_id) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return false;
    cJSON_AddNumberToObject(data, "id", contact_id);
    cJSON *response = request(sms_aero, "contact/delete", data, -1);
    cJSON_Delete(data);
    return extract_success(response);
}

bool contact_delete_all(const SmsAero *sms_aero) {
    cJSON *response = request(sms_aero, "contact/delete-all", NULL, -1);
    return extract_success(response);
}

cJSON *contact_list(const SmsAero *sms_aero, const int page) {
    return request(sms_aero, "contact/list", NULL, page);
}

cJSON *blacklist_add(const SmsAero *sms_aero, const char *numbers) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddStringToObject(data, "numbers", numbers);
    cJSON *response = request(sms_aero, "blacklist/add", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *blacklist_list(const SmsAero *sms_aero, const char *numbers, const int page) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    if (numbers != NULL) {
        cJSON_AddStringToObject(data, "numbers", numbers);
    }
    cJSON *response = request(sms_aero, "blacklist/list", data, page);
    cJSON_Delete(data);
    return response;
}

bool blacklist_delete(const SmsAero *sms_aero, const int blacklist_id) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return false;
    cJSON_AddNumberToObject(data, "id", blacklist_id);
    cJSON *response = request(sms_aero, "blacklist/delete", data, -1);
    cJSON_Delete(data);
    return extract_success(response);
}

cJSON *hlr_check(const SmsAero *sms_aero, const char *numbers) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddStringToObject(data, "numbers", numbers);
    cJSON *response = request(sms_aero, "hlr/check", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *hlr_status(const SmsAero *sms_aero, const int hlr_id) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddNumberToObject(data, "id", hlr_id);
    cJSON *response = request(sms_aero, "hlr/status", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *number_operator(const SmsAero *sms_aero, const char *numbers) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddStringToObject(data, "numbers", numbers);
    cJSON *response = request(sms_aero, "number/operator", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *viber_send(const SmsAero *sms_aero, const char *sign, const char *channel, const char *text, const char *number,
                  const int group_id, const char *image_source, const char *text_button, const char *link_button,
                  const char *date_send, const char *sign_sms, const char *channel_sms, const char *text_sms,
                  const int price_sms) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    if (group_id != -1) cJSON_AddNumberToObject(data, "groupId", group_id);
    if (sign) cJSON_AddStringToObject(data, "sign", sign);
    if (channel) cJSON_AddStringToObject(data, "channel", channel);
    if (text) cJSON_AddStringToObject(data, "text", text);
    if (image_source) cJSON_AddStringToObject(data, "imageSource", image_source);
    if (text_button) cJSON_AddStringToObject(data, "textButton", text_button);
    if (link_button) cJSON_AddStringToObject(data, "linkButton", link_button);
    if (date_send) cJSON_AddStringToObject(data, "dateSend", date_send);
    if (sign_sms) cJSON_AddStringToObject(data, "signSms", sign_sms);
    if (channel_sms) cJSON_AddStringToObject(data, "channelSms", channel_sms);
    if (text_sms) cJSON_AddStringToObject(data, "textSms", text_sms);
    if (price_sms != -1) cJSON_AddNumberToObject(data, "priceSms", price_sms);
    if (number) fill_nums(data, number);

    cJSON *response = request(sms_aero, "viber/send", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *viber_sign_list(const SmsAero *sms_aero) {
    return request(sms_aero, "viber/sign/list", NULL, -1);
}

cJSON *viber_list(const SmsAero *sms_aero, const int page) {
    return request(sms_aero, "viber/list", NULL, page);
}

cJSON *viber_statistics(const SmsAero *sms_aero, const int sending_id, const int page) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddNumberToObject(data, "sendingId", sending_id);
    cJSON *response = request(sms_aero, "viber/statistic", data, page);
    cJSON_Delete(data);
    return response;
}

cJSON *send_telegram(const SmsAero *sms_aero, const char *number, const int code, const char *sign, const char *text, SmsAeroError **error) {
    if (!sms_aero || !number) {
        set_error(error, "params `number` is required");
        return NULL;
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;

    fill_nums(data, number);
    cJSON_AddNumberToObject(data, "code", code);

    if (sign) {
        cJSON_AddStringToObject(data, "sign", sign);
    }
    if (text) {
        cJSON_AddStringToObject(data, "text", text);
    }

    cJSON *response = request(sms_aero, "telegram/send", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *telegram_status(const SmsAero *sms_aero, const int telegram_id) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddNumberToObject(data, "id", telegram_id);
    cJSON *response = request(sms_aero, "telegram/status", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *send_mobile_id(const SmsAero *sms_aero, const char *number, const char *sign, const char *callback_url, SmsAeroError **error) {
    if (!number || !sign || !callback_url) {
        set_error(error, "params `number`, `sign` and `callback_url` are required");
        return NULL;
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;

    cJSON_AddStringToObject(data, "number", number);
    cJSON_AddStringToObject(data, "sign", sign);
    cJSON_AddStringToObject(data, "callbackUrl", callback_url);

    cJSON *response = request(sms_aero, "mobile-id/send", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *mobile_id_status(const SmsAero *sms_aero, const int req_id, SmsAeroError **error) {
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        set_error(error, "failed to create request data");
        return NULL;
    }
    cJSON_AddNumberToObject(data, "id", req_id);
    cJSON *response = request(sms_aero, "mobile-id/status", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *verify_mobile_id(const SmsAero *sms_aero, const int req_id, const char *code, const char *sign, SmsAeroError **error) {
    if (!code || !sign) {
        set_error(error, "params `code` and `sign` are required");
        return NULL;
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;

    cJSON_AddNumberToObject(data, "id", req_id);
    cJSON_AddStringToObject(data, "code", code);
    cJSON_AddStringToObject(data, "sign", sign);

    cJSON *response = request(sms_aero, "mobile-id/verify", data, -1);
    cJSON_Delete(data);
    return response;
}
