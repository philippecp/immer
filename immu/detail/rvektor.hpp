
#pragma once

#include <immu/config.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_facade.hpp>

#include <cassert>
#include <memory>
#include <numeric>

namespace immu {
namespace detail {
namespace rvektor {

//#ifdef NDEBUG
//#define IMMU_RVEKTOR_TAGGED_NODES 0
//#else
#define IMMU_RVEKTOR_TAGGED_NODES 1
//#endif

template <int B, typename T=std::size_t>
constexpr T branches = T{1} << B;

template <int B, typename T=std::size_t>
constexpr T mask = branches<B, T> - 1;

template <typename T,
          int B,
          typename MemoryPolicy>
struct impl
{
    using heap_policy = typename MemoryPolicy::heap;
    using refcount    = typename MemoryPolicy::refcount;

    struct node_t : refcount::data
    {
#if IMMU_RVEKTOR_TAGGED_NODES
        enum
        {
            leaf_kind,
            inner_kind
        } kind;
#endif // IMMU_RVEKTOR_TAGGED_NODES
        unsigned count = 0u;
        union data_t
        {
            using  leaf_t = T[branches<B>];
            struct inner_t
            {
                node_t* elems[branches<B>];
                std::size_t* sizes;
            };

            inner_t inner;
            leaf_t  leaf;

            data_t() {}
            ~data_t() {}
        } data;

        unsigned& slots()
        {
            return count;
        }
        const unsigned& slots() const
        {
            return count;
        }

        std::size_t* sizes()
        {
            assert(kind == inner_kind);
            return data.inner.sizes;
        }

        const std::size_t* sizes() const
        {
            assert(kind == inner_kind);
            return data.inner.sizes;
        }

        node_t** inner()
        {
            assert(kind == inner_kind);
            return data.inner.elems;
        }

        const node_t* const* inner() const
        {
            assert(kind == inner_kind);
            return data.inner.elems;
        }

        T* leaf()
        {
            assert(kind == leaf_kind);
            return data.leaf;
        }

        const T* leaf() const
        {
            assert(kind == leaf_kind);
            return data.leaf;
        }
    };

    using heap = typename heap_policy::template apply<
        sizeof(node_t)
    >::type;

    std::size_t size;
    unsigned    shift;
    node_t*     root;
    node_t*     tail;

    static const impl empty;

    static node_t* make_inner()
    {
        auto p = new (heap::allocate(sizeof(node_t))) node_t;
        p->data.inner.sizes = nullptr;
#if IMMU_RVEKTOR_TAGGED_NODES
        p->kind = node_t::inner_kind;
#endif
        return p;
    }

    static node_t* make_inner_r()
    {
        auto p = new (heap::allocate(sizeof(node_t))) node_t;
        auto s = heap::allocate(sizeof(std::size_t) * branches<B>);
        p->data.inner.sizes = static_cast<std::size_t*>(s);
#if IMMU_RVEKTOR_TAGGED_NODES
        p->kind = node_t::inner_kind;
#endif
        return p;
    }

    static node_t* make_inner(node_t* x)
    {
        auto p = make_inner();
        p->inner() [0] = x;
        p->slots() = 1;
        return p;
    }

    static node_t* make_inner(node_t* x, node_t* y)
    {
        auto p = make_inner();
        p->inner() [0] = x;
        p->inner() [1] = y;
        p->slots() = 2;
        return p;
    }

    static node_t* make_inner_r(node_t* x)
    {
        auto p = make_inner_r();
        p->inner() [0] = x;
        p->slots() = 1;
        return p;
    }

    static node_t* make_inner_r(node_t* x, node_t* y)
    {
        auto p = make_inner_r();
        p->inner() [0] = x;
        p->inner() [1] = y;
        p->slots() = 2;
        return p;
    }

    static node_t* make_inner_r(node_t* x, std::size_t xs, node_t* y)
    {
        auto p = make_inner_r();
        p->inner() [0] = x;
        p->inner() [1] = y;
        p->slots() = 2;
        return p;
    }

