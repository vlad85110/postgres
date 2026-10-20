#include "rest/rest_config.h"
#include "mb/pg_wchar.h"

extern char *DataDir;
#define BUF_SIZE 8192

RestConfig rest_config;

static const char *
json_error_name(JsonParseErrorType err)
{
    switch (err)
    {
        case JSON_SUCCESS:                  return "JSON_SUCCESS";
        case JSON_INCOMPLETE:               return "JSON_INCOMPLETE";
        case JSON_INVALID_LEXER_TYPE:       return "JSON_INVALID_LEXER_TYPE";
        case JSON_NESTING_TOO_DEEP:         return "JSON_NESTING_TOO_DEEP";
        case JSON_ESCAPING_INVALID:         return "JSON_ESCAPING_INVALID";
        case JSON_ESCAPING_REQUIRED:        return "JSON_ESCAPING_REQUIRED";
        case JSON_EXPECTED_ARRAY_FIRST:     return "JSON_EXPECTED_ARRAY_FIRST";
        case JSON_EXPECTED_ARRAY_NEXT:      return "JSON_EXPECTED_ARRAY_NEXT";
        case JSON_EXPECTED_COLON:           return "JSON_EXPECTED_COLON";
        case JSON_EXPECTED_END:             return "JSON_EXPECTED_END";
        case JSON_EXPECTED_JSON:            return "JSON_EXPECTED_JSON";
        case JSON_EXPECTED_MORE:            return "JSON_EXPECTED_MORE";
        case JSON_EXPECTED_OBJECT_FIRST:    return "JSON_EXPECTED_OBJECT_FIRST";
        case JSON_EXPECTED_OBJECT_NEXT:     return "JSON_EXPECTED_OBJECT_NEXT";
        case JSON_EXPECTED_STRING:          return "JSON_EXPECTED_STRING";
        case JSON_INVALID_TOKEN:            return "JSON_INVALID_TOKEN";
        case JSON_OUT_OF_MEMORY:            return "JSON_OUT_OF_MEMORY";
        case JSON_UNICODE_CODE_POINT_ZERO:  return "JSON_UNICODE_CODE_POINT_ZERO";
        case JSON_UNICODE_ESCAPE_FORMAT:    return "JSON_UNICODE_ESCAPE_FORMAT";
        case JSON_UNICODE_HIGH_ESCAPE:      return "JSON_UNICODE_HIGH_ESCAPE";
        case JSON_UNICODE_UNTRANSLATABLE:   return "JSON_UNICODE_UNTRANSLATABLE";
        case JSON_UNICODE_HIGH_SURROGATE:   return "JSON_UNICODE_HIGH_SURROGATE";
        case JSON_UNICODE_LOW_SURROGATE:    return "JSON_UNICODE_LOW_SURROGATE";
        case JSON_SEM_ACTION_FAILED:        return "JSON_SEM_ACTION_FAILED";
    }
    return "UNKNOWN";
}

static ssize_t
read_file_to_buffer(const char *path, char *buffer, size_t buf_size)
{
    FILE *file;
    ssize_t bytes_read;

    file = fopen(path, "r");
    if (file == NULL)
    {
        elog(WARNING, "rest: cannot open config file: %s %m", path);
        return -1;
    }

    bytes_read = fread(buffer, 1, buf_size - 1, file);
    if (bytes_read == buf_size - 1 && fgetc(file) != EOF)
    {
        fclose(file);
        elog(WARNING, "rest: config file %s is too large", path);
        return -1;
    }
    fclose(file);
    if (bytes_read == 0)
    {
        elog(WARNING, "rest: cannot read config file: %s %m", path);
        return -1;
    }

    buffer[bytes_read] = '\0';

    return bytes_read;
}

static void
build_path(const char *path, char *f_path, size_t size)
{
    if (path[0] == '/')
    {
        strlcpy(f_path, path, size);
    }
    else
    {
        snprintf(f_path, size, "%s/%s", DataDir, path);
    }
}

