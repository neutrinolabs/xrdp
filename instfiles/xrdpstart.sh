#!/bin/bash
cd /usr/local/xrdp
./xrdp&
./sesman > /usr/local/xrdp/sesman.boot.log
