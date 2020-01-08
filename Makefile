main:
	g++ -o simple_bench.out -IBTree/ -L. -ltlx -mpopcnt -std=c++17 simple_bench.cpp

clean:
	rm -rf simple_bench.out
