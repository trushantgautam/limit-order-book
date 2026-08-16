#include "order_book.hpp"
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>

using namespace lob;
using Clock = std::chrono::steady_clock;

struct GeneratedOrder {
    Side side;
    double price;
    uint64_t quantity;
};

int main(int argc, char** argv) {
    const uint64_t N = (argc > 1) ? std::stoull(argv[1]) : 1'000'000;

    std::mt19937_64 rng(42);
    std::normal_distribution<double> priceNoise(0.0, 0.75);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<uint64_t> qtyDist(1, 100);

    const double mid = 100.0;

    std::vector<GeneratedOrder> orders;
    orders.reserve(N);
    for (uint64_t i = 0; i < N; ++i) {
        Side side = sideDist(rng) == 0 ? Side::Buy : Side::Sell;
        double raw = mid + priceNoise(rng);
        double price = std::round(raw * 4.0) / 4.0; // quantize to 0.25 ticks
        if (price <= 0) price = 0.25;
        uint64_t qty = qtyDist(rng);
        orders.push_back({ side, price, qty });
    }

    OrderBook book;
    std::vector<double> latenciesNs;
    latenciesNs.reserve(N);

    uint64_t totalTrades = 0;
    auto wallStart = Clock::now();

    for (uint64_t i = 0; i < N; ++i) {
        const auto& o = orders[i];
        auto t0 = Clock::now();
        auto trades = book.addLimitOrder(i + 1, o.side, o.price, o.quantity);
        auto t1 = Clock::now();
        latenciesNs.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        totalTrades += trades.size();
    }

    auto wallEnd = Clock::now();
    double totalSeconds = std::chrono::duration<double>(wallEnd - wallStart).count();

    std::sort(latenciesNs.begin(), latenciesNs.end());
    auto pct = [&](double p) {
        size_t idx = static_cast<size_t>(p * (latenciesNs.size() - 1));
        return latenciesNs[idx];
    };

    double sum = 0;
    for (double v : latenciesNs) sum += v;
    double mean = sum / latenciesNs.size();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "==== Order Book Benchmark ====\n";
    std::cout << "Orders processed:     " << N << "\n";
    std::cout << "Trades generated:     " << totalTrades << "\n";
    std::cout << "Wall time:            " << totalSeconds << " s\n";
    std::cout << "Throughput:           " << (N / totalSeconds) << " orders/sec\n";
    std::cout << "Final book depth:     " << book.bidLevels() << " bid levels, "
              << book.askLevels() << " ask levels, " << book.openOrders() << " open orders\n";
    std::cout << "-- Per-order latency (single-threaded, includes matching + book maintenance) --\n";
    std::cout << "  mean:   " << mean << " ns\n";
    std::cout << "  p50:    " << pct(0.50) << " ns\n";
    std::cout << "  p99:    " << pct(0.99) << " ns\n";
    std::cout << "  p99.9:  " << pct(0.999) << " ns\n";
    std::cout << "  max:    " << latenciesNs.back() << " ns\n";
    return 0;
}
