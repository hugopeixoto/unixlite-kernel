#!/bin/sh
ls -l /dev/hda1|sed 's/,//'|awk '{print $5*256+$6}'
