по умолчанию сервер выключен

можно включить в postgresql.conf, раскомментировав

#rest_config_file = 'rest_config.json'
----------------------------------------------------------------

rest_config.json - содержимое (оставляем необходимые процессы, меняем порты на удобные):

{
    "ports": {
        "walreceiver": 6432,
        "walsender": 6532,
        "walwriter": 6632,
        "bgwriter": 6732,
        "checkpointer": 6832,
        "autovacuum": 6932
    }
}

файл по умолчанию должен лежать в pgdata

----------------------------------------------------------------

доступен для теста эндпоинт
/info - выводит порт и тип процесса

эндпоинты необходимо регистировать в main-функции процесса перед основным циклом с помощью функции

register_endpoint(RestServer *server, const char *url, endpoint_handler handler, void *user_data), где

url       - сам эндпоинт (например /info)
handler   - функция-обработчик, которая будет вызвана при отправке запроса с соответствующим эндпоинтом
user_data - данные, которые можно передать из main-функции процесса в обработчик

примеры регистрации:
register_endpoint(rest_server, "/info", handle_info, &data);
register_endpoint(rest_server, "/info", handle_info, NULL);
----------------------------------------------------------------

свои хендлеры можно писать в файле src/backend/rest/endpoint_handlers.c

хендлеры имеют вид:

handle_info(Request *request, Response *response), где

request содержит:
method       - GET/POST
body         - тело запроса
url          - эндпоинт
user_data    - данные переданные из main-функции процесса

response содержит:
status_code  - статус-код HTTP-ответа (с каким статус-кодом должен быть ответ на запрос)
status_text  - статус-текст HTTP-ответа (с каким статус-текстом должен быть ответ на запрос)
content-type - тип HTTP-ответа (какого типа должен быть ответ)
body         - тело ответа (что должно прийти в ответе)
----------------------------------------------------------------

поддерживаются запросы вида:

curl http://<host>:<port>/<endpoint>
curl -X GET http://<host>:<port>/<endpoint>
curl -X POST http://<host>:<port>/<endpoint> -H "Content-Type: <type>" -d '{"<variable>": <value>}'

примеры запросов:

curl http://localhost:8432/info
curl -X GET http://localhost:8432/info
curl -X POST http://localhost:8432/set -H "Content-Type: application/json" -d '{"value": 50}'
----------------------------------------------------------------