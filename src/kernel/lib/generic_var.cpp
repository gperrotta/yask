/*****************************************************************************

YASK: Yet Another Stencil Kit
Copyright (c) 2014-2019, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*****************************************************************************/

#include "yask_stencil.hpp"
using namespace std;

namespace yask {

    // Ctor. No allocation is done. See notes on default_alloc().
    GenericVarBase::GenericVarBase(KernelStateBase& state,
                                   const string& name,
                                   const VarDimNames& dimNames) :
        KernelStateBase(state),
        _name(name) {
        for (auto& dn : dimNames)
            _var_dims.addDimBack(dn, 1);
    }

    // Template implementations.

    // Make some descriptive info.
    template <typename T>
    string GenericVarTyped<T>::make_info_string(const string& elem_name) const {
        stringstream oss;
        oss << "'" << _name << "' ";
        if (_var_dims.getNumDims() == 0)
            oss << "scalar";
        else
            oss << _var_dims.getNumDims() << "-D var (" <<
                _var_dims.makeDimValStr(" * ") << ")";
        if (_elems)
            oss << " with storage at " << _elems << " containing ";
        else
            oss << " with storage not yet allocated for ";
        oss << makeByteStr(get_num_bytes()) <<
            " (" << makeNumStr(get_num_elems()) << " " <<
            elem_name << " element(s) of " <<
            get_elem_bytes() << " byte(s) each)";
        return oss.str();
    }

    // Free any old storage.
    // Set pointer to storage.
    // 'base' should provide get_num_bytes() bytes at offset bytes.
    template <typename T>
    void GenericVarTyped<T>::set_storage(shared_ptr<char>& base, size_t offset) {
        STATE_VARS(this);

        // Release any old data if last owner.
        release_storage();

        // Share ownership of base.
        // This ensures that last var to use a shared allocation
        // will trigger dealloc.
        _base = base;

        // Set plain pointer to new data.
        if (base.get()) {
            char* p = _base.get() + offset;
            _elems = (void*)p;
        } else {
            _elems = 0;
        }
    }

    // Release storage.
    template <typename T>
    void GenericVarTyped<T>::release_storage() {
        STATE_VARS(this);

        _base.reset();
        _elems = 0;
    }

    // Perform default allocation.  For other options, programmer should
    // call get_num_elems() or get_num_bytes() and then provide allocated
    // memory via set_storage().
    template <typename T>
    void GenericVarTyped<T>::default_alloc() {
        STATE_VARS(this);

        // What node?
        int numa_pref = get_numa_pref();

        // Alloc required number of bytes.
        size_t sz = get_num_bytes();
        string loc = (numa_pref >= 0) ?
            "preferring NUMA node " + to_string(numa_pref) :
            "on default NUMA node";
        DEBUG_MSG("Allocating " << makeByteStr(sz) <<
                  " for var '" << _name << "' " << loc << "...");
        auto base = shared_numa_alloc<char>(sz, numa_pref);

        // Set as storage for this var.
        set_storage(base, 0);
    }

    template <typename T>
    void GenericVarTyped<T>::set_elems_same(T val) {
        if (_elems) {
            yask_parallel_for(0, get_num_elems(), 1,
                              [&](idx_t start, idx_t stop, idx_t thread_num) {
                                  ((T*)_elems)[start] = val;
                              });
        }
    }

    template <typename T>
    void GenericVarTyped<T>::set_elems_in_seq(T seed) {
        if (_elems) {
            const idx_t wrap = 71; // TODO: avoid multiple of any dim size.
            yask_parallel_for(0, get_num_elems(), 1,
                              [&](idx_t start, idx_t stop, idx_t thread_num) {
                                  ((T*)_elems)[start] = seed * T(start % wrap + 1);
                              });
        }
    }

    // Explicitly allowed instantiations.
    template class GenericVarTyped<real_t>;
    template class GenericVarTyped<real_vec_t>;

} // yask namespace.
