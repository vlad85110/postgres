сейчас сервер выключен
можно включить в postgresql.conf
rest_include_processes = 'walreceiver, walsender, walwriter,
                          bgwriter, checkpointer, autovacuum'


запрос на порт (port = порт, на котором запущен постгрес)
walreceiver    port + 1000
walsender      port + 1001
walwriter      port + 1002
bgwriter       port + 1003
checkpointer   port + 1004
autovacuum     port + 1005


доступен для теста эндпоинт
/info - выводит порт и тип процесса


поддерживаются запросы вида:
curl http://<host>:<port>/<endpoint>
curl -X GET http://<host>:<port>/<endpoint>
curl -X POST http://<host>:<port>/<endpoint> -H "Content-Type: <type>" -d '{"<variable>": <value>}'


примеры:
curl http://localhost:8432/info
curl -X GET http://localhost:8432/info
curl -X POST http://localhost:8432/set -H "Content-Type: application/json" -d '{"value": 50}'