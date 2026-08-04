# Copyright (c) 2022-2026, PostgreSQL Global Development Group

my_extension_sources = files(
    'rest_bgworker.c',
    'rest_server.c',
    'endpoint_handlers.c',
)

if host_system == 'windows'
    my_extension_sources += rc_lib_gen.process(win32ver_rc, extra_args: [
                                                   '--NAME', 'rest_bgworker',
                                                   '--FILEDESC', 'rest_bgworker with C func',])
endif

my_extension = shared_module('rest_bgworker',
                             my_extension_sources,
                             kwargs: contrib_mod_args,
)
contrib_targets += my_extension

install_data(
    'rest_bgworker.control',
    kwargs: contrib_data_args,
)

tests += {
    'name': 'rest_bgworker',
    'sd': meson.current_source_dir(),
    'bd': meson.current_build_dir(),
    'regress': {
        'sql': [
            'rest_bgworker',
        ],
    },
}