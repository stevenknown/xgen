#!/bin/bash
THREAD_NUM=16

make -j$THREAD_NUM xocc.exe -f Makefile.xocc TARG=FOR_ARM TARG_DIR=../arm DEBUG=true THREAD_NUM=$THREAD_NUM
