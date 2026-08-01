# Copyright (c) 2022-2026, PostgreSQL Global Development Group

my_extension_sources = files(
    'my_extension.c',
    'rest_server.c',
    'endpoint_handlers.c',
)

if host_system == 'windows'
    my_extension_sources += rc_lib_gen.process(win32ver_rc, extra_args: [
                                                   '--NAME', 'my_extension',
                                                   '--FILEDESC', 'my_extension with C func',])
endif

my_extension = shared_module('my_extension',
                             my_extension_sources,
                             kwargs: contrib_mod_args,
)
contrib_targets += my_extension

install_data(
    'my_extension--1.0.sql',
    'my_extension.control',
    kwargs: contrib_data_args,
)

tests += {
    'name': 'my_extension',
    'sd': meson.current_source_dir(),
    'bd': meson.current_build_dir(),
    'regress': {
        'sql': [
            'my_extension',
        ],
    },
}