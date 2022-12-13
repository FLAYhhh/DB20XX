/* Masstree
 * Eddie Kohler, Yandong Mao, Robert Morris
 * Copyright (c) 2012-2014 President and Fellows of Harvard College
 * Copyright (c) 2012-2014 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Masstree LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Masstree LICENSE file; the license in that file
 * is legally binding.
 */
#ifndef MASSTREE_SCAN_HH
#define MASSTREE_SCAN_HH
#include "masstree_tcursor.hh"
#include "masstree_struct.hh"
namespace Masstree {


template <typename P>
class scanstackelt {
  public:
    typedef leaf<P> leaf_type;
    typedef typename leaf_type::leafvalue_type leafvalue_type;
    typedef typename leaf_type::bound_type bound_type;
    typedef typename P::ikey_type ikey_type;
    typedef key<ikey_type> key_type;
    typedef typename leaf_type::permuter_type permuter_type;
    typedef typename P::threadinfo_type threadinfo;
    typedef typename node_base<P>::nodeversion_type nodeversion_type;

    enum ScanState {
      scan_emit,
      scan_find_next,
      scan_down,
      scan_up,
      scan_retry,
      scan_no_value
    };

    scanstackelt() {}

    leaf<P>* node() const {
        return n_;
    }
    typename nodeversion_type::value_type full_version_value() const {
        return (v_.version_value() << permuter_type::size_bits) + perm_.size();
    }
    int size() const {
        return perm_.size();
    }
    permuter_type permutation() const {
        return perm_;
    }
    int operator()(const key_type &k, const scanstackelt<P> &n, int p) {
        return n.n_->compare_key(k, p);
    }

    /* 检查scan状态 */
    bool no_value() {
        if (state_ == scan_no_value)
            return true;
        else
            return false;
    }

    /* 获取当前scan状态下的entry */
    typename leafvalue_type::value_type
    get_value() {
      return entry_.value();
    }

    // FIXME: reset every thing
    void reset() {
      root_ = nullptr;
      scan_count_ = 0;
    }

private:
    // 定义这个union是为了让keybuf是sizeof(ikey_type)的整数倍
    union KeyBuf{
        ikey_type x[(MASSTREE_MAXKEYLEN + sizeof(ikey_type) - 1)/sizeof(ikey_type)];
        char s[MASSTREE_MAXKEYLEN];
    };



  private:
    node_base<P>* root_;
    leaf<P>* n_;
    nodeversion_type v_;
    permuter_type perm_;
    int ki_;
    small_vector<node_base<P>*, 2> node_stack_;
    ScanState state_;
    uint32_t scan_count_ = 0;
    KeyBuf keybuf_;
    key_type ka_;
    leafvalue_type entry_;



    template <typename H>
    ScanState find_initial(H& helper, key_type& ka, bool emit_equal,
                     leafvalue_type& entry, threadinfo& ti);
    template <typename H>
    ScanState find_retry(H& helper, key_type& ka, threadinfo& ti);
    template <typename H>
    ScanState find_next(H& helper, key_type& ka, leafvalue_type& entry);

    int kp() const {
        if (unsigned(ki_) < unsigned(perm_.size()))
            return perm_[ki_];
        else
            return -1;
    }

