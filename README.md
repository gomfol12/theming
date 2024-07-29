# theming

A simple and overengineered theme generator. I know this could be a simple python script,
but where be the fun in that? So I wrote it in C.

It generates a theme based on a given image (mainly using oomox). But any command
can be added in the config file.

# Installation

```
make release
make install
```

# Usage

Copy `/usr/local/share/theming/content/config.json` to `~/.config/theming/config.json` and edit it to your needs.

To change theme run: `theming -ri /path/to/image`

# config file

Example file can be found in `content` dir or in `/usr/local/share/theming/content/config.json`

- generating_commands: list of commands that should be executed to generate the theme
- reload_commands: list of commands that should be executed to reload the theme

# Building and dependencies

- Dependencies:
    - [json-c](https://github.com/json-c/json-c)

- Building:
```
make
```

# Greatly inspired and copied from

- [wal](https://github.com/dylanaraps/pywal)
