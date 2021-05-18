# TinyDNS

This is a tiny DNS server with simple JSON config written in C.

## Features

* filesize is just only 20Kb!
* cache DNS queries for boost internet connection
* resolve own domains from config
* resolve multidomains like *.example.com
* server can work on IPv6 address

## Compile and Install

* if you use [Archlinux](https://archlinux.org), you may install [tinydns from AUR](https://aur.archlinux.org/packages/tinydns/)
* for compile just run `make`
* after install you need to write your IP address in `/etc/tinydns.conf`
* you may also use `systemctl` for start and stop service

## 备注
1. 原版基础上增加了端口的配置，可以在配置文件 /etc/tinydns/tinydns.conf 中指定 
2. 修改了Makefile文件，可以 make CC=arm-openwrt-linux-gcc 进行跨平台编译
3. 本地侧修改为epoll方式实现

主要是用来做DNS缓存服务器用
