сейчас сервер выключен

можно включить в postgresql.conf

rest_include_processes = 'walreceiver, walsender, walwriter,
                          bgwriter, checkpointer, autovacuum'
----------------------------------------------------------------

запрос на порт (port = порт, на котором запущен постгрес)

walreceiver    port + 1000
walsender      port + 1100
walwriter      port + 1200
bgwriter       port + 1300
checkpointer   port + 1400
autovacuum     port + 1500
----------------------------------------------------------------

доступен для теста эндпоинт
/info - выводит порт и тип процесса

эндпоинты необходимо регистировать в main-функции процесса перед основным циклом с помощью функции

register_endpoint(const char *url, endpoint_handler handler, void *user_data), где

url       - сам эндпоинт (например /info)
handler   - функция-обработчик, которая будет вызвана при отправке запроса с соответствующим эндпоинтом
user_data - данные, которые можно передать из main-функции процесса в обработчик

примеры регистрации:
register_endpoint("/info", handle_info, &data);
register_endpoint("/info", handle_info, NULL);
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