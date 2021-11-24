# Testing

This file holds notes for testing purposes.

## Valgrind

Use Valgrind on a build with debug symbols to discover memory issues:
```bash
mkdir -p tmp-journal
make debug
valgrind --leak-check=full ./diary tmp-journal/ 2>log.txt
```

https://developers.redhat.com/blog/2021/04/23/valgrind-memcheck-different-ways-to-lose-your-memory#generating_a_leak_summary

Don't use `--show-leak-kinds=all` with Ncurses:
https://invisible-island.net/ncurses/ncurses.faq.html#config_leaks

## Compile with Debug Symbols

To make a build with debug symbols use the `debug` target:
```bash
make debug
```

## Send stderr to File

Send stderr to a file for debugging:
```bash
diary 2>log.txt
```

Or to `/dev/null` to ignore the debug messages flickering on the screen:
```bash
diary 2>/dev/null
```

## Render Documentation / Man Page

### Plain Text
Generate plain text from "runoff" text:
```bash
tbl man1/diary.1 | nroff -man | less
```

### `mandoc` HTML (preferred)
Install [`mandoc`](https://en.wikipedia.org/wiki/Mandoc):
```bash
sudo apt-get install mandoc
```

Generate HTML file:
```bash
mandoc -Thtml man1/diary.1 > man1/diary.1.html
```

### `groff` HTML
Install [`groff`](https://www.gnu.org/software/groff):
```bash
sudo apt-get install groff
```

Generate HTML file (`-t` for `tbl`):
```bash
groff -t -mandoc -Thtml man1/diary.1 > man1/diary.1.html
```
