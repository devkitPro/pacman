scripts = [
  'makepkg-template.pl.in',
  'makepkg.sh.in',
  'pacman-db-upgrade.sh.in',
  'pacman-key.sh.in',
  'pkgdelta.sh.in',
  'repo-add.sh.in'
]

library_files = [
  'library/human_to_size.sh',
  'library/size_to_human.sh',
]

SCRIPT_EDITOR = find_program(configure_file(
  input : join_paths(meson.source_root(), 'build-aux/edit-script.sh.in'),
  output : 'edit-script.sh',
  configuration : substs))

m4_edit = generator(
  M4,
  arguments : ['-P', '-I', meson.current_source_dir(), '@INPUT@'],
  output : '@PLAINNAME@',
  capture : true)

foreach script : scripts
  script_shortname = script.split('.')[0]

  # Build the script, but don't install it. We want to keep it as a "private"
  # artifact that we reference from a wrapper script in order to bootstrap it
  # the build directory.
  internal_script = custom_target(
    script,
    input : m4_edit.process(script),
    command : [ SCRIPT_EDITOR, '@INPUT@', '@OUTPUT@', '0755'],
    output : script,
    depend_files : library_files,
    build_by_default : true)

  # Create a wrapper script that bootstraps the real script within the build
  # directory.
  custom_target(
    'wrap_@0@'.format(script_shortname),
    input : join_paths(meson.source_root(), 'build-aux', 'script-wrapper.sh.in'),
    output : script_shortname,
    build_by_default : true,
    command : [
      SED,
      '-e', 's,@BASH@,"@0@",'.format(BASH.path()),
      '-e', 's,@BUILDDIR@,"@0@",'.format(meson.current_build_dir()),
      '-e', 's,@REAL_PROGPATH@,"@0@",'.format(internal_script.full_path()),
      '@INPUT@',
    ],
    capture : true)

  # Install the real script
  meson.add_install_script(MESON_INSTALL_SCRIPT,
                           internal_script.full_path(),
                           join_paths(BINDIR, script_shortname))
endforeach

foreach symlink : ['repo-remove', 'repo-elephant']
  meson.add_install_script(MESON_MAKE_SYMLINK,
                           'repo-add',
                           join_paths(BINDIR, symlink))
endforeach

subdir('libmakepkg')

custom_target(
  'bash_completion',
  command : [ SCRIPT_EDITOR, '@INPUT@', '@OUTPUT@' ],
  input : 'completion/bash_completion.in',
  output : 'pacman',
  install : true,
  install_dir : BASHCOMPDIR)

foreach symlink : ['pacman-key', 'makepkg']
  meson.add_install_script(MESON_MAKE_SYMLINK,
                           'pacman',
                           join_paths(BASHCOMPDIR, symlink))
endforeach

zsh_completion_dir = join_paths(DATAROOTDIR, 'zsh/site-functions')
custom_target(
  'zsh_completion',
  command : [ SCRIPT_EDITOR, '@INPUT@', '@OUTPUT@' ],
  input : 'completion/zsh_completion.in',
  output : '_pacman',
  install : true,
  install_dir : zsh_completion_dir)
