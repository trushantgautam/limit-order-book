CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude

.PHONY: all test bench compare demo clean

all: test compare demo

test:
	$(CXX) $(CXXFLAGS) src/order_book.cpp tests/test_order_book.cpp -o tests/run_tests
	./tests/run_tests
	$(CXX) $(CXXFLAGS) src/order_book_v2.cpp tests/test_order_book_v2.cpp -o tests/run_tests_v2
	./tests/run_tests_v2

bench:
	$(CXX) $(CXXFLAGS) src/order_book.cpp bench/benchmark.cpp -o bench/run_bench
	./bench/run_bench 1000000

compare:
	$(CXX) $(CXXFLAGS) src/order_book.cpp src/order_book_v2.cpp bench/benchmark_compare.cpp -o bench/run_compare
	./bench/run_compare 1000000

demo:
	$(CXX) $(CXXFLAGS) src/order_book.cpp src/demo_main.cpp -o demo
	./demo

clean:
	rm -f tests/run_tests tests/run_tests_v2 bench/run_bench bench/run_compare demo