    static node_t* make_leaf()
    {
        auto p = new (heap::allocate(sizeof(node_t))) node_t;
#if IMMU_RVEKTOR_TAGGED_NODES
        p->kind = node_t::leaf_kind;
#endif
        return p;
    }

    template <typename U>
    static node_t* make_leaf(U&& x)
    {
        auto p = make_leaf();
        new (p->leaf()) T{ std::forward<U>(x) };
        p->slots() = 1;
        return p;
    }

    static node_t* copy_inner(node_t* src, int n)
    {
        assert(src->kind == node_t::inner_kind);
        auto dst = make_inner();
        for (auto i = src->inner(), e = i + n; i != e; ++i)
            refcount::inc(*i);
        std::uninitialized_copy(src->inner(), src->inner() + n, dst->inner());
        dst->slots() = n;
        return dst;
    }

    static node_t* copy_inner_r(node_t* src, int n)
    {
        assert(src->kind == node_t::inner_kind);
        auto dst = make_inner();
        for (auto i = src->inner(), e = i + n; i != e; ++i)
            refcount::inc(*i);
        std::uninitialized_copy(src->inner(), src->inner() + n, dst->inner());
        std::uninitialized_copy(src->sizes(), src->sizes() + n, dst->sizes());
        dst->slots() = n;
        return dst;
    }

    static node_t* copy_leaf(node_t* src, int n)
    {
        assert(src->kind == node_t::leaf_kind);
        auto dst = make_leaf();
        std::uninitialized_copy(src->leaf(), src->leaf() + n, dst->leaf());
        dst->slots() = n;
        return dst;
    }

    static node_t* copy_leaf(node_t* src1, int n1,
                             node_t* src2, int n2)
    {
        assert(src1->kind == node_t::leaf_kind);
        assert(src2->kind == node_t::leaf_kind);
        auto dst = make_leaf();
        std::uninitialized_copy(
            src1->leaf(), src1->leaf() + n1, dst->leaf());
        std::uninitialized_copy(
            src2->leaf(), src2->leaf() + n2, dst->leaf() + n1);
        dst->slots() = n1 + n2;
        return dst;
    }

    static node_t* copy_leaf(node_t* src, int idx, int n)
    {
        assert(src->kind == node_t::leaf_kind);
        auto dst = make_leaf();
        std::uninitialized_copy(
            src->leaf() + idx, src->leaf() + idx + n, dst->leaf());
        dst->slots() = n - idx;
        return dst;
    }

    template <typename U>
    static node_t* copy_leaf_emplace(node_t* src, int n, U&& x)
    {
        auto dst = copy_leaf(src, n);
        new (dst->leaf() + n) T{std::forward<U>(x)};
        dst->slots() = n + 1;
        return dst;
    }

    static void delete_inner(node_t* p)
    {
        assert(p->kind == node_t::inner_kind);
        auto sizes = p->sizes();
        if (sizes) heap::deallocate(sizes);
        heap::deallocate(p);
    }

    static void delete_leaf(node_t* p, int n)
    {
        assert(p->kind == node_t::leaf_kind);
        for (auto i = p->leaf(), e = i + n; i != e; ++i)
            i->~T();
        heap::deallocate(p);
    }

    impl(std::size_t sz, unsigned sh, node_t* r, node_t* t)
        : size{sz}, shift{sh}, root{r}, tail{t}
    {}

    impl(const impl& other)
        : impl{other.size, other.shift, other.root, other.tail}
    {
        inc();
    }

    impl(impl&& other)
        : impl{empty}
    {
        swap(*this, other);
    }

    impl& operator=(const impl& other)
    {
        auto next = other;
        swap(*this, next);
        return *this;
    }

    impl& operator=(impl&& other)
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(impl& x, impl& y)
    {
        using std::swap;
        swap(x.size,  y.size);
        swap(x.shift, y.shift);
        swap(x.root,  y.root);
        swap(x.tail,  y.tail);
    }

    ~impl()
    {
        //dec();
    }

    void inc() const
    {
        refcount::inc(root);
        refcount::inc(tail);
    }

    struct dec_traversal
    {
        bool predicate(node_t* p)
        { return refcount::dec(p); }

