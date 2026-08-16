#include "order_book.hpp"
#include <cassert>
#include <iostream>

using namespace lob;

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAILED: " << #cond << " at line " << __LINE__ << "\n"; \
        failures++; \
    } \
} while (0)

void test_basic_match() {
    OrderBook book;
    // Resting sell at 100 x10
    auto t1 = book.addLimitOrder(1, Side::Sell, 100.0, 10);
    CHECK(t1.empty());
    CHECK(book.bestAsk().value() == 100.0);

    // Buy 5 @ 100 -> should fully match against resting sell, leaving 5 resting on ask
    auto t2 = book.addLimitOrder(2, Side::Buy, 100.0, 5);
    CHECK(t2.size() == 1);
    CHECK(t2[0].quantity == 5);
    CHECK(t2[0].price == 100.0);
    CHECK(book.askDepthAt(100.0) == 5);
    CHECK(book.bestBid() == std::nullopt); // buy order fully filled, nothing rests
}

void test_price_time_priority() {
    OrderBook book;
    // Two resting sells at the same price, different arrival order
    book.addLimitOrder(1, Side::Sell, 50.0, 5);
    book.addLimitOrder(2, Side::Sell, 50.0, 5);

    // Incoming buy for 7 should hit order 1 fully (5) then order 2 partially (2)
    auto trades = book.addLimitOrder(3, Side::Buy, 50.0, 7);
    CHECK(trades.size() == 2);
    CHECK(trades[0].restingOrderId == 1);
    CHECK(trades[0].quantity == 5);
    CHECK(trades[1].restingOrderId == 2);
    CHECK(trades[1].quantity == 2);
    CHECK(book.askDepthAt(50.0) == 3); // order 2 has 3 left
}

void test_price_improvement() {
    OrderBook book;
    // Resting sell at 99 (cheaper than the buy's limit of 101)
    book.addLimitOrder(1, Side::Sell, 99.0, 10);
    auto trades = book.addLimitOrder(2, Side::Buy, 101.0, 10);
    CHECK(trades.size() == 1);
    // Trade should execute at the resting order's price (99), not the aggressor's (101)
    CHECK(trades[0].price == 99.0);
}

void test_no_cross_rests_in_book() {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 105.0, 10);
    auto trades = book.addLimitOrder(2, Side::Buy, 100.0, 10); // doesn't cross
    CHECK(trades.empty());
    CHECK(book.bestBid().value() == 100.0);
    CHECK(book.bestAsk().value() == 105.0);
}

void test_cancel() {
    OrderBook book;
    book.addLimitOrder(1, Side::Buy, 100.0, 10);
    CHECK(book.openOrders() == 1);
    CHECK(book.cancelOrder(1) == true);
    CHECK(book.openOrders() == 0);
    CHECK(book.bestBid() == std::nullopt);
    // Canceling again should fail
    CHECK(book.cancelOrder(1) == false);
}

void test_partial_fill_across_multiple_levels() {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 100.0, 5);
    book.addLimitOrder(2, Side::Sell, 101.0, 5);
    book.addLimitOrder(3, Side::Sell, 102.0, 5);

    // Aggressive buy that sweeps three price levels
    auto trades = book.addLimitOrder(4, Side::Buy, 102.0, 12);
    CHECK(trades.size() == 3);
    CHECK(trades[0].price == 100.0);
    CHECK(trades[1].price == 101.0);
    CHECK(trades[2].price == 102.0);
    CHECK(trades[2].quantity == 2); // only 2 left of the 5 at the top level
    CHECK(book.askDepthAt(102.0) == 3);
}

int main() {
    test_basic_match();
    test_price_time_priority();
    test_price_improvement();
    test_no_cross_rests_in_book();
    test_cancel();
    test_partial_fill_across_multiple_levels();

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    } else {
        std::cout << failures << " test(s) failed.\n";
        return 1;
    }
}
