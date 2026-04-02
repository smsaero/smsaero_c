#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "smsaero.h"

#define MAX_ARG_LENGTH 1024

static void print_help(void) {
    printf("Help:\n"
        "-e: Email\t\t(ex: -e 'user@local.host')\n"
        "-t: Auth Token\t\t(ex: -t 'your_token')\n"
        "-n: To Number(s)\t(ex: -n 70000000000)\n"
        "-m: Message to send\t(ex: -m 'Hello, World!')\n"
        "-h: This help dialog\n");
}

static int validate_args(const char *user_email, const char *auth_token, const char *to_number, const char *message) {
    if (!user_email || !auth_token || !to_number || !message) {
        return 0;
    }

    if (strlen(user_email) >= MAX_ARG_LENGTH ||
        strlen(auth_token) >= MAX_ARG_LENGTH ||
        strlen(to_number) >= MAX_ARG_LENGTH ||
        strlen(message) >= MAX_ARG_LENGTH) {
        fprintf(stderr, "Argument length exceeds maximum allowed size\n");
        return 0;
    }

    if (!strchr(user_email, '@')) {
        fprintf(stderr, "Invalid email format\n");
        return 0;
    }

    return 1;
}

int main(int argc, char *argv[]) {
    const char *user_email = NULL, *auth_token = NULL, *message = NULL, *to_number = NULL;
    int exit_code = EXIT_SUCCESS;

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
            default:
                print_help();
                return EXIT_FAILURE;
        }
    }

    if (!validate_args(user_email, auth_token, to_number, message)) {
        fprintf(stderr, "Missing required arguments. Use -h for help.\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    SmsAero *sms_aero = init_sms_aero(user_email, auth_token, NULL);
    if (sms_aero == NULL) {
        fprintf(stderr, "Failed to initialize SmsAero\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    SmsAeroError *error = NULL;
    cJSON *result = send_sms(sms_aero, to_number, message, NULL, NULL, &error);
    if (error) {
        fprintf(stderr, "SmsAero error: %s\n", error->message);
        free_error(error);
        exit_code = EXIT_FAILURE;
    } else if (result) {
        char *result_str = cJSON_Print(result);
        if (result_str) {
            printf("%s\n", result_str);
            free(result_str);
        } else {
            fprintf(stderr, "Failed to print JSON result\n");
            exit_code = EXIT_FAILURE;
        }
    }
    cJSON_Delete(result);
    cleanup_sms_aero(sms_aero);

cleanup:
    return exit_code;
}
