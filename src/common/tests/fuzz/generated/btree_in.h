#ifndef BTREE_IN_H_
#define BTREE_IN_H_

// This is a generated file! Please edit source .ksy file and use kaitai-struct-compiler to rebuild

class btree_in_t;

#include "kaitai/kaitaistruct.h"
#include <stdint.h>
#include <set>
#include <vector>

#if KAITAI_STRUCT_VERSION < 11000L
#error "Incompatible Kaitai Struct C++/STL API: version 0.11 or later is required"
#endif

/**
 * File format introduced to be both easy to fuzz and describe a nontrivial sequence of btree
 * operations.
 */

class btree_in_t : public kaitai::kstruct {

public:
    class btree_op_t;
    class btree_parameters_t;

    enum consts_t {
        CONSTS_OPS_NUM = 3
    };
    static bool _is_defined_consts_t(consts_t v);

private:
    static const std::set<consts_t> _values_consts_t;
    static std::set<consts_t> _build_values_consts_t();

public:

    btree_in_t(kaitai::kstream* p__io, kaitai::kstruct* p__parent = 0, btree_in_t* p__root = 0);

private:
    void _read();
    void _clean_up();

public:
    ~btree_in_t();

    class btree_op_t : public kaitai::kstruct {

    public:

        btree_op_t(kaitai::kstream* p__io, btree_in_t* p__parent = 0, btree_in_t* p__root = 0);

    private:
        void _read();
        void _clean_up();

    public:
        ~btree_op_t();

    private:
        uint8_t m_type;
        uint8_t m_key_id;
        uint8_t m_value_id;
        bool n_value_id;

    public:
        bool _is_null_value_id() { value_id(); return n_value_id; };

    private:
        btree_in_t* m__root;
        btree_in_t* m__parent;

    public:
        uint8_t type() const { return m_type; }
        uint8_t key_id() const { return m_key_id; }
        uint8_t value_id() const { return m_value_id; }
        btree_in_t* _root() const { return m__root; }
        btree_in_t* _parent() const { return m__parent; }
    };

    class btree_parameters_t : public kaitai::kstruct {

    public:

        btree_parameters_t(kaitai::kstream* p__io, btree_in_t* p__parent = 0, btree_in_t* p__root = 0);

    private:
        void _read();
        void _clean_up();

    public:
        ~btree_parameters_t();

    private:
        uint8_t m_tree_order;
        uint32_t m_seed;
        uint8_t m_key_num;
        uint8_t m_value_num;
        btree_in_t* m__root;
        btree_in_t* m__parent;

    public:
        uint8_t tree_order() const { return m_tree_order; }
        uint32_t seed() const { return m_seed; }
        uint8_t key_num() const { return m_key_num; }
        uint8_t value_num() const { return m_value_num; }
        btree_in_t* _root() const { return m__root; }
        btree_in_t* _parent() const { return m__parent; }
    };

private:
    btree_parameters_t* m_params;
    std::vector<btree_op_t*>* m_op;
    btree_in_t* m__root;
    kaitai::kstruct* m__parent;

public:
    btree_parameters_t* params() const { return m_params; }
    std::vector<btree_op_t*>* op() const { return m_op; }
    btree_in_t* _root() const { return m__root; }
    kaitai::kstruct* _parent() const { return m__parent; }
};

#endif  // BTREE_IN_H_
