MODULE_big = rest_bgworker
OBJS = rest_bgworker.o rest_server.o endpoint_handlers.o
PGFILEDESC = "BG Worker with REST server"

EXTENSION = rest_bgworker
DATA = my_extension--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)