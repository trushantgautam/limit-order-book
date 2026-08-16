// Small interactive-ish demo: seeds a book with resting orders, then sends
// in a few aggressive orders and prints the resulting trades and book state.
#include "order_book.hpp"
#include <iostream>

using namespace lob;

static void printTrades(const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "  TRADE  resting#" << t.restingOrderId
                  << " x incoming#" << t.incomingOrderId
                  << "  price=" << t.price
                  << "  qty=" << t.quantity << "\n";
    }
}

static void printBook(const OrderBook& book) {
    std::cout << "  book: best bid=";
    if (auto b = book.bestBid()) std::cout << *b; else std::cout << "-";
    std::cout << "  best ask=";
    if (auto a = book.bestAsk()) std::cout << *a; else std::cout << "-";
    std::cout << "  (" << book.bidLevels() << " bid levels / "
              << book.askLevels() << " ask levels / "
              << book.openOrders() << " open orders)\n";
}

int main() {
    OrderBook book;

    std::cout << "Resting sell 10 @ 100.25 (#1)\n";
    printTrades(book.addLimitOrder(1, Side::Sell, 100.25, 10));
    printBook(book);

    std::cout << "\nResting sell 5 @ 100.25 (#2, joins queue behind #1)\n";
    printTrades(book.addLimitOrder(2, Side::Sell, 100.25, 5));
    printBook(book);

    std::cout << "\nResting buy 8 @ 99.75 (#3, doesn't cross)\n";
    printTrades(book.addLimitOrder(3, Side::Buy, 99.75, 8));
    printBook(book);

    std::cout << "\nAggressive buy 12 @ 100.25 (#4, should fill #1 fully then #2 partially)\n";
    printTrades(book.addLimitOrder(4, Side::Buy, 100.25, 12));
    printBook(book);

    std::cout << "\nCancel order #3\n";
    std::cout << "  canceled=" << std::boolalpha << book.cancelOrder(3) << "\n";
    printBook(book);

    return 0;
}
