#include "smsaero.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>


SmsAeroError *create_error(const char *message) {
    SmsAeroError *error = malloc(sizeof(SmsAeroError));
    if (error != NULL) {
        error->message = strdup(message);
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
    SmsAero *sms_aero = malloc(sizeof(SmsAero));
    if (sms_aero != NULL) {
        sms_aero->email = strdup(email);
        sms_aero->api_key = strdup(api_key);
        sms_aero->signature = strdup(signature ? signature : "Sms Aero");
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    return sms_aero;
}

void cleanup_sms_aero(SmsAero *sms_aero) {
    if (sms_aero) {
        free(sms_aero->email);
        free(sms_aero->api_key);
        free(sms_aero->signature);
        free(sms_aero);
        curl_global_cleanup();
    }
}

static size_t WriteCallback(const void *contents, const size_t size, const size_t nmemb, void *userp) {
    const size_t realSize = size * nmemb;
    char **response = (char **) userp;

    char *ptr = realloc(*response, strlen(*response) + realSize + 1);
    if (ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    *response = ptr;

    memcpy(&((*response)[strlen(*response)]), contents, realSize);
    (*response)[strlen(*response) + realSize] = '\0';

    return realSize;
}

cJSON *request(const SmsAero *sms_aero, const char *selector, const cJSON *data, const int page) {
    char *response = calloc(1, sizeof(char));
    if (response == NULL) return NULL;

    cJSON *jsonResponse = NULL;
    CURL *curl = curl_easy_init();

    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "https://gate.smsaero.ru/v2/%s", selector);

        if (page >= 0) {
            char pageQuery[64];
            snprintf(pageQuery, sizeof(pageQuery), "?page=%d", page);
            strncat(url, pageQuery, sizeof(url) - strlen(url) - 1);
        }


        char auth[256];
        snprintf(auth, sizeof(auth), "%s:%s", sms_aero->email, sms_aero->api_key);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "User-Agent: SACClient/1.0");

        char *dataString = cJSON_PrintUnformatted(data);
        if (data != NULL) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, dataString);
        }

        curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            jsonResponse = cJSON_Parse(response);
        }

        curl_slist_free_all(headers);
        free(dataString);
        free(response);
        curl_easy_cleanup(curl);
    }

    return jsonResponse;
}

void fill_nums(cJSON *data, const char *number) {
    if (strchr(number, ',')) {
        cJSON_AddStringToObject(data, "numbers", number);
    } else {
        cJSON_AddStringToObject(data, "number", number);
    }
}

cJSON *send_sms(const SmsAero *sms_aero, const char *number, const char *text, const char *date_send,
                const char *callback_url, SmsAeroError **error) {
    cJSON *data = cJSON_CreateObject();
    fill_nums(data, number);
    cJSON_AddStringToObject(data, "sign", sms_aero->signature);
    cJSON_AddStringToObject(data, "text", text);
    if (callback_url) {
        cJSON_AddStringToObject(data, "callbackUrl", callback_url);
    }

    if (date_send && strlen(date_send) > 0) {
        struct tm tm = {0};
        if (!strptime(date_send, "%Y-%m-%d %H:%M:%S", &tm)) {
            *error = create_error("param `date` is not in correct format");
            cJSON_Delete(data);
            return NULL;
        }
        time_t time = mktime(&tm);
        cJSON_AddNumberToObject(data, "dateSend", (int) time);
    }

    cJSON *response = request(sms_aero, "sms/send", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *sms_status(const SmsAero *sms_aero, const int sms_id) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", sms_id);
    cJSON *response = request(sms_aero, "sms/status", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *sms_list(const SmsAero *sms_aero, const char *number, const char *text, const int page) {
    cJSON *data = cJSON_CreateObject();
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
    cJSON_AddNumberToObject(data, "sum", amount);
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
    cJSON_AddStringToObject(data, "name", name);
    cJSON *response = request(sms_aero, "group/add", data, -1);
    cJSON_Delete(data);
    return response;
}

bool group_delete(const SmsAero *sms_aero, const int group_id) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", group_id);
    cJSON *response = request(sms_aero, "group/delete", data, -1);
    cJSON_Delete(data);
    cJSON *successItem = cJSON_GetObjectItemCaseSensitive(response, "success");
    if (successItem != NULL && cJSON_IsBool(successItem)) {
        return cJSON_IsTrue(successItem);
    }
    return false;
}

bool group_delete_all(const SmsAero *sms_aero) {
    cJSON *response = request(sms_aero, "group/delete-all", NULL, -1);
    cJSON *successItem = cJSON_GetObjectItemCaseSensitive(response, "success");
    if (successItem != NULL && cJSON_IsBool(successItem)) {
        return cJSON_IsTrue(successItem);
    }
    return false;
}

cJSON *group_list(const SmsAero *sms_aero, const int page) {
    cJSON *response = request(sms_aero, "group/list", NULL, page);
    return response;
}

cJSON *contact_add(const SmsAero *sms_aero, const char *number, const int group_id, const char *birthday, const char *sex,
                   const char *last_name, const char *first_name, const char *surname, const char *param1,
                   const char *param2, const char *param3) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "number", number);
    if (group_id != -1) {
        cJSON_AddNumberToObject(data, "groupId", group_id);
    }
    cJSON *response = request(sms_aero, "contact/add", data, -1);
    cJSON_Delete(data);
    return response;
}

