all: l3.exe spdlog.exe

l3.exe: mt_ubench_l3.cpp
	g++ -Ofast -o l3.exe -I../../include mt_ubench_l3.cpp ../../src/l3.c ../../l3.S -lpthread

spdlog.exe: mt_ubench_spdlog.cpp
	g++ -Ofast -o spdlog.exe mt_ubench_spdlog.cpp -lpthread

