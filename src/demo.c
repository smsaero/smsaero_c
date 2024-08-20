#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "smsaero.h"


void print_help() {
    printf("Help:\n"
        "-e: Email\t\t(ex: -e 'user@local.host')\n"
        "-t: Auth Token\t\t(ex: -t 'your_token')\n"
        "-n: To Number(s)\t(ex: -n 70000000000)\n"
        "-m: Message to send\t(ex: -m 'Hello, World!')\n"
        "-h: This help dialog\n");
}

int validate_args(const char *user_email, const char *auth_token, const char *to_number, const char *message) {
    return user_email && auth_token && to_number && message;
}

int main(int argc, char *argv[]) {
    char *user_email = NULL, *auth_token = NULL, *message = NULL, *to_number = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "e:t:n:m:h")) != -1) {
        switch (opt) {
            case 'e':
                user_email = optarg;
                break;
            case 't':
                auth_token = optarg;
                break;
            case 'n':
                to_number = optarg;
                break;
            case 'm':
                message = optarg;
                break;
            case 'h':
            default:
                print_help();
                return EXIT_FAILURE;
        }
    }

    if (!validate_args(user_email, auth_token, to_number, message)) {
        fprintf(stderr, "Missing required arguments. Use -h for help.\n");
        return EXIT_FAILURE;
    }

    SmsAero *sms_aero = init_sms_aero(user_email, auth_token, NULL);
    if (sms_aero == NULL) {
        fprintf(stderr, "Failed to initialize SmsAero\n");
        return EXIT_FAILURE;
    }

    SmsAeroError *error = NULL;
    cJSON *result = send_sms(sms_aero, to_number, message, NULL, NULL, &error);
    if (error) {
        fprintf(stderr, "SmsAero error: %s\n", error->message);
        free_error(error);
    } else if (result) {
        char *result_str = cJSON_Print(result);
        printf("%s\n", result_str);
        free(result_str);
    }
    cJSON_Delete(result);

    cleanup_sms_aero(sms_aero);
    return EXIT_SUCCESS;
}
