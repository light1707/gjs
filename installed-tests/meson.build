### Installed tests ############################################################

installed_tests_execdir = get_option('prefix') / get_option('libexecdir') / 'installed-tests' / meson.project_name()
installed_tests_metadir = abs_datadir / 'installed-tests' / meson.project_name()

# Simple shell script tests #

simple_tests = []

# The test scripts need to be ported from shell scripts
# for MSVC builds, which do not use BASH-style shells
if cxx.get_id() != 'msvc'
    simple_tests += [
        'CommandLine',
        'Warnings',
    ]
endif

foreach test : simple_tests
    test_file = files('scripts' / 'test@0@.sh'.format(test))

    test(test, test_file, env: tests_environment, protocol: 'tap',
        suite: 'Scripts')

    test_description_subst = {
        'name': 'test@0@.sh'.format(test),
        'installed_tests_execdir': installed_tests_execdir,
    }
    test_description = configure_file(configuration: test_description_subst,
        input: 'script.test.in', output: 'test@0@.sh.test'.format(test),
        install: get_option('installed_tests'),
        install_dir: installed_tests_metadir)

    if get_option('installed_tests')
        install_data(test_file, install_dir: installed_tests_execdir / 'scripts')
    endif
endforeach

# Jasmine tests #

subdir('js')

# esm script tests #

esm_tests = [
    'default-import',
    'named-import',
    'gi-import',
    'system-import'
]

esm_test_driver = find_program(files('esm-test.sh'))
if get_option('installed_tests')
    install_data('esm-test.sh', install_dir: installed_tests_execdir)
endif

foreach test : esm_tests
    test_file = files('esm' / '@0@.js'.format(test))

    test('@0@ command'.format(test), esm_test_driver,
        args: test_file, env: tests_environment, protocol: 'tap',
        suite: 'ESM')

    test_description_subst = {
        'name': '@0@.js'.format(test),
        'installed_tests_execdir': installed_tests_execdir,
    }
    test_description = configure_file(configuration: test_description_subst,
        input: 'esm.test.in',
        output: '@0@.test'.format(test),
        install: get_option('installed_tests'),
        install_dir: installed_tests_metadir)

    if get_option('installed_tests')
        install_data(test_file, install_dir: installed_tests_execdir / 'esm')
        install_data('esm' / '@0@.js'.format(test),
            'esm' / '@0@.js.output'.format(test),
            install_dir: installed_tests_execdir / 'esm')
    endif
endforeach

# Debugger script tests #

debugger_tests = [
    'backtrace',
    'breakpoint',
    'continue',
    'delete',
    'detach',
    'down-up',
    'finish',
    'frame',
    'keys',
    'next',
    'print',
    'quit',
    'return',
    'set',
    'step',
    'throw',
    'until',
]

debugger_test_driver = find_program(files('debugger-test.sh'))
if get_option('installed_tests')
    install_data('debugger-test.sh', install_dir: installed_tests_execdir)
endif

foreach test : debugger_tests
    test_file = files('debugger' / '@0@.debugger'.format(test))

    test('@0@ command'.format(test), debugger_test_driver,
        args: test_file, env: tests_environment, protocol: 'tap',
        suite: 'Debugger')

    test_description_subst = {
        'name': '@0@.debugger'.format(test),
        'installed_tests_execdir': installed_tests_execdir,
    }
    test_description = configure_file(configuration: test_description_subst,
        input: 'debugger.test.in',
        output: '@0@.test'.format(test),
        install: get_option('installed_tests'),
        install_dir: installed_tests_metadir)

    if get_option('installed_tests')
        install_data(test_file, install_dir: installed_tests_execdir / 'debugger')
        install_data('debugger' / '@0@.debugger.js'.format(test),
            'debugger' / '@0@.debugger.output'.format(test),
            install_dir: installed_tests_execdir / 'debugger')
    endif
endforeach
