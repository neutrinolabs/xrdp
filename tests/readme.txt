
profiling the test

build with -pg
run
make clean
make PROFIL=-pg

run
tests/rfxcodectest --speed --count 10000

gprof -b tests/rfxcodectest > profile.txt

look at profile.txt

