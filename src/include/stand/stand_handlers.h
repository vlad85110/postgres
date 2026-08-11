#ifndef POSTGRES_STAND_HANDLERS_H
#define POSTGRES_STAND_HANDLERS_H

extern const char *handle_set_delay(const char *method, const char *body, void *user_data,
                                int *status_code, const char **status_text, const char **content_type);
extern const char *handle_get_delays(const char *method, const char *body, void *user_data,
                                int *status_code, const char **status_text, const char **content_type);

#endif //POSTGRES_STAND_HANDLERS_H
