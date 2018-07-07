tests = [
  'parseopts_test.sh',
  'makepkg-template_test.sh',
  'human_to_size_test.sh',
]

foreach tst : tests
  test(tst,
       PYTHON,
       env : TEST_ENV,
       args : [
         join_paths(meson.source_root(), 'build-aux/tap-driver.py'),
         join_paths(meson.current_source_dir(), tst),
       ])
endforeach
