// The game's flood-fill work queue (VA 0x413780 reset+push, 0x4137B0 push, 0x413820 pop; ring head @0x4CB920, tail @0x4C943C).
// 512 entries of (a, b); FIFO; on overflow the oldest entry is silently dropped. VERIFIED by decompilation.
#pragma once
#include <array>
#include <cstdint>

namespace sc2k {

class TileQueue {
public:
    struct Item { int32_t a, b; };
    void reset(int a, int b) { head_ = tail_ = 0; push(a, b); }
    void push(int a, int b) {
        buf_[tail_] = {a, b};
        tail_ = (tail_ + 1) & 0x1FF;
        if (head_ == tail_) head_ = (tail_ + 1) & 0x1FF;
    }
    bool empty() const { return head_ == tail_; }
    Item pop() { Item it = buf_[head_]; head_ = (head_ + 1) & 0x1FF; return it; }
private:
    std::array<Item, 512> buf_{};
    uint16_t head_ = 0, tail_ = 0;
};

}  // namespace sc2k
