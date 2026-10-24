/* =========================================================
 * SEMVER: parseo y comparacion de versiones/restricciones.
 * ======================================================= */

#include "semver.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef enum SemVerOp {
    OP_ANY,
    OP_CARET,
    OP_TILDE,
    OP_EXACT,
    OP_GE,
    OP_GT,
    OP_LE,
    OP_LT
} SemVerOp;

typedef struct SemVerConstraint {
    SemVerOp op;
    SemVer version;
} SemVerConstraint;

static char *semver_trim(char *text)
{
    if (text == NULL)
        return NULL;

    while (*text == ' ' || *text == '\t')
        text++;

    char *end = text + strlen(text);

    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t'))
        end--;

    *end = '\0';

    return text;
}


static int semver_parse_component(
    const char **cursor,
    int *out
)
{
    const char *text = *cursor;

    if (!isdigit((unsigned char) *text))
        return 0;

    long value = 0;

    while (isdigit((unsigned char) *text)) {

        value = value * 10 + (*text - '0');

        if (value > INT_MAX)
            return 0;

        text++;
    }

    *out = (int) value;
    *cursor = text;

    return 1;
}


int semver_parse(const char *text, SemVer *out)
{
    if (text == NULL || out == NULL)
        return 0;

    while (*text == ' ' || *text == '\t')
        text++;

    SemVer version;
    version.major = 0;
    version.minor = 0;
    version.patch = 0;
    version.parts = 0;

    const char *cursor = text;

    if (!semver_parse_component(&cursor, &version.major))
        return 0;

    version.parts = 1;

    if (*cursor == '.') {

        cursor++;

        if (!semver_parse_component(&cursor, &version.minor))
            return 0;

        version.parts = 2;

        if (*cursor == '.') {

            cursor++;

            if (!semver_parse_component(
                    &cursor,
                    &version.patch))
                return 0;

            version.parts = 3;
        }
    }

    while (*cursor == ' ' || *cursor == '\t')
        cursor++;

    if (*cursor != '\0')
        return 0;

    *out = version;

    return 1;
}


int semver_compare(const SemVer *left, const SemVer *right)
{
    if (left->major != right->major)
        return left->major < right->major ? -1 : 1;

    if (left->minor != right->minor)
        return left->minor < right->minor ? -1 : 1;

    if (left->patch != right->patch)
        return left->patch < right->patch ? -1 : 1;

    return 0;
}


static int semver_parse_constraint(
    const char *text,
    SemVerConstraint *out
)
{
    while (*text == ' ' || *text == '\t')
        text++;

    if (*text == '\0')
        return 0;

    if (strcmp(text, "*") == 0 ||
        strcmp(text, "x") == 0 ||
        strcmp(text, "X") == 0) {

        out->op = OP_ANY;
        return semver_parse("0.0.0", &out->version);
    }

    SemVerOp op = OP_CARET;
    const char *version = text;

    if (text[0] == '^') {
        op = OP_CARET;
        version = text + 1;
    } else if (text[0] == '~') {
        op = OP_TILDE;
        version = text + 1;
    } else if (text[0] == '=') {
        op = OP_EXACT;
        version = text + 1;
    } else if (text[0] == '>' && text[1] == '=') {
        op = OP_GE;
        version = text + 2;
    } else if (text[0] == '<' && text[1] == '=') {
        op = OP_LE;
        version = text + 2;
    } else if (text[0] == '>') {
        op = OP_GT;
        version = text + 1;
    } else if (text[0] == '<') {
        op = OP_LT;
        version = text + 1;
    }

    while (*version == ' ' || *version == '\t')
        version++;

    if (!semver_parse(version, &out->version))
        return 0;

    out->op = op;

    return 1;
}


static int semver_constraint_matches(
    const SemVerConstraint *constraint,
    const SemVer *version
)
{
    int comparison =
        semver_compare(version, &constraint->version);

    switch (constraint->op) {

        case OP_ANY:
            return 1;

        case OP_EXACT:
            return comparison == 0;

        case OP_GE:
            return comparison >= 0;

        case OP_GT:
            return comparison > 0;

        case OP_LE:
            return comparison <= 0;

        case OP_LT:
            return comparison < 0;

        case OP_CARET:

            if (comparison < 0)
                return 0;

            if (constraint->version.major > 0)
                return version->major ==
                    constraint->version.major;

            if (constraint->version.minor > 0)
                return version->major == 0 &&
                    version->minor ==
                        constraint->version.minor;

            if (constraint->version.parts >= 3)
                return comparison == 0;

            if (constraint->version.parts == 2)
                return version->major == 0 &&
                    version->minor == 0;

            return version->major == 0;

        case OP_TILDE:

            if (comparison < 0)
                return 0;

            if (constraint->version.parts <= 1)
                return version->major ==
                    constraint->version.major;

            return version->major ==
                    constraint->version.major &&
                version->minor ==
                    constraint->version.minor;
    }

    return 0;
}


int semver_satisfies(const char *requirement, const SemVer *version)
{
    if (requirement == NULL || version == NULL)
        return -1;

    char *copy = malloc(strlen(requirement) + 1);

    if (copy == NULL)
        return -1;

    strcpy(copy, requirement);

    int satisfied = 1;
    char *cursor = copy;

    while (cursor != NULL && *cursor != '\0') {

        char *comma = strchr(cursor, ',');

        if (comma != NULL)
            *comma = '\0';

        char *piece = semver_trim(cursor);

        if (*piece == '\0') {

            if (comma != NULL) {
                cursor = comma + 1;
                continue;
            }

            satisfied = 0;
            break;
        }

        SemVerConstraint constraint;

        if (!semver_parse_constraint(piece, &constraint)) {

            satisfied = -1;
            break;
        }

        if (!semver_constraint_matches(
                &constraint,
                version)) {
            satisfied = 0;
        }

        cursor = comma != NULL ? comma + 1 : NULL;
    }

    free(copy);

    return satisfied;
}


int semver_valid_requirement(const char *requirement)
{
    if (requirement == NULL)
        return 0;

    SemVer dummy;
    dummy.major = 0;
    dummy.minor = 0;
    dummy.patch = 0;
    dummy.parts = 3;

    return semver_satisfies(requirement, &dummy) != -1;
}
