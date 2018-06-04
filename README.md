# TinyDNS

This is a tiny DNS server with simple JSON config written in C.

## Features

* filesize is just only 20Kb!
* cache DNS queries for boost internet connection
* resolve own domains from config
* resolve multidomains like *.example.com

## Compile and Install

* if you use [Archlinux](https://archlinux.org), you may install [tinydns from AUR](https://aur.archlinux.org/packages/tinydns/)
* for compile just run `compile.sh`
* after install you need to write your IP address in `/etc/tinydns.conf`
* you may also use `systemctl` for start and stop service
