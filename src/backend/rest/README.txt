сейчас сервер выкл

можно включить в postgresql.conf
rest_include_process = 'walreceiver, walsender'

запрос на порт 

8080 - walreceiver
8081 - walsender

доступны для теста эндпоинты 

/status - просто заглушка
/info - выводит порт и тип процесса (числом)
/lsn - показывает replay_lsn