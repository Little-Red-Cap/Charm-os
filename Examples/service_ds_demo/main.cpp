#include <cstdio>
#include <cstdint>

import util.core;
import service.fifo;
import service.queue;
import service.small_vector;
import service.linked_list;
import service.lru_cache;
import service.fixed_hash_map;
import service.rb_tree;

int main() {
    service::Fifo<int, 4> fifo{};
    (void)fifo.push(1);
    (void)fifo.push(2);
    auto f = fifo.pop();
    std::printf("[fifo] %d\n", f.has_value() ? f.value() : -1);

    service::Queue<int, 4> queue{};
    (void)queue.push(7);
    auto q = queue.pop();
    std::printf("[queue] %d\n", q.has_value() ? q.value() : -1);

    service::SmallVector<int, 4> vec{};
    (void)vec.push_back(3);
    (void)vec.push_back(4);
    std::printf("[small_vector] size=%llu last=%d\n",
                static_cast<unsigned long long>(vec.size()),
                vec[vec.size() - 1]);

    service::LinkedList<int, 6> list{};
    const auto a = list.push_back(10);
    const auto b = list.push_front(5);
    (void)a;
    (void)b;
    int out = 0;
    (void)list.pop_front(&out);
    std::printf("[linked_list] pop=%d size=%llu\n", out,
                static_cast<unsigned long long>(list.size()));

    service::LruCache<int, int, 4> lru{};
    (void)lru.put(1, 11);
    (void)lru.put(2, 22);
    int lv = 0;
    (void)lru.get(1, lv);
    std::printf("[lru] get(1)=%d\n", lv);

    service::FixedHashMap<int, int, 8> map{};
    (void)map.insert(4, 44);
    auto* mv = map.find(4);
    std::printf("[hash] 4=%d\n", mv ? *mv : -1);

    service::RbTree<int, int, 8> tree{};
    (void)tree.insert(2, 20);
    (void)tree.insert(1, 10);
    (void)tree.insert(3, 30);
    (void)tree.erase(2);
    std::printf("[rb_tree] ");
    tree.inorder([](int k, int v) noexcept {
        std::printf("%d:%d ", k, v);
    });
    std::printf("\n");
    return 0;
}