        void visit_inner(node_t* p)
        { delete_inner(p); }

        void visit_leaf(node_t* p, unsigned n)
        { delete_leaf(p, n); }
    };

    void dec() const
    {
        traverse(dec_traversal{});
    }

    auto tail_size() const
    {
        return tail->slots();
    }

    auto tail_offset() const
    {
        return size - tail->slots();
    }

    template <typename Step, typename State>
    struct reduce_traversal
    {
        Step fn;
        State acc;

        bool predicate(node_t*) { return true; }
        void visit_inner(node_t* n) {}

        void visit_leaf(node_t* n, unsigned elems)
        {
            acc = std::accumulate(n->leaf(),
                                  n->leaf() + elems,
                                  acc,
                                  fn);
        }
    };

    template <typename Step, typename State>
    State reduce(Step step, State init) const
    {
        auto t = reduce_traversal<Step, State>{step, init};
        traverse(t);
        return t.acc;
    }

    template <typename Traversal>
    void traverse_node_relaxed(Traversal&& t,
                               node_t* node,
                               unsigned level,
                               std::size_t size) const
    {
        assert(level > 0);
        auto s = node->sizes();
        if (s) {
            if (t.predicate(node)) {
                auto count = node->slots();
                auto next = level - B;
                if (next == 0) {
                    auto c = node->inner();
                    auto last_s = 0u;
                    for (auto i = 0u; i < count; ++i) {
                        if (t.predicate(c[i]))
                            t.visit_leaf(c[i], s[i] - last_s);
                        last_s = s[i];
                    }
                } else {
                    auto c = node->inner();
                    auto last_s = 0u;
                    for (auto i = 0u; i < count; ++i) {
                        traverse_node_relaxed(t, c[i], next, s[i] - last_s);
                        last_s = s[i];
                    }
                }
            }
        } else {
            traverse_node_last(t, node, level, size);
        }
    }

    template <typename Traversal>
    void traverse_node_last(Traversal&& t,
                            node_t* node,
                            unsigned level,
                            std::size_t size) const
    {
        assert(level > 0);
        assert(size > branches<B>);
        if (t.predicate(node)) {
            auto next = level - B;
            auto last = ((size - 1) >> level) & mask<B>;
            if (next == 0) {
                for (auto i = node->inner(), e = i + last + 1; i != e; ++i)
                    if (t.predicate(*i))
                        t.visit_leaf(*i, branches<B>);
            } else {
                auto i = node->inner();
                for (auto e = i + last; i != e; ++i)
                    traverse_node_full(t, *i, next);
                traverse_node_last(t, *i, next);
            }
            t.visit_inner(node);
        }
    }

    template <typename Traversal>
    void traverse_node_full(Traversal&& t, node_t* node, unsigned level) const
    {
        assert(level > 0);
        assert(size > branches<B>);
        if (t.predicate(node)) {
            auto next = level - B;
            if (next == 0) {
                for (auto i = node->inner(), e = i + branches<B>; i != e; ++i)
                    if (t.predicate(*i))
                        t.visit_leaf(*i, branches<B>);
            } else {
                for (auto i = node->inner(), e = i + branches<B>; i != e; ++i)
                    traverse_node_full(t, *i, next);
            }
            t.visit_inner(node);
        }
    }

    template <typename Traversal>
    void traverse(Traversal&& t) const
    {
        if (size > branches<B>)
            traverse_node_relaxed(t, root, shift, tail_offset());
        else if (t.predicate(root))
            t.visit_inner(root);
        if (t.predicate(tail))
            t.visit_leaf(tail, tail_size());
    }

    const T* array_for(std::size_t& index) const
    {
        assert(index < size);
        auto tail_off = tail_offset();
        if (index >= tail_offset()) {
            index -= tail_off;
            return tail->leaf();
        } else {
            auto node = root;
            for (auto level = shift; level; level -= B) {
                auto sizes = node->sizes();
                if (sizes) {
                    auto node_index = (index >> level) & mask<B>; // todo
                    while (sizes[node_index] <= index) ++node_index;
                    if (node_index) index -= sizes[node_index - 1];
                    node = node->inner() [node_index];
                } else {
                    // balanced subtree
                    do {
                        node = node->inner() [(index >> level) & mask<B>];
                    } while (level -= B);
                    break;
                }
            }
            return node->leaf();
        }
    }

