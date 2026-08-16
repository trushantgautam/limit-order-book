#include "order_book_v2.hpp"
#include <iostream>
#include <vector>

using namespace lob::v2;

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAILED: " << #cond << " at line " << __LINE__ << "\n"; \
        failures++; \
    } \
} while (0)

void test_basic_match() {
    OrderBook book;
    std::vector<Trade> t;
    book.addLimitOrder(1, Side::Sell, 100.0, 10, t);
    CHECK(t.empty());
    CHECK(book.bestAsk().value() == 100.0);

    t.clear();
    book.addLimitOrder(2, Side::Buy, 100.0, 5, t);
    CHECK(t.size() == 1);
    CHECK(t[0].quantity == 5);
    CHECK(t[0].price == 100.0);
    CHECK(book.bestBid() == std::nullopt);
}

void test_price_time_priority() {
    OrderBook book;
    std::vector<Trade> t;
    book.addLimitOrder(1, Side::Sell, 50.0, 5, t);
    book.addLimitOrder(2, Side::Sell, 50.0, 5, t);

    t.clear();
    book.addLimitOrder(3, Side::Buy, 50.0, 7, t);
    CHECK(t.size() == 2);
    CHECK(t[0].restingOrderId == 1);
    CHECK(t[0].quantity == 5);
    CHECK(t[1].restingOrderId == 2);
    CHECK(t[1].quantity == 2);
}

void test_price_improvement() {
    OrderBook book;
    std::vector<Trade> t;
    book.addLimitOrder(1, Side::Sell, 99.0, 10, t);
    t.clear();
    book.addLimitOrder(2, Side::Buy, 101.0, 10, t);
    CHECK(t.size() == 1);
    CHECK(t[0].price == 99.0);
}

void test_no_cross_rests() {
    OrderBook book;
    std::vector<Trade> t;
    book.addLimitOrder(1, Side::Sell, 105.0, 10, t);
    t.clear();
    book.addLimitOrder(2, Side::Buy, 100.0, 10, t);
    CHECK(t.empty());
    CHECK(book.bestBid().value() == 100.0);
    CHECK(book.bestAsk().value() == 105.0);
}

void test_cancel_and_pool_reuse() {
    OrderBook book;
    std::vector<Trade> t;
    book.addLimitOrder(1, Side::Buy, 100.0, 10, t);
    CHECK(book.cancelOrder(1) == true);
    CHECK(book.cancelOrder(1) == false);
    CHECK(book.openOrders() == 0);

    // Node should be recycled from the free list; behavior must stay correct.
    book.addLimitOrder(2, Side::Buy, 101.0, 7, t);
    CHECK(book.bestBid().value() == 101.0);
    t.clear();
    book.addLimitOrder(3, Side::Sell, 101.0, 7, t);
    CHECK(t.size() == 1);
    CHECK(t[0].restingOrderId == 2);
    CHECK(book.openOrders() == 0);
}

void test_cancel_middle_of_queue() {
    OrderBook book;
    std::vector<Trade> t;
    book.addLimitOrder(1, Side::Sell, 50.0, 5, t);
    book.addLimitOrder(2, Side::Sell, 50.0, 5, t);
    book.addLimitOrder(3, Side::Sell, 50.0, 5, t);

    // Cancel the middle order: intrusive unlink must relink 1 <-> 3.
    CHECK(book.cancelOrder(2) == true);

    t.clear();
    book.addLimitOrder(4, Side::Buy, 50.0, 10, t);
    CHECK(t.size() == 2);
    CHECK(t[0].restingOrderId == 1);
    CHECK(t[1].restingOrderId == 3);
}

void test_multi_level_sweep() {
    OrderBook book;
    std::vector<Trade> t;
    book.addLimitOrder(1, Side::Sell, 100.0, 5, t);
    book.addLimitOrder(2, Side::Sell, 101.0, 5, t);
    book.addLimitOrder(3, Side::Sell, 102.0, 5, t);

    t.clear();
    book.addLimitOrder(4, Side::Buy, 102.0, 12, t);
    CHECK(t.size() == 3);
    CHECK(t[0].price == 100.0);
    CHECK(t[1].price == 101.0);
    CHECK(t[2].price == 102.0);
    CHECK(t[2].quantity == 2);
}

int main() {
    test_basic_match();
    test_price_time_priority();
    test_price_improvement();
    test_no_cross_rests();
    test_cancel_and_pool_reuse();
    test_cancel_middle_of_queue();
    test_multi_level_sweep();

    if (failures == 0) {
        std::cout << "All v2 tests passed.\n";
        return 0;
    }
    std::cout << failures << " v2 test(s) failed.\n";
    return 1;
}
