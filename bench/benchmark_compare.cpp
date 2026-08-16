#include "order_book.hpp"
#include "order_book_v2.hpp"
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>

using Clock = std::chrono::steady_clock;

struct Gen { bool buy; double price; uint64_t qty; };

static std::vector<Gen> makeOrders(uint64_t n) {
    std::mt19937_64 rng(42);
    std::normal_distribution<double> priceNoise(0.0, 0.75);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<uint64_t> qtyDist(1, 100);
    std::vector<Gen> out;
    out.reserve(n);
    for (uint64_t i = 0; i < n; ++i) {
        double raw = 100.0 + priceNoise(rng);
        double price = std::round(raw * 4.0) / 4.0;
        if (price <= 0) price = 0.25;
        out.push_back({ sideDist(rng) == 0, price, qtyDist(rng) });
    }
    return out;
}

struct Stats {
    double throughput, mean, p50, p99, p999, mx;
};

static Stats summarize(std::vector<double>& lat, double wallSec, uint64_t n) {
    std::sort(lat.begin(), lat.end());
    auto pct = [&](double p) { return lat[static_cast<size_t>(p * (lat.size() - 1))]; };
    double sum = 0; for (double v : lat) sum += v;
    return { n / wallSec, sum / lat.size(), pct(0.50), pct(0.99), pct(0.999), lat.back() };
}

static void print(const char* name, const Stats& s) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << name << "\n"
              << "  throughput: " << s.throughput / 1e6 << "M orders/sec\n"
              << "  mean: " << s.mean << " ns   p50: " << s.p50
              << " ns   p99: " << s.p99 << " ns   p99.9: " << s.p999
              << " ns   max: " << s.mx / 1000.0 << " us\n";
}

int main(int argc, char** argv) {
    const uint64_t N = (argc > 1) ? std::stoull(argv[1]) : 1'000'000;
    auto orders = makeOrders(N);

    // ---- v1: std::list-based levels ----
    {
        lob::OrderBook book;
        std::vector<double> lat; lat.reserve(N);
        auto w0 = Clock::now();
        for (uint64_t i = 0; i < N; ++i) {
            const auto& o = orders[i];
            auto t0 = Clock::now();
            auto trades = book.addLimitOrder(i + 1, o.buy ? lob::Side::Buy : lob::Side::Sell, o.price, o.qty);
            auto t1 = Clock::now();
            lat.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
            (void)trades;
        }
        auto w1 = Clock::now();
        print("v1 (heap-allocated std::list nodes)", summarize(lat, std::chrono::duration<double>(w1 - w0).count(), N));
    }

    // ---- v2: pooled nodes, intrusive lists, reused trade buffer ----
    {
        lob::v2::OrderBook book(N);
        std::vector<lob::v2::Trade> trades;
        trades.reserve(64);
        std::vector<double> lat; lat.reserve(N);
        auto w0 = Clock::now();
        for (uint64_t i = 0; i < N; ++i) {
            const auto& o = orders[i];
            trades.clear();
            auto t0 = Clock::now();
            book.addLimitOrder(i + 1, o.buy ? lob::v2::Side::Buy : lob::v2::Side::Sell, o.price, o.qty, trades);
            auto t1 = Clock::now();
            lat.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        }
        auto w1 = Clock::now();
        print("v2 (preallocated pool + intrusive lists)", summarize(lat, std::chrono::duration<double>(w1 - w0).count(), N));
    }
    return 0;
}
