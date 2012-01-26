#!/bin/sh

export LD_LIBRARY_PATH=$PWD

#./nxagent -R -bs -dpi 96 -geometry 1024x768 -noshpix -display nx/nx,link=adsl,delta=1,stream=1,data=1,a8taint=0,cache=4M:9 :9

# with cache
#./nxagent -D -bs -ac -dpi 96 -geometry 1024x768 -noshpix -display nx/nx,link=adsl,delta=1,stream=1,data=1,cache=4M:9 :9

# without cache
./nxagent -D -bs -ac -dpi 96 -geometry 1024x768 -noshpix -display nx/nx,link=adsl,delta=1,stream=1,data=1,cache=0M:9 :9

