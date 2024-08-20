#ifndef SMSAERO_H
#define SMSAERO_H

#include "cjson/cJSON.h"
#include <stdbool.h>

typedef struct {
    char *email;
    char *api_key;
    char *signature;
} SmsAero;

typedef struct {
    char *message;
} SmsAeroError;

SmsAeroError *create_error(const char *message);
void free_error(SmsAeroError *error);

SmsAero *init_sms_aero(const char *email, const char *api_key, const char *signature);
void cleanup_sms_aero(SmsAero *sms_aero);

cJSON *request(const SmsAero *sms_aero, const char *selector, const cJSON *data, int page);

cJSON *send_sms(const SmsAero *sms_aero, const char *number, const char *text, const char *date_send, const char *callback_url, SmsAeroError **error);

cJSON *sms_status(const SmsAero *sms_aero, int sms_id);

cJSON *sms_list(const SmsAero *sms_aero, const char *number, const char *text, int page);

cJSON *balance(const SmsAero *sms_aero);

cJSON *balance_add(const SmsAero *sms_aero, float amount, int card_id);

cJSON *cards(const SmsAero *sms_aero);

cJSON *tariffs(const SmsAero *sms_aero);

cJSON *sign_list(const SmsAero *sms_aero, int page);

cJSON *group_add(const SmsAero *sms_aero, const char *name);

bool group_delete(const SmsAero *sms_aero, int group_id);

bool group_delete_all(const SmsAero *sms_aero);

cJSON *group_list(const SmsAero *sms_aero, int page);

cJSON *contact_add(const SmsAero *sms_aero, const char *number, int group_id, const char *birthday, const char *sex, const char *last_name, const char *first_name, const char *surname, const char *param1, const char *param2, const char *param3);

bool contact_delete(const SmsAero *sms_aero, int contact_id);

bool contact_delete_all(const SmsAero *sms_aero);

cJSON *contact_list(const SmsAero *sms_aero, int page);

cJSON *blacklist_add(const SmsAero *sms_aero, const char *numbers);

cJSON *blacklist_list(const SmsAero *sms_aero, const char *numbers, int page);

bool blacklist_delete(const SmsAero *sms_aero, int blacklist_id);

cJSON *hlr_check(const SmsAero *sms_aero, const char *numbers);

cJSON *hlr_status(const SmsAero *sms_aero, int hlr_id);

cJSON *number_operator(const SmsAero *sms_aero, const char *numbers);

cJSON *viber_send(const SmsAero *sms_aero, const char *sign, const char *channel, const char *text, const char *number, int group_id, const char *image_source, const char *text_button, const char *link_button, const char *date_send, const char *sign_sms, const char *channel_sms, const char *text_sms, int price_sms);

cJSON *viber_sign_list(const SmsAero *sms_aero);

cJSON *viber_list(const SmsAero *sms_aero, int page);

cJSON *viber_statistics(const SmsAero *sms_aero, int sending_id, int page);

#endif // SMSAERO_H