    node_t* make_path(unsigned level, node_t* node) const
    {
        assert(node->kind == node_t::leaf_kind);
        return level == 0
            ? node
            : make_inner(make_path(level - B, std::move(node)));
    }

    node_t* push_tail(unsigned level,
                      node_t* parent,
                      node_t* tail) const
    {
        auto sizes = parent->sizes();
        if (sizes) {
            auto idx = parent->slots();
            auto children   = idx ? sizes[idx] - sizes[idx - 1] : sizes[idx];
            auto new_idx    = children == 1 << level ? idx + 1 : idx;
            auto new_parent = copy_inner_r(parent, new_idx);
            new_parent->inner()[new_idx] =
                level == B       ? tail :
                idx == new_idx   ? push_tail(level - B, parent->inner()[idx], tail)
                /* otherwise */  : make_path(level - B, tail);
            new_parent->sizes()[new_idx] = sizes[idx] + branches<B>;
            new_parent->slots() = parent->slots() + (idx != new_idx);
            assert(new_parent->slots() <= branches<B>);
            return new_parent;
        } else {
            auto idx        = ((size - branches<B> - 1) >> level) & mask<B>;
            auto new_idx    = ((size - 1) >> level) & mask<B>;
            auto new_parent = copy_inner(parent, new_idx);
            new_parent->inner()[new_idx] =
                level == B       ? tail :
                idx == new_idx   ? push_tail(level - B, parent->inner()[idx], tail)
                /* otherwise */  : make_path(level - B, tail);
            new_parent->slots() = parent->slots() + (idx != new_idx);
            assert(new_parent->slots() <= branches<B>);
            return new_parent;
        }
    }

    bool is_overflow() const
    {
        auto node  = root;
        auto sizes = node->sizes();
        auto count = tail_offset();
        if (!sizes)
            return (count >> B) >= (1u << shift);
        for (auto level = shift; level > B; level -= B) {
            if (!sizes)
                return (count >> B) >= (1u << level);
            auto slots = node->slots();
            if (slots != branches<B>) return false;
            node  = node->inner() [slots - 1];
            sizes = node->sizes();
            count = (sizes[branches<B> - 1]  -
                     sizes[branches<B> - 2]) + branches<B>;
        }
        return true;
    }

    std::pair<unsigned, node_t*> push_tail_into_root(node_t* tail) const
    {
        IMMU_TRACE("push_tail_into_root: " << is_overflow());
        if (is_overflow()) {
            refcount::inc(root);
            auto new_path = make_path(shift, tail);
            auto new_root = root->sizes()
                ? make_inner_r(root,
                               root->sizes() [branches<B> - 1],
                               new_path)
                : make_inner(root, new_path);
            if (new_root->sizes())
                set_sizes(new_root, shift + B);
            return { shift + B, new_root };
        } else {
            auto new_root = push_tail(shift, root, tail);
            return { shift, new_root };
        }
    }

    impl push_back(T value) const
    {
        auto ts = tail_size();
        if (ts < branches<B>) {
            auto new_tail = copy_leaf_emplace(tail, ts, std::move(value));
            refcount::inc(root);
            return { size + 1, shift, root, new_tail };
        } else {
            auto new_tail = make_leaf(std::move(value));
            auto new_root = push_tail_into_root(tail);
            refcount::inc(tail);
            return { size + 1, new_root.first, new_root.second, new_tail };
        }
    }

    const T& get(std::size_t index) const
    {
        auto arr = array_for(index);
        return arr [index & mask<B>];
    }