static bool
is_duplicate_field(const char *process_name)
{
    for (int i = 0; i < rest_config.count; i++)
    {
        if (strcmp(rest_config.entries[i].process_name, process_name) == 0)
        {
            elog(WARNING, "rest: field \"%s\" is duplicated", process_name);
            return true;
        }
    }
    return false;
}

static void
add_config_entry(const char *process_name, char *token)
{
    RestConfigEntry *entry = &rest_config.entries[rest_config.count];

    strlcpy(entry->process_name, process_name, sizeof(entry->process_name));
    entry->port = atoi(token);
    rest_config.count++;
}

static JsonParseErrorType
rest_object_start(void *pstate)
{
    RestParseState *state = (RestParseState *)pstate;
    state->depth++;

    return JSON_SUCCESS;
}

static JsonParseErrorType
rest_object_end(void *pstate)
{
    RestParseState *state = (RestParseState *)pstate;
    state->depth--;

    if (state->ports_depth > state->depth)
    {
        state->ports_depth = 0;
    }

    return JSON_SUCCESS;
}

static JsonParseErrorType
rest_object_field_start(void *pstate, char *fname, bool isnull)
{
    RestParseState *state = (RestParseState *)pstate;

    if (fname == NULL)
    {
        return JSON_SUCCESS;
    }

    if (state->ports_depth == 0)
    {
        if (strcmp(fname, "ports") == 0)
        {
            state->ports_depth = state->depth + 1;
        }
    }
    else if (state->depth == state->ports_depth)
    {
        strlcpy(state->process_name, fname, sizeof(state->process_name));
    }

    return JSON_SUCCESS;
}

static JsonParseErrorType
rest_object_field_end(void *pstate, char *fname, bool isnull)
{
    RestParseState *state = (RestParseState *)pstate;
    state->process_name[0] = '\0';

    return JSON_SUCCESS;
}

static JsonParseErrorType
rest_scalar(void *pstate, char *token, JsonTokenType tokentype)
{
    RestParseState *state = (RestParseState *)pstate;

    if (state->ports_depth == 0 || state->depth != state->ports_depth || tokentype != JSON_TOKEN_NUMBER)
    {
        return JSON_SUCCESS;
    }

    if (rest_config.count >= MAX_CONFIG_ENTRIES)
    {
        return JSON_SUCCESS;
    }

    if (is_duplicate_field(state->process_name))
    {
        return JSON_SUCCESS;
    }

    add_config_entry(state->process_name, token);

    return JSON_SUCCESS;
}

JsonParseErrorType
rest_config_parse(const char *path)
{
    char buffer[BUF_SIZE];
    char f_path[MAXPGPATH];
    JsonSemAction sem = {0};
    RestParseState state = {0};
    JsonLexContext lex;
    JsonParseErrorType result;

    if (path == NULL || path[0] == '\0')
    {
        return JSON_SUCCESS;
    }
    build_path(path, f_path, sizeof(f_path));

    if (read_file_to_buffer(f_path, buffer, sizeof(buffer)) < 0)
    {
        return JSON_INVALID_TOKEN;
    }

    sem.object_start = rest_object_start;
    sem.object_end = rest_object_end;
    sem.object_field_start = rest_object_field_start;
    sem.object_field_end = rest_object_field_end;
    sem.scalar = rest_scalar;
    sem.semstate = &state;

    makeJsonLexContextCstringLen(&lex, buffer, strlen(buffer), PG_UTF8, true);

    result = pg_parse_json(&lex, &sem);
    if (result != JSON_SUCCESS)
    {
        elog(WARNING, "rest: json parse error: %s", json_error_name(result));
        rest_config.count = 0;
        memset(rest_config.entries, 0, sizeof(rest_config.entries));
    }

    return result;
}

int
rest_config_get_port(const char *process_name)
{
    for (int i = 0; i < rest_config.count; i++)
    {
        if (strcmp(rest_config.entries[i].process_name, process_name) == 0)
        {
            return rest_config.entries[i].port;
        }
    }
    return -1;
}
