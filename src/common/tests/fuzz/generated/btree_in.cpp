// This is a generated file! Please edit source .ksy file and use kaitai-struct-compiler to rebuild

#include "btree_in.h"
std::set<btree_in_t::consts_t> btree_in_t::_build_values_consts_t() {
    std::set<btree_in_t::consts_t> _t;
    _t.insert(btree_in_t::CONSTS_OPS_NUM);
    return _t;
}
const std::set<btree_in_t::consts_t> btree_in_t::_values_consts_t = btree_in_t::_build_values_consts_t();
bool btree_in_t::_is_defined_consts_t(btree_in_t::consts_t v) {
    return btree_in_t::_values_consts_t.find(v) != btree_in_t::_values_consts_t.end();
}

btree_in_t::btree_in_t(kaitai::kstream* p__io, kaitai::kstruct* p__parent, btree_in_t* p__root) : kaitai::kstruct(p__io) {
    m__parent = p__parent;
    m__root = p__root ? p__root : this;
    m_params = 0;
    m_op = 0;

    try {
        _read();
    } catch(...) {
        _clean_up();
        throw;
    }
}

void btree_in_t::_read() {
    m_params = new btree_parameters_t(m__io, this, m__root);
    m_op = new std::vector<btree_op_t*>();
    {
        int i = 0;
        while (!m__io->is_eof()) {
            m_op->push_back(new btree_op_t(m__io, this, m__root));
            i++;
        }
    }
}

btree_in_t::~btree_in_t() {
    _clean_up();
}

void btree_in_t::_clean_up() {
    if (m_params) {
        delete m_params; m_params = 0;
    }
    if (m_op) {
        for (std::vector<btree_op_t*>::iterator it = m_op->begin(); it != m_op->end(); ++it) {
            delete *it;
        }
        delete m_op; m_op = 0;
    }
}

btree_in_t::btree_op_t::btree_op_t(kaitai::kstream* p__io, btree_in_t* p__parent, btree_in_t* p__root) : kaitai::kstruct(p__io) {
    m__parent = p__parent;
    m__root = p__root;

    try {
        _read();
    } catch(...) {
        _clean_up();
        throw;
    }
}

void btree_in_t::btree_op_t::_read() {
    m_type = m__io->read_u1();
    m_key_id = m__io->read_u1();
    n_value_id = true;
    if (kaitai::kstream::mod(type(), 3) == 0) {
        n_value_id = false;
        m_value_id = m__io->read_u1();
    }
}

btree_in_t::btree_op_t::~btree_op_t() {
    _clean_up();
}

void btree_in_t::btree_op_t::_clean_up() {
    if (!n_value_id) {
    }
}

btree_in_t::btree_parameters_t::btree_parameters_t(kaitai::kstream* p__io, btree_in_t* p__parent, btree_in_t* p__root) : kaitai::kstruct(p__io) {
    m__parent = p__parent;
    m__root = p__root;

    try {
        _read();
    } catch(...) {
        _clean_up();
        throw;
    }
}

void btree_in_t::btree_parameters_t::_read() {
    m_tree_order = m__io->read_u1();
    m_seed = m__io->read_u4le();
    m_key_num = m__io->read_u1();
    m_value_num = m__io->read_u1();
}

btree_in_t::btree_parameters_t::~btree_parameters_t() {
    _clean_up();
}

void btree_in_t::btree_parameters_t::_clean_up() {
}
