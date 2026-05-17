# AL-OS

![OS Dev](https://img.shields.io/badge/OS-Development-blue)
![Language C](https://img.shields.io/badge/Language-C-green)
![Bootloader GRUB](https://img.shields.io/badge/Bootloader-GRUB-orange)
![Status Experimental](https://img.shields.io/badge/Status-Experimental-yellow)
![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)

---

## 🇷🇺 О проекте

**AL-OS** - экспериментальная операционная система на языке **C** с загрузчиком **GRUB**.
Проект создаётся "ради прикола" и интереса к системному программированию.

Работает на обычных компьютерах **в BIOS-режиме**.
**UEFI пока не поддерживается.**

Система находится в активной разработке, обновления выходят постепенно.

---

## Лицензия

Этот проект распространяется под лицензией GPL-3.0 (GNU General Public License v3.0).
Подробности смотрите в файле LICENSE.

---

## Автор

Создатель проекта - **Al-Mus**

---

## Возможности

* собственное ядро
* GRUB загрузка
* базовый вывод
* ISO-образ в Releases

---

## Поддерживаемые режимы загрузки

* BIOS - ✔ поддерживается
* UEFI - ✘ пока нет

---

## Релизы

➡ [![Releases](https://img.shields.io/github/v/release/MrLeo0010/al-os?label=Releases\&color=blue)](https://github.com/MrLeo0010/al-os/releases)

---

## Сборка

Ubuntu/Debian:
sudo apt update && sudo apt install -y build-essential gcc-i686-linux-gnu gcc-14-i686-linux-gnu libc6-dev-i386 binutils nasm grub-pc-bin grub-common xorriso && make iso

Также вы можете использовать docker контейнер mrleo0010/al-os-build:
docker run --rm -v "$PWD:/build" mrleo0010/al-os-build sh -c "make iso"

---

## Запуск

Инструкция по запуску появится позже.
На данный момент можно использовать:

* QEMU
* VirtualBox
* реальный ПК (BIOS mode)

---

## Участие

Форк, предложения, Pull Requests - приветствуются.
Если есть идеи - создавайте Issue.

---

## Благодарности

Проект частично основан на форке репозитория, в котором была выполнена переработка и упорядочивание кода:

- https://github.com/MrLeo0010/al-os

Автор форка:
- @MrLeo0010

---
