#include "order_book_v2.hpp"

namespace lob::v2 {

OrderBook::OrderBook(size_t reserveOrders) {
    pool_.reserve(reserveOrders);
    locations_.reserve(reserveOrders);
}

uint32_t OrderBook::allocNode(uint64_t id, uint64_t qty) {
    uint32_t idx;
    if (freeHead_ != NIL) {
        idx = freeHead_;
        freeHead_ = pool_[idx].next;
    } else {
        idx = static_cast<uint32_t>(pool_.size());
        pool_.emplace_back();
    }
    Node& n = pool_[idx];
    n.id = id;
    n.quantity = qty;
    n.prev = NIL;
    n.next = NIL;
    return idx;
}

void OrderBook::freeNode(uint32_t idx) {
    pool_[idx].next = freeHead_;
    freeHead_ = idx;
}

void OrderBook::pushBack(Level& level, uint32_t idx) {
    Node& n = pool_[idx];
    n.prev = level.tail;
    n.next = NIL;
    if (level.tail != NIL) pool_[level.tail].next = idx;
    level.tail = idx;
    if (level.head == NIL) level.head = idx;
}

void OrderBook::unlink(Level& level, uint32_t idx) {
    Node& n = pool_[idx];
    if (n.prev != NIL) pool_[n.prev].next = n.next; else level.head = n.next;
    if (n.next != NIL) pool_[n.next].prev = n.prev; else level.tail = n.prev;
}

template <typename Book>
void OrderBook::matchAgainst(Book& book, uint64_t id, double limit, uint64_t& qty,
                             bool aggressorIsBuy, std::vector<Trade>& out) {
    while (qty > 0 && !book.empty()) {
        auto levelIt = book.begin();
        double levelPrice = levelIt->first;
        // Buy crosses while best ask <= limit; sell crosses while best bid >= limit.
        if (aggressorIsBuy ? (levelPrice > limit) : (levelPrice < limit)) break;

        Level& level = levelIt->second;
        while (qty > 0 && level.head != NIL) {
            uint32_t idx = level.head;
            Node& resting = pool_[idx];
            uint64_t fill = std::min(qty, resting.quantity);

            out.push_back(Trade{ resting.id, id, levelPrice, fill });

            resting.quantity -= fill;
            qty -= fill;

            if (resting.quantity == 0) {
                locations_.erase(resting.id);
                unlink(level, idx);
                freeNode(idx);
            }
        }
        if (level.head == NIL) {
            book.erase(levelIt);
        }
    }
}

void OrderBook::restOrder(uint64_t id, Side side, double price, uint64_t qty) {
    uint32_t idx = allocNode(id, qty);
    if (side == Side::Buy) {
        pushBack(bids_[price], idx);
    } else {
        pushBack(asks_[price], idx);
    }
    locations_.emplace(id, Location{ side, price, idx });
}

void OrderBook::addLimitOrder(uint64_t id, Side side, double price, uint64_t quantity,
                              std::vector<Trade>& out) {
    uint64_t remaining = quantity;
    if (side == Side::Buy) {
        matchAgainst(asks_, id, price, remaining, /*aggressorIsBuy=*/true, out);
    } else {
        matchAgainst(bids_, id, price, remaining, /*aggressorIsBuy=*/false, out);
    }
    if (remaining > 0) {
        restOrder(id, side, price, remaining);
    }
}

bool OrderBook::cancelOrder(uint64_t id) {
    auto found = locations_.find(id);
    if (found == locations_.end()) return false;

    const Location& loc = found->second;
    if (loc.side == Side::Buy) {
        auto levelIt = bids_.find(loc.price);
        unlink(levelIt->second, loc.node);
        if (levelIt->second.head == NIL) bids_.erase(levelIt);
    } else {
        auto levelIt = asks_.find(loc.price);
        unlink(levelIt->second, loc.node);
        if (levelIt->second.head == NIL) asks_.erase(levelIt);
    }
    freeNode(loc.node);
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

} // namespace lob::v2