    template <typename FnT>
    impl update(std::size_t idx, FnT&& fn) const
    {
        auto tail_off = tail_offset();
        if (idx >= tail_off) {
            auto new_tail  = copy_leaf(tail, size - tail_off);
            auto& item     = new_tail->leaf() [idx & mask<B>];
            auto new_value = std::forward<FnT>(fn) (std::move(item));
            item = std::move(new_value);
            refcount::inc(root);
            return { size, shift, root, new_tail };
        } else {
            refcount::inc(tail);
            auto new_root = do_update(shift,
                                      root,
                                      idx,
                                      tail_offset(),
                                      std::forward<FnT>(fn));
            return { size, shift, new_root, tail};
        }
    }

    template <typename FnT>
    node_t* do_update(unsigned level,
                      node_t* node,
                      std::size_t idx,
                      std::size_t count,
                      FnT&& fn) const
    {
        auto sizes = node->sizes();
        if (level == 0) {
            auto new_node  = copy_leaf(node, branches<B>);
            auto& item     = new_node->leaf() [idx & mask<B>];
            auto new_value = std::forward<FnT>(fn) (std::move(item));
            item = std::move(new_value);
            return new_node;
        } else if (sizes) {
            auto offset   = (idx >> level) & mask<B>;
            while (sizes[offset] <= idx) ++offset;
            auto new_node = copy_inner_r(node, node->slots());
            refcount::dec_unsafe(node->inner()[offset]);
            new_node->inner()[offset] =
                do_update(level - B,
                          node->inner()[offset],
                          offset ? idx - sizes[offset - 1] : idx,
                          sizes[idx],
                          std::forward<FnT>(fn));
            return new_node;
        } else {
            return do_update_last(level, node, idx, count,
                                  std::forward<FnT>(fn));
        }
    }

    template <typename FnT>
    node_t* do_update_full(unsigned level,
                           node_t* node,
                           std::size_t idx,
                           FnT&& fn) const
    {
        if (level == 0) {
            auto new_node  = copy_leaf(node, branches<B>);
            auto& item     = new_node->leaf() [idx & mask<B>];
            auto new_value = std::forward<FnT>(fn) (std::move(item));
            item = std::move(new_value);
            return new_node;
        } else {
            auto offset   = (idx >> level) & mask<B>;
            auto new_node = copy_inner(node, branches<B>);
            refcount::dec_unsafe(node->inner()[offset]);
            new_node->inner()[offset] =
                do_update_full(level - B, node->inner()[offset], idx,
                               std::forward<FnT>(fn));
            return new_node;
        }
    }

    template <typename FnT>
    node_t* do_update_last(unsigned level,
                           node_t* node,
                           std::size_t idx,
                           std::size_t count,
                           FnT&& fn) const
    {
        if (level == 0) {
            auto new_node  = copy_leaf(node, branches<B>);
            auto& item     = new_node->leaf() [idx & mask<B>];
            auto new_value = std::forward<FnT>(fn) (std::move(item));
            item = std::move(new_value);
            return new_node;
        } else {
            auto offset     = (idx >> level) & mask<B>;
            auto end_offset = ((count - 1) >> level) & mask<B>;
            auto new_node   = copy_inner(node, end_offset + 1);
            refcount::dec_unsafe(node->inner()[offset]);
            new_node->inner()[offset] =
                offset == end_offset
                ? do_update_last(level - B, node->inner()[offset], idx, count,
                                 std::forward<FnT>(fn))
                : do_update_full(level - B, node->inner()[offset], idx,
                                 std::forward<FnT>(fn));
            return new_node;
        }
    }

    impl assoc(std::size_t idx, T value) const
    {
        return update(idx, [&] (auto&&) {
                return std::move(value);
            });
    }

