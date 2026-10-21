/**
 * @file test_error.c
 * @brief The host test for the error codes: what a code says, and what a code
 * that is not one of them says.
 *
 * The chip-independent half of the platform is what runs on a host, and this is
 * it: the error codes are the one thing every layer of the platform agrees on,
 * and a program that is told "invalid argument" and gets "Unknown error" is a
 * program nobody can debug.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdio.h>
#include <string.h>

#include "error.h"

static int failures = 0;

/* One fact, checked: the code says what it says. */
static void expect_string(RoboHubError code, const char* expected)
{
    const char* actual = robohub_error_to_string(code);
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "expected %d to say \"%s\", got \"%s\"\n",
                (int)code, expected, actual == NULL ? "(null)" : actual);
        failures++;
    }
}

int main(void)
{
    /* Every code the header declares says its own name. */
    expect_string(ROBOHUB_OK, "OK");
    expect_string(ROBOHUB_ERR_FAIL, "Generic failure");
    expect_string(ROBOHUB_ERR_INVALID_ARG, "Invalid argument");
    expect_string(ROBOHUB_ERR_TIMEOUT, "Operation timed out");
    expect_string(ROBOHUB_ERR_NO_MEM, "Out of memory");
    expect_string(ROBOHUB_ERR_NOT_SUPPORTED, "Feature not supported");
    expect_string(ROBOHUB_ERR_BUSY, "Resource busy");
    expect_string(ROBOHUB_ERR_NOT_FOUND, "Device or resource not found");
    expect_string(ROBOHUB_ERR_IO, "I/O error");
    expect_string(ROBOHUB_ERR_PROTOCOL, "Protocol error");
    expect_string(ROBOHUB_ERR_STATE, "Invalid state for operation");
    expect_string(ROBOHUB_ERR_UNKNOWN, "Unknown error");

    /* A code that is not one of them is unknown, rather than whatever the
     * switch happened to fall through to. */
    expect_string((RoboHubError)-999, "Unknown error");

    /* The macros the platform asks about errors with say what they mean. */
    if (!ROBOHUB_SUCCEEDED(ROBOHUB_OK)) {
        fprintf(stderr, "expected success to have succeeded\n");
        failures++;
    }
    if (ROBOHUB_SUCCEEDED(ROBOHUB_ERR_IO)) {
        fprintf(stderr, "expected an I/O error not to have succeeded\n");
        failures++;
    }
    if (!ROBOHUB_FAILED(ROBOHUB_ERR_BUSY)) {
        fprintf(stderr, "expected a busy resource to have failed\n");
        failures++;
    }

    if (failures > 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("error: every code says what it says\n");
    return 0;
}
