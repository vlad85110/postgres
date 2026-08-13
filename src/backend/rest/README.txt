сейчас сервер выкл

можно включить в postgresql.conf
rest_include_processes = 'walreceiver, walsender'

запрос на порт (*port = порт, на котором запущен постгрес)

walreceiver - 8080
walsender - 8081
walwriter - 8082
bgwriter - port + 3000
checkpointer - port + 3100
autovacuum - port + 3200

доступны для теста эндпоинты 

/status - просто заглушка
/info - выводит порт и тип процесса
/lsn - показывает replay_lsn