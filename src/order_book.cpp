#include "order_book.hpp"

namespace lob {

std::vector<Trade> OrderBook::matchBuy(uint64_t id, double price, uint64_t& qty) {
    std::vector<Trade> trades;
    // A buy crosses the book while the best ask is at or below the buy's limit price.
    while (qty > 0 && !asks_.empty()) {
        auto levelIt = asks_.begin();
        if (levelIt->first > price) break; // best ask too expensive, no more crosses

        auto& level = levelIt->second;
        while (qty > 0 && !level.orders.empty()) {
            Order& resting = level.orders.front();
            uint64_t fill = std::min(qty, resting.quantity);

            trades.push_back(Trade{ resting.id, id, levelIt->first, fill, nextSeq_++ });

            resting.quantity -= fill;
            qty -= fill;

            if (resting.quantity == 0) {
                locations_.erase(resting.id);
                level.orders.pop_front();
            }
        }
        if (level.orders.empty()) {
            asks_.erase(levelIt);
        }
    }
    return trades;
}

std::vector<Trade> OrderBook::matchSell(uint64_t id, double price, uint64_t& qty) {
    std::vector<Trade> trades;
    while (qty > 0 && !bids_.empty()) {
        auto levelIt = bids_.begin();
        if (levelIt->first < price) break; // best bid too low, no more crosses

        auto& level = levelIt->second;
        while (qty > 0 && !level.orders.empty()) {
            Order& resting = level.orders.front();
            uint64_t fill = std::min(qty, resting.quantity);

            trades.push_back(Trade{ resting.id, id, levelIt->first, fill, nextSeq_++ });

            resting.quantity -= fill;
            qty -= fill;

            if (resting.quantity == 0) {
                locations_.erase(resting.id);
                level.orders.pop_front();
            }
        }
        if (level.orders.empty()) {
            bids_.erase(levelIt);
        }
    }
    return trades;
}

void OrderBook::restOrder(uint64_t id, Side side, double price, uint64_t qty) {
    Order o{ id, side, price, qty, nextSeq_++ };
    if (side == Side::Buy) {
        auto& level = bids_[price];
        level.orders.push_back(o);
        auto it = std::prev(level.orders.end());
        locations_[id] = Location{ side, price, it };
    } else {
        auto& level = asks_[price];
        level.orders.push_back(o);
        auto it = std::prev(level.orders.end());
        locations_[id] = Location{ side, price, it };
    }
}

std::vector<Trade> OrderBook::addLimitOrder(uint64_t id, Side side, double price, uint64_t quantity) {
    uint64_t remaining = quantity;
    std::vector<Trade> trades = (side == Side::Buy)
        ? matchBuy(id, price, remaining)
        : matchSell(id, price, remaining);

    if (remaining > 0) {
        restOrder(id, side, price, remaining);
    }
    return trades;
}

bool OrderBook::cancelOrder(uint64_t id) {
    auto found = locations_.find(id);
    if (found == locations_.end()) return false;

    const Location& loc = found->second;
    if (loc.side == Side::Buy) {
        auto levelIt = bids_.find(loc.price);
        levelIt->second.orders.erase(loc.it);
        if (levelIt->second.orders.empty()) bids_.erase(levelIt);
    } else {
        auto levelIt = asks_.find(loc.price);
        levelIt->second.orders.erase(loc.it);
        if (levelIt->second.orders.empty()) asks_.erase(levelIt);
    }
    locations_.erase(found);
    return true;
}

std::optional<double> OrderBook::bestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<double> OrderBook::bestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

uint64_t OrderBook::bidDepthAt(double price) const {
    auto it = bids_.find(price);
    if (it == bids_.end()) return 0;
    uint64_t total = 0;
    for (const auto& o : it->second.orders) total += o.quantity;
    return total;
}

uint64_t OrderBook::askDepthAt(double price) const {
    auto it = asks_.find(price);
    if (it == asks_.end()) return 0;
    uint64_t total = 0;
    for (const auto& o : it->second.orders) total += o.quantity;
    return total;
}

} // namespace lob
