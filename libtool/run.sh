#!/usr/bin/sh
./generic  --funcdump --funcdumplist main,devi_lfind --funcdumppath ./fucku.c ../main.c
less ./fucku.c