    impl concat(const impl& r) const
    {
        if (size == 0)
            return r;
        else if (r.size == 0)
            return *this;
        else if (r.size <= branches<B>) {
            // just concat the tail, similar to push_back
            auto ts = tail_size();
            if (ts == branches<B>) {
                auto new_tail = r.tail;
                refcount::inc(new_tail);
                auto new_root = push_tail_into_root(tail);
                refcount::inc(tail);
                return { size + r.size, new_root.first, new_root.second, new_tail };
            } else if (ts + r.size <= branches<B>) {
                auto new_tail = copy_leaf(tail, ts, r.tail, r.size);
                refcount::inc(root);
                return { size + r.size, shift, root, new_tail };
            } else {
                auto remaining = branches<B> - ts;
                auto add_tail  = copy_leaf(tail, ts, r.tail, remaining);
                auto new_tail  = copy_leaf(r.tail, remaining, r.size);
                auto new_root = push_tail_into_root(add_tail);
                return { size + r.size, new_root.first, new_root.second, new_tail };
            }
        } else {
            auto left_root = push_tail_into_root(tail);
            auto new_root = concat_sub_tree(left_root.first, left_root.second,
                                            r.shift, r.root,
                                            true);
            auto new_shift = compute_shift(new_root);
            IMMU_TRACE("new_shift: " << new_shift);
            set_sizes(new_root, new_shift);
            assert(compute_size(new_root, new_shift) == size + r.tail_offset());
            return { size + r.size, new_shift, new_root, r.tail };
        }
    }

    friend node_t* concat_sub_tree(
        unsigned lshift, node_t* lnode,
        unsigned rshift, node_t* rnode,
        bool is_top)
    {
        IMMU_TRACE("concat_sub_tree: " << lshift << " <> " << rshift);
        if (lshift > rshift) {
            auto lidx  = lnode->slots() - 1;
            auto cnode = concat_sub_tree(lshift - B, lnode->inner() [lidx],
                                         rshift, rnode,
                                         false);
            return rebalance(lnode, cnode, nullptr, lshift, is_top);
        } else if (lshift < rshift) {
            auto cnode = concat_sub_tree(lshift, lnode,
                                         rshift - B, rnode->inner() [0],
                                         false);
            return rebalance(nullptr, cnode, rnode, rshift, is_top);
        } else if (lshift == 0) {
            auto lslots = lnode->slots();
            auto rslots = rnode->slots();
            IMMU_TRACE("ls: " << lslots << " " << rslots);
            if (is_top && lslots + rslots <= branches<B>)
                return copy_leaf(lnode, lslots, rnode, rslots);
            else
                return make_inner_r(lnode, rnode);
        } else {
            auto lidx  = lnode->slots() - 1;
            auto cnode = concat_sub_tree(lshift - B, lnode->inner() [lidx],
                                         rshift - B, rnode->inner() [0],
                                         false);
            return rebalance(lnode, cnode, rnode, lshift, is_top);
        }
    }