bool contact_delete(const SmsAero *sms_aero, const int contact_id) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", contact_id);
    cJSON *response = request(sms_aero, "contact/delete", data, -1);
    cJSON_Delete(data);
    cJSON *successItem = cJSON_GetObjectItemCaseSensitive(response, "success");
    if (successItem != NULL && cJSON_IsBool(successItem)) {
        return cJSON_IsTrue(successItem);
    }
    return false;
}

bool contact_delete_all(const SmsAero *sms_aero) {
    cJSON *response = request(sms_aero, "contact/delete-all", NULL, -1);
    cJSON *successItem = cJSON_GetObjectItemCaseSensitive(response, "success");
    if (successItem != NULL && cJSON_IsBool(successItem)) {
        return cJSON_IsTrue(successItem);
    }
    return false;
}

cJSON *contact_list(const SmsAero *sms_aero, const int page) {
    cJSON *response = request(sms_aero, "contact/list", NULL, page);
    return response;
}

cJSON *blacklist_add(const SmsAero *sms_aero, const char *numbers) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "numbers", numbers);
    cJSON *response = request(sms_aero, "blacklist/add", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *blacklist_list(const SmsAero *sms_aero, const char *numbers, const int page) {
    cJSON *data = cJSON_CreateObject();
    if (numbers != NULL) {
        cJSON_AddStringToObject(data, "numbers", numbers);
    }
    cJSON *response = request(sms_aero, "blacklist/list", data, page);
    cJSON_Delete(data);
    return response;
}

bool blacklist_delete(const SmsAero *sms_aero, const int blacklist_id) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", blacklist_id);
    cJSON *response = request(sms_aero, "blacklist/delete", data, -1);
    cJSON_Delete(data);
    cJSON *successItem = cJSON_GetObjectItemCaseSensitive(response, "success");
    if (successItem != NULL && cJSON_IsBool(successItem)) {
        return cJSON_IsTrue(successItem);
    }
    return false;
}

cJSON *hlr_check(const SmsAero *sms_aero, const char *numbers) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "numbers", numbers);
    cJSON *response = request(sms_aero, "hlr/check", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *hlr_status(const SmsAero *sms_aero, const int hlr_id) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", hlr_id);
    cJSON *response = request(sms_aero, "hlr/status", data, -1);
    cJSON_Delete(data);
    return response;
}

cJSON *number_operator(const SmsAero *sms_aero, const char *numbers) {
    cJSON *data = cJSON_CreateObject();
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
    if (group_id != -1) cJSON_AddNumberToObject(data, "groupId", group_id);
    if (sign) cJSON_AddStringToObject(data, "sign", sign);
    if (channel) cJSON_AddStringToObject(data, "channel", channel);
    cJSON_AddStringToObject(data, "text", text);
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
    cJSON *response = request(sms_aero, "viber/list", NULL, page);
    return response;
}

cJSON *viber_statistics(const SmsAero *sms_aero, const int sending_id, const int page) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "sendingId", sending_id);
    cJSON *response = request(sms_aero, "viber/statistic", data, page);
    cJSON_Delete(data);
    return response;
}
