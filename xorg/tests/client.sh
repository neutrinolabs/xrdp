#!/bin/sh

xhost +

# delete chache directory for testing
rm -r .nx

if ! [ -d .nx ]
then
  mkdir .nx
fi

export LD_LIBRARY_PATH=$PWD

./nxproxy -S nx/nx,session=session,id=jay,root=.nx,connect=127.0.0.1:10,delta=1,stream=1,data=1