    friend node_t* rebalance(node_t* lnode,
                             node_t* cnode,
                             node_t* rnode,
                             unsigned shift,
                             bool is_top)
    {
        assert(cnode);
        assert(lnode || rnode);

        // list all children and their slot counts
        node_t* all [3 * branches<B>];
        auto all_n = 0u;
        if (lnode) {
            auto slots = lnode->slots() - 1;
            IMMU_TRACE("l-slots: " << slots);
            std::copy(lnode->inner(), lnode->inner() + slots, all + all_n);
            all_n += slots;
        } {
            auto slots = cnode->slots();
            IMMU_TRACE("c-slots: " << slots);
            std::copy(cnode->inner(), cnode->inner() + slots, all + all_n);
            all_n += slots;
        } if (rnode) {
            auto slots = rnode->slots() - 1;
            IMMU_TRACE("r-slots: " << slots);
            std::copy(rnode->inner() + 1, rnode->inner() + slots + 1, all + all_n);
            all_n += slots;
        }

        IMMU_TRACE("all_n: " << all_n);
        unsigned all_slots [3 * branches<B>];
        auto total_all_slots = 0u;
        for (auto i = 0u; i < all_n; ++i) {
            auto sub_slots = all[i]->slots();
            all_slots[i] = sub_slots;
            total_all_slots += sub_slots;
        }

        // plan rebalance
        const auto RRB_EXTRAS = 2;
        const auto RRB_INVARIANT = 1;

        IMMU_TRACE("all_slots: " <<
                   pretty_print_array(all_slots, all_n));

        const auto optimal_slots = ((total_all_slots - 1) / branches<B>) + 1;
        IMMU_TRACE("optimal_slots: " << optimal_slots <<
                   " // " << total_all_slots);

        auto shuffled_n = all_n;
        auto i = 0u;
        while (shuffled_n >= optimal_slots + RRB_EXTRAS) {
            // skip ok nodes
            while (all_slots[i] > branches<B> - RRB_INVARIANT) i++;

            // short node, redistribute
            auto remaining = all_slots[i];
            do {
                auto min_size = std::min(remaining + all_slots[i+1],
                                         branches<B, unsigned>);
                all_slots[i] = min_size;
                remaining += all_slots[i+1] - min_size;
                ++i;
            } while (remaining > 0);

            // remove node
            std::move(all_slots + i + 1, all_slots + shuffled_n, all_slots + i);
            --shuffled_n;
            --i;
        }

        IMMU_TRACE("suffled_all_slots: " <<
                   pretty_print_array(all_slots, shuffled_n));

        // actually rebalance the nodes
        if (shift == B) {
            auto from_i = 0u;
            auto from_offset = 0u;
            for (auto i = 0; i < shuffled_n; ++i) {
                auto new_slots  = all_slots[i];
                auto from_node  = all[from_i];
                auto from_data  = from_node->leaf();
                auto from_slots = from_node->slots();
                if (from_offset == 0 && new_slots == from_slots) {
                    ++from_i;
                    all[i] = from_node;
                } else {
                    auto new_node = all[i] = make_leaf();
                    auto new_data = new_node->leaf();
                    auto cur_slots = 0u;
                    new_node->slots() = new_slots;
                    while (cur_slots < new_slots) {
                        if (new_slots - cur_slots >= from_slots - from_offset) {
                            std::uninitialized_copy(
                                from_data + from_offset,
                                from_data + from_slots,
                                new_data + cur_slots);
                            cur_slots += from_slots - from_offset;
                            ++ from_i;
                            from_node   = all[from_i];
                            from_slots  = from_node->slots();
                            from_data   = from_node->leaf();
                            from_offset = 0;
                        } else {
                            auto to_copy = new_slots - cur_slots;
                            std::uninitialized_copy(
                                from_data + from_offset,
                                from_data + from_offset + to_copy,
                                new_data + cur_slots);
                            from_offset += to_copy;
                            assert(cur_slots + to_copy == new_slots);
                            cur_slots = new_slots;
                        }
                    }
                }
            }
        } else {
            auto from_i = 0u;
            auto from_offset = 0u;
            for (auto i = 0; i < shuffled_n; ++i) {
                auto new_slots  = all_slots[i];
                auto from_node  = all[from_i];
                auto from_data  = from_node->inner();
                auto from_slots = from_node->slots();
                if (from_offset == 0 && new_slots == from_slots) {
                    ++from_i;
                    all[i] = from_node;
                } else {
                    auto new_node = all[i] = make_inner_r();
                    auto new_data = new_node->inner();
                    auto cur_slots = 0u;
                    new_node->slots() = new_slots;
                    while (cur_slots < new_slots) {
                        if (new_slots - cur_slots >= from_slots - from_offset) {
                            std::uninitialized_copy(
                                from_data + from_offset,
                                from_data + from_slots,
                                new_data + cur_slots);
                            cur_slots += from_slots - from_offset;
                            ++ from_i;
                            from_node   = all[from_i];
                            from_data   = from_node->inner();
                            from_slots  = from_node->slots(); // crash?
                            from_offset = 0;
                        } else {
                            auto to_copy = new_slots - cur_slots;
                            std::uninitialized_copy(
                                from_data + from_offset,
                                from_data + from_offset + to_copy,
                                new_data + cur_slots);
                            from_offset += to_copy;
                            assert(cur_slots + to_copy == new_slots);
                            cur_slots = new_slots;
                        }
                    }
                    set_sizes(new_node, shift - B);
                }
            }
        }

        // make a node and return it
        IMMU_TRACE("shuffled_n: " << shuffled_n << "  is_top: " << is_top);
        if (shuffled_n <= branches<B>) {
            auto node = make_inner_r();
            std::copy(all, all + shuffled_n, node->inner());
            node->slots() = shuffled_n;
            set_sizes(node, shift);
            if (is_top)
                return node;
            else
                return make_inner_r(node); // set_sizes?
        } else {
            assert(shuffled_n <= 2 * branches<B>);
            auto node1 = make_inner_r(); {
                std::copy(all, all + branches<B>, node1->inner());
                node1->slots() = branches<B>;
                set_sizes(node1, shift);
            }
            auto node2 = make_inner_r(); {
                std::copy(all + branches<B>, all + shuffled_n, node2->inner());
                node2->slots() = shuffled_n - branches<B>;
                set_sizes(node2, shift);
            }
            return make_inner_r(node1, node2); // set_sizes above
        }
    }

