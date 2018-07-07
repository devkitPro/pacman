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
  custom_target(
    script,
    input : m4_edit.process(script),
    command : [ SCRIPT_EDITOR, '@INPUT@', '@OUTPUT@', '0755'],
    output : script.split('.')[0],
    depend_files : library_files,
    install : true,
    install_dir : get_option('bindir'))
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
