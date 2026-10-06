#include "rest/rest_config.h"
#include "postgres.h"

#define BUF_SIZE 8192

RestConfig rest_config;
extern char *DataDir;

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
    fclose(file);
    if (bytes_read == 0)
    {
        elog(WARNING, "rest: cannot read config file: %s %m", path);
        return -1;
    }

    buffer[bytes_read] = '\0';

    return bytes_read;
}

static char *
read_proc_name(char *cur, char *proc_name, size_t max_len)
{
    char *name_start;
    size_t name_len;

    if (*cur != '"')
    {
        elog(WARNING, "rest: expected '\"' before process name");
        return NULL;
    }
    cur++;

    name_start = cur;
    while (*cur != '\0' && *cur != '"')
    {
        cur++;
    }

    if (*cur != '"')
    {
        elog(WARNING, "rest: expected '\"' after process name");
        return NULL;
    }

    name_len = cur - name_start;
    if (name_len >= max_len)
    {
        name_len = max_len - 1;
    }

    strncpy(proc_name, name_start, name_len);
    proc_name[name_len] = '\0';
    cur++;

    return cur;
}

static char *
read_port(char *cur, int *port)
{
    if(!isdigit(*cur))
    {
        elog(WARNING, "rest: expected number for port");
        return NULL;
    }

    *port = atoi(cur);

    while (*cur != '\0' && isdigit(*cur))
    {
        cur++;
    }

    return cur;
}

static char *
skip_chars(char *cur, const char *chars) 
{
    while (*cur != '\0' && strchr(chars, *cur) != NULL)
    {
        cur++;
    }
    return cur;
}

void
rest_config_parse(const char *path)
{
    char f_path[MAXPGPATH];
    char buffer[BUF_SIZE];
    char *ports_pos;
    char *obj_start;
    char *cur;
    int count = 0;

    if (path == NULL || path[0] == '\0')
    {
        return;
    }

    if (path[0] == '/')
    {
        strlcpy(f_path, path, sizeof(f_path));
    }
    else 
    {
        snprintf(f_path, sizeof(f_path), "%s/%s", DataDir, path);
    }

    if (read_file_to_buffer(f_path, buffer, sizeof(buffer)) < 0)
    {
        return;
    }

    ports_pos = strstr(buffer, "\"ports\"");
    if (ports_pos == NULL)
    {
        elog(WARNING, "rest: no \"ports:\" in config file");
        return;
    }

    obj_start = strchr(ports_pos, '{');
    if (obj_start == NULL)
    {
        elog(WARNING, "rest: no '{' after \"ports:\"");
        return;
    }
    obj_start++;

    cur = obj_start;
    while (*cur != '\0' && *cur != '}' && count < MAX_CONFIG_ENTRIES)
    {
        cur = skip_chars(cur, "\n\r\t, ");
        if (*cur == '}' || *cur == '\0')
        {
            break;
        }

        cur = read_proc_name(cur, rest_config.entries[count].process_name, sizeof(rest_config.entries[count].process_name));
        if (cur == NULL)
        {
            return;
        }

        cur = skip_chars(cur, "\n\r\t: ");

        cur = read_port(cur, &rest_config.entries[count].port);
        if (cur == NULL)
        {
            return;
        }

        count++;
    }

    rest_config.count = count;
    elog(DEBUG1, "rest: loaded %d entries", count);
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