    friend void set_sizes(node_t* node, unsigned shift)
    {
        auto acc = 0u;
        for (auto i = 0; i < node->slots(); ++i) {
            acc += compute_size(node->inner() [i], shift - B);
            node->sizes() [i] = acc;
        }
    }

    friend std::size_t compute_size(node_t* node, unsigned shift)
    {
        if (shift == 0) {
            return node->slots();
        } else {
            auto sizes = node->sizes();
            auto slots = node->slots();
            return sizes
                ? sizes[node->slots() - 1]
                : ((slots - 1) << shift)
                /* */ + compute_size(node->inner() [slots - 1],
                                     shift - B);
        }
    }

    friend unsigned compute_shift(node_t* node)
    {
        if (node->kind == node_t::leaf_kind)
            return 0;
        else
            return B + compute_shift(node->inner() [0]);
    }
};

template <typename T, int B, typename MP>
const impl<T, B, MP> impl<T, B, MP>::empty = {
    0,
    B,
    make_inner(),
    make_leaf()
};

template <typename T, int B, typename MP>
struct iterator : boost::iterator_facade<
    iterator<T, B, MP>,
    T,
    boost::random_access_traversal_tag,
    const T&>
{
    struct end_t {};

    iterator() = default;

    iterator(const impl<T, B, MP>& v)
        : v_    { &v }
        , i_    { 0 }
        , base_ { 0 }
          //, curr_ { v.array_for(0) } // WIP
    {
    }

    iterator(const impl<T, B, MP>& v, end_t)
        : v_    { &v }
        , i_    { v.size }
        , base_ { i_ - (i_ & mask<B>) }
          //, curr_ { v.array_for(i_ - 1) + (i_ - base_) } // WIP
    {}

private:
    friend class boost::iterator_core_access;

    const impl<T, B, MP>* v_;
    std::size_t       i_;
    std::size_t       base_;
    const T*          curr_;

    void increment()
    {
        assert(i_ < v_->size);
        ++i_;
        if (i_ - base_ < branches<B>) {
            ++curr_;
        } else {
            base_ += branches<B>;
            curr_ = v_->array_for(i_);
        }
    }

    void decrement()
    {
        assert(i_ > 0);
        --i_;
        if (i_ >= base_) {
            --curr_;
        } else {
            base_ -= branches<B>;
            curr_ = v_->array_for(i_) + (branches<B> - 1);
        }
    }

    void advance(std::ptrdiff_t n)
    {
        assert(n <= 0 || i_ + static_cast<std::size_t>(n) <= v_->size);
        assert(n >= 0 || static_cast<std::size_t>(-n) <= i_);

        i_ += n;
        if (i_ <= base_ && i_ - base_ < branches<B>) {
            curr_ += n;
        } else {
            base_ = i_ - (i_ & mask<B>);
            curr_ = v_->array_for(i_) + (i_ - base_);
        }
    }

    bool equal(const iterator& other) const
    {
        return i_ == other.i_;
    }

    std::ptrdiff_t distance_to(const iterator& other) const
    {
        return other.i_ > i_
            ?   static_cast<std::ptrdiff_t>(other.i_ - i_)
            : - static_cast<std::ptrdiff_t>(i_ - other.i_);
    }

    const T& dereference() const
    {
        return *curr_;
    }
};

} /* namespace rvektor */
} /* namespace detail */
} /* namespace immu */