    template <typename PX> friend class basic_table;
};

struct forward_scan_helper {
    bool initial_ksuf_match(int ksuf_compare, bool emit_equal) const {
        return ksuf_compare > 0 || (ksuf_compare == 0 && emit_equal);
    }
    template <typename K> bool is_duplicate(const K &k,
                                            typename K::ikey_type ikey,
                                            int keylenx) const {
        return k.compare(ikey, keylenx) >= 0;
    }
    template <typename K, typename N> int lower(const K &k, const N *n) const {
        return N::bound_type::lower_by(k, *n, *n).i;
    }
    template <typename K, typename N>
    key_indexed_position lower_with_position(const K &k, const N *n) const {
        return N::bound_type::lower_by(k, *n, *n);
    }
    void mark_key_complete() const {
    }
    int next(int ki) const {
        return ki + 1;
    }
    template <typename N, typename K>
    N *advance(const N *n, const K &) const {
        return n->safe_next();
    }
    template <typename N, typename K>
    typename N::nodeversion_type stable(const N *n, const K &) const {
        return n->stable();
    }
    template <typename K> void shift_clear(K &ka) const {
        ka.shift_clear();
    }
};

struct reverse_scan_helper {
    // We run ki backwards, referring to perm.size() each time through,
    // because inserting elements into a node need not bump its version.
    // Therefore, if we decremented ki, starting from a node's original
    // size(), we might miss some concurrently inserted keys!
    // Also, a node's size might change DURING a lower_bound operation.
    // The "backwards" ki must be calculated using the size taken by the
    // lower_bound, NOT some later size() (which might be bigger or smaller).
    reverse_scan_helper()
        : upper_bound_(false) {
    }
    bool initial_ksuf_match(int ksuf_compare, bool emit_equal) const {
        return ksuf_compare < 0 || (ksuf_compare == 0 && emit_equal);
    }
    template <typename K> bool is_duplicate(const K &k,
                                            typename K::ikey_type ikey,
                                            int keylenx) const {
        return k.compare(ikey, keylenx) <= 0 && !upper_bound_;
    }
    template <typename K, typename N> int lower(const K &k, const N *n) const {
        if (upper_bound_)
            return n->size() - 1;
        key_indexed_position kx = N::bound_type::lower_by(k, *n, *n);
        return kx.i - (kx.p < 0);
    }
    template <typename K, typename N>
    key_indexed_position lower_with_position(const K &k, const N *n) const {
        key_indexed_position kx = N::bound_type::lower_by(k, *n, *n);
        kx.i -= kx.p < 0;
        return kx;
    }
    int next(int ki) const {
        return ki - 1;
    }
    void mark_key_complete() const {
        upper_bound_ = false;
    }
    template <typename N, typename K>
    N *advance(const N *n, K &k) const {
        k.assign_store_ikey(n->ikey_bound());
        k.assign_store_length(0);
        return n->prev_;
    }
    template <typename N, typename K>
    typename N::nodeversion_type stable(N *&n, const K &k) const {
        while (1) {
            typename N::nodeversion_type v = n->stable();
            N *next = n->safe_next();
            int cmp;
            if (!next
                || (cmp = Masstree::compare(k.ikey(), next->ikey_bound())) < 0
                || (cmp == 0 && k.length() == 0))
                return v;
            n = next;
        }
    }
    template <typename K> void shift_clear(K &ka) const {
        ka.shift_clear_reverse();
        upper_bound_ = true;
    }
  private:
    mutable bool upper_bound_;
};


/**
@brief
  给定start key @arg[ka], 多次调用find_initial后,
  能够定位到scan operation的起始leafvalue.
  单次调用的作用是traverse down一个layer.

@return value
  @retval1 scan_down 需要继续调用fina_initial下降到masstree的叶子节点
  @retval2 scan_emit 已经下降到masstree的叶子节点, 发射leaf value
  @retval3 scan_find_next 当前layer的leafvalue仅仅是key的low bound,
           (leafvalue可能是是value, 也可能指向next layer)
@pre condition
  root_指向find_initial过程中的layer的根节点 (每个layer都是一颗B+树)
@post condition
  n_指向包含[output]leafvalue的leafnode.
  ki_指向permutation vector中的lower bound
*/
template <typename P> template <typename H>
typename scanstackelt<P>::ScanState
scanstackelt<P>::find_initial(H& helper, key_type& ka, bool emit_equal,
                                  leafvalue_type& entry, threadinfo& ti)
{
    key_indexed_position kx;
    int keylenx = 0;
    char suffixbuf[MASSTREE_MAXKEYLEN];
    Str suffix;

 retry_root:
    n_ = root_->reach_leaf(ka, v_, ti);

 retry_node:
    if (v_.deleted())
        goto retry_root;
    n_->prefetch();
    perm_ = n_->permutation();

    /**
      kx是leaf value的逻辑指针,
      kx.i表示leaf node permutation的index
      kx.p表示leaf value array的index
      当kx.p >= 0时, kx指向key完全匹配的leafvalue.
      当kx.p < 0时, kx.i指向key的low bound
    */
    kx = helper.lower_with_position(ka, this);
    if (kx.p >= 0) {
        keylenx = n_->keylenx_[kx.p];
        fence();
        entry = n_->lv_[kx.p];
        entry.prefetch(keylenx);
        if (n_->keylenx_has_ksuf(keylenx)) {
            suffix = n_->ksuf(kx.p);
            memcpy(suffixbuf, suffix.s, suffix.len);
            suffix.s = suffixbuf;
        }
    }
    if (n_->has_changed(v_)) {
        ti.mark(tc_leaf_retry);
        n_ = n_->advance_to_key(ka, v_, ti);
        goto retry_node;
    }

    ki_ = kx.i;
    if (kx.p >= 0) {
        if (n_->keylenx_is_layer(keylenx)) {
            node_stack_.push_back(root_);
            node_stack_.push_back(n_);
            root_ = entry.layer();
            return scan_down;
        } else if (n_->keylenx_has_ksuf(keylenx)) {
            int ksuf_compare = suffix.compare(ka.suffix());
            if (helper.initial_ksuf_match(ksuf_compare, emit_equal)) {
                int keylen = ka.assign_store_suffix(suffix);
                ka.assign_store_length(keylen);
                return scan_emit;
            }
        } else if (emit_equal)
            return scan_emit;
        // otherwise, this entry must be skipped
        ki_ = helper.next(ki_);
    }
    return scan_find_next;
}

template <typename P> template <typename H>
typename scanstackelt<P>::ScanState
scanstackelt<P>::find_retry(H& helper, key_type& ka, threadinfo& ti)
{
 retry:
    n_ = root_->reach_leaf(ka, v_, ti);
    if (v_.deleted())
        goto retry;

    n_->prefetch();
    perm_ = n_->permutation();
    ki_ = helper.lower(ka, this);
    return scan_find_next;
}

/**
@brief
scan过程中,通过find_next找到下一个value

@pre conditon
(n_,ki_)指向一个leafvalue, 从这个leafvalue出发,
进行最左遍历能够得到next value.
*/
template <typename P> template <typename H>
typename scanstackelt<P>::ScanState
scanstackelt<P>::find_next(H &helper, key_type &ka, leafvalue_type &entry)
{
    int kp;

    if (v_.deleted())
        return scan_retry;

 retry_entry:
    kp = this->kp();
    if (kp >= 0) {
    // next value仍然可以通过n_找到
        ikey_type ikey = n_->ikey0_[kp];
        int keylenx = n_->keylenx_[kp];
        int keylen = keylenx;
        fence();
        entry = n_->lv_[kp];
        entry.prefetch(keylenx);
        if (n_->keylenx_has_ksuf(keylenx))
            keylen = ka.assign_store_suffix(n_->ksuf(kp));

        if (n_->has_changed(v_))
            goto changed;
        // 向上回溯的过程中发现已经这个node是已经遍历过的
        else if (helper.is_duplicate(ka, ikey, keylenx)) {
            ki_ = helper.next(ki_);
            goto retry_entry;
        }

        // We know we can emit the data collected above.
        ka.assign_store_ikey(ikey);
        helper.mark_key_complete();
        if (n_->keylenx_is_layer(keylenx)) {
            node_stack_.push_back(root_);
            node_stack_.push_back(n_);
            root_ = entry.layer();
            return scan_down;
        } else {
            ka.assign_store_length(keylen);
            return scan_emit;
        }
    }

    // next value不在同一leaf节点上时, 直接跳转到这里
    if (!n_->has_changed(v_)) {
        n_ = helper.advance(n_, ka);
        if (!n_) {
            helper.mark_key_complete();
            return scan_up;
        }
        n_->prefetch();
    }

 changed:
    // node发生了变化 或者 到达了新的leaf节点
    v_ = helper.stable(n_, ka);
    perm_ = n_->permutation();
    ki_ = helper.lower(ka, this);
    return scan_find_next;
}

template <typename P> template <typename H, typename F>
int basic_table<P>::scan(H helper,
                         Str firstkey, bool emit_firstkey,
                         F& scanner,
                         threadinfo& ti) const
{
    typedef typename P::ikey_type ikey_type;
    typedef typename node_type::key_type key_type;
    typedef typename node_type::leaf_type::leafvalue_type leafvalue_type;
    union {
        ikey_type x[(MASSTREE_MAXKEYLEN + sizeof(ikey_type) - 1)/sizeof(ikey_type)];
        char s[MASSTREE_MAXKEYLEN];
    } keybuf;
    masstree_precondition(firstkey.len <= (int) sizeof(keybuf));
    memcpy(keybuf.s, firstkey.s, firstkey.len);
    key_type ka(keybuf.s, firstkey.len);

    /**
      stack记录了scan过程中的状态, 包含:
      1. root_: layer的root_节点
      2. n_: layer中的叶子节点
      3. ki_: 叶子节点中,permutation vector中的偏移量
    */
    typedef scanstackelt<P> mystack_type;
    mystack_type stack;
    stack.root_ = root_;
    leafvalue_type entry = leafvalue_type::make_empty();

    int scancount = 0;
    int state;

    while (1) {
        state = stack.find_initial(helper, ka, emit_firstkey, entry, ti);
        scanner.visit_leaf(stack, ka, ti);
        if (state != mystack_type::scan_down)
            break;
        ka.shift();
    }

    while (1) {
        switch (state) {
        case mystack_type::scan_emit:
            ++scancount;
            if (!scanner.visit_value(ka, entry.value(), ti))
                goto done;
            stack.ki_ = helper.next(stack.ki_);
            state = stack.find_next(helper, ka, entry);
            break;

        case mystack_type::scan_find_next:
        find_next:
            state = stack.find_next(helper, ka, entry);
            if (state != mystack_type::scan_up)
                scanner.visit_leaf(stack, ka, ti);
            break;

        case mystack_type::scan_up:
            do {
                if (stack.node_stack_.empty())
                    goto done;
                stack.n_ = static_cast<leaf<P>*>(stack.node_stack_.back());
                stack.node_stack_.pop_back();
                stack.root_ = stack.node_stack_.back();
                stack.node_stack_.pop_back();
                ka.unshift();
            } while (unlikely(ka.empty()));
            stack.v_ = helper.stable(stack.n_, ka);
            stack.perm_ = stack.n_->permutation();
            /*
              由于ki_没有通过stack记录,所以需要重新确定low bound;
              下一次调用find_next时,会发现重复遍历了leafvalue,
              然后会递增ki_调整到正确的位置
            */
            stack.ki_ = helper.lower(ka, &stack);
            goto find_next;

        case mystack_type::scan_down:
            helper.shift_clear(ka);
            goto retry;

        case mystack_type::scan_retry:
        retry:
            state = stack.find_retry(helper, ka, ti);
            break;
        }
    }

 done:
    return scancount;
}

template <typename P> template <typename F>
int basic_table<P>::scan(Str firstkey, bool emit_firstkey,
                         F& scanner,
                         threadinfo& ti) const
{
    return scan(forward_scan_helper(), firstkey, emit_firstkey, scanner, ti);
}

template <typename P> template <typename F>
int basic_table<P>::rscan(Str firstkey, bool emit_firstkey,
                          F& scanner,
                          threadinfo& ti) const
{
    return scan(reverse_scan_helper(), firstkey, emit_firstkey, scanner, ti);
}

template <typename P>
int basic_table<P>::scan_range_first(Str firstkey, bool emit_firstkey,
                         scanstackelt<P> &stack, threadinfo& ti) const
{
  return scan_range_first(forward_scan_helper(),
           firstkey, emit_firstkey, stack, ti);
}

template <typename P>
int basic_table<P>::scan_range_next(scanstackelt<P> &stack,
                         threadinfo& ti) const
{
  return scan_range_next(forward_scan_helper(), stack, ti);
}

template <typename P>
int basic_table<P>::rscan_range_first(Str firstkey, bool emit_firstkey,
                         scanstackelt<P> &stack,
                         threadinfo& ti) const
{
  return scan_range_first(reverse_scan_helper(),
          firstkey, emit_firstkey, stack, ti);
}

template <typename P>
int basic_table<P>::rscan_range_next(scanstackelt<P> &stack,
                         threadinfo& ti) const
{
  return scan_range_next(reverse_scan_helper(), stack, ti);
}

/**
mysql的scan操作要求存储引擎提供: index_read和index_next这两个索引接口.
因此有必要在masstree这一层抽象出类似的接口.
*/

/**
@brief 找到范围查询中第一条符合条件的记录

@args
  arg5 stack
      stack记录了scan过程中的状态, 包含:
      1. root_: layer的root_节点
      2. n_: layer中的叶子节点
      3. ki_: 叶子节点中,permutation vector中的偏移量
      stack必须处于初始状态
*/
template <typename P> template <typename H>
int basic_table<P>::scan_range_first(H helper,
                         Str firstkey, bool emit_firstkey,
                         scanstackelt<P> &stack,
                         threadinfo& ti) const
{
    typedef typename node_type::key_type key_type;
    typedef typename node_type::leaf_type::leafvalue_type leafvalue_type;
    typedef scanstackelt<P> mystack_type;

    masstree_precondition(firstkey.len <= (int) sizeof(stack.keybuf_));
    memcpy(stack.keybuf_.s, firstkey.s, firstkey.len);
    stack.ka_ = key_type(stack.keybuf_.s, firstkey.len);

    stack.root_ = root_;
    stack.entry_ = leafvalue_type::make_empty();

    while (1) {
        stack.state_ = stack.find_initial(helper, stack.ka_,
                emit_firstkey, stack.entry_, ti);
        //scanner.visit_leaf(stack, stack.ka_, ti);
        if (stack.state_ != mystack_type::scan_down)
            break;
        stack.ka_.shift();
    }

    while (1) {
        switch (stack.state_) {
        case mystack_type::scan_emit:
        // scan_range_first在第一次emit后就可以返回了
            ++stack.scan_count_;
            /*
            if (!scanner.visit_value(stack.ka_, stack.entry_.value(), ti)) {
                stack.state_ = mystack_type::scan_finish;
                goto done;
            }
            */
            goto done;

        case mystack_type::scan_find_next:
        find_next:
            stack.state_ = stack.find_next(helper, stack.ka_, stack.entry_);
            //if (stack.state_ != mystack_type::scan_up)
                //scanner.visit_leaf(stack, stack.ka_, ti);
            break;

        case mystack_type::scan_up:
            do {
                if (stack.node_stack_.empty()) {
                    stack.state_ = mystack_type::scan_no_value;
                    goto done;
                }
                stack.n_ = static_cast<leaf<P>*>(stack.node_stack_.back());
                stack.node_stack_.pop_back();
                stack.root_ = stack.node_stack_.back();
                stack.node_stack_.pop_back();
                stack.ka_.unshift();
            } while (unlikely(stack.ka_.empty()));
            stack.v_ = helper.stable(stack.n_, stack.ka_);
            stack.perm_ = stack.n_->permutation();
            /*
              由于ki_没有通过stack记录,所以需要重新确定low bound;
              下一次调用find_next时,会发现重复遍历了leafvalue,
              然后会递增ki_调整到正确的位置
            */
            stack.ki_ = helper.lower(stack.ka_, &stack);
            goto find_next;

        case mystack_type::scan_down:
            helper.shift_clear(stack.ka_);
            goto retry;

        case mystack_type::scan_retry:
        retry:
            stack.state_ = stack.find_retry(helper, stack.ka_, ti);
            break;

        case mystack_type::scan_no_value:
            goto done;
            break;
        }
    }

 done:
    return stack.scan_count_;
}

template <typename P> template <typename H>
int basic_table<P>::scan_range_next(H helper,
                         scanstackelt<P> &stack,
                         threadinfo& ti) const
{
    typedef scanstackelt<P> mystack_type;

            // 为后续遍历调整stack的状态
    stack.ki_ = helper.next(stack.ki_);
    stack.state_ = stack.find_next(helper, stack.ka_, stack.entry_);
    while (1) {
        switch (stack.state_) {
        case mystack_type::scan_emit:
        // scan_range_next每次emit之后需要返回
            ++stack.scan_count_;
            /*
            if (!scanner.visit_value(stack.ka_, stack.entry_.value(), ti)) {
                stack.state_ = mystack_type::scan_finish;
                goto done;
            }
            */
            // 为后续遍历调整stack的状态
            //stack.ki_ = helper.next(stack.ki_);
            //stack.state_ = stack.find_next(helper, stack.ka_, stack.entry_);
            goto done;

        case mystack_type::scan_find_next:
        find_next:
            stack.state_ = stack.find_next(helper, stack.ka_, stack.entry_);
            //if (stack.state_ != mystack_type::scan_up)
                //scanner.visit_leaf(stack, stack.ka_, ti);
            break;

        case mystack_type::scan_up:
            do {
                if (stack.node_stack_.empty()) {
                    stack.state_ = mystack_type::scan_no_value;
                    goto done;
                }
                stack.n_ = static_cast<leaf<P>*>(stack.node_stack_.back());
                stack.node_stack_.pop_back();
                stack.root_ = stack.node_stack_.back();
                stack.node_stack_.pop_back();
                stack.ka_.unshift();
            } while (unlikely(stack.ka_.empty()));
            stack.v_ = helper.stable(stack.n_, stack.ka_);
            stack.perm_ = stack.n_->permutation();
            /*
              由于ki_没有通过stack记录,所以需要重新确定low bound;
              下一次调用find_next时,会发现重复遍历了leafvalue,
              然后会递增ki_调整到正确的位置
            */
            stack.ki_ = helper.lower(stack.ka_, &stack);
            goto find_next;

        case mystack_type::scan_down:
            helper.shift_clear(stack.ka_);
            goto retry;

        case mystack_type::scan_retry:
        retry:
            stack.state_ = stack.find_retry(helper, stack.ka_, ti);
            break;

        case mystack_type::scan_no_value:
            goto done;
            break;
        }
    }

 done:
    return stack.scan_count_;
}

} // namespace Masstree
#endif
