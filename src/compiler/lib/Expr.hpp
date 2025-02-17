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

///////// AST Expressions ////////////

// TODO: break this up into several smaller files.

#pragma once

#include "yask_compiler_api.hpp"

#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <cstdarg>
#include <fstream>

// Need g++ >= 4.9 for regex.
#if defined(__GNUC__) && !defined(__clang__)
#define GCC_VERSION (__GNUC__ * 10000       \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)
#if GCC_VERSION < 40900
#error G++ 4.9.0 or later is required
#endif
#endif
#include <regex>

// Common utilities.
#define CHECK
#include "common_utils.hpp"
#include "tuple.hpp"
#include "idiv.hpp"

using namespace std;

namespace yask {

    // Forward-decls of expressions.
    class Expr;
    typedef shared_ptr<Expr> exprPtr;
    class NumExpr;
    typedef shared_ptr<NumExpr> numExprPtr;
    typedef vector<numExprPtr> numExprPtrVec;
    class VarPoint;
    typedef shared_ptr<VarPoint> varPointPtr;
    class IndexExpr;
    typedef shared_ptr<IndexExpr> indexExprPtr;
    typedef vector<indexExprPtr> indexExprPtrVec;
    class BoolExpr;
    typedef shared_ptr<BoolExpr> boolExprPtr;
    class EqualsExpr;
    typedef shared_ptr<EqualsExpr> equalsExprPtr;

    // More forward-decls.
    class ExprVisitor;
    class Var;
    class StencilSolution;
    struct Dimensions;

    typedef map<string, string> VarMap; // map used when substituting vars.

    //// Classes to implement parts of expressions.

    // The base class for all expression nodes.
    // NB: there is no clone() method defined here; they
    // are defined on immediate derived types:
    // NumExpr, BoolExpr, and EqualsExpr.
    class Expr : public virtual yc_expr_node {

    public:
        Expr() { }
        virtual ~Expr() { }

        // For visitors.
        virtual string accept(ExprVisitor* ev) =0;
        virtual string accept(ExprVisitor* ev) const;

        // check for expression equivalency.
        // Does *not* check value equivalency except for
        // constants.
        virtual bool isSame(const Expr* other) const =0;
        virtual bool isSame(const exprPtr& other) const {
            return isSame(other.get());
        }

        // Make pair if possible.
        // Return whether pair made.
        virtual bool makePair(Expr* other) {
            return false;
        }
        virtual bool makePair(exprPtr other) {
            return makePair(other.get());
        }

        // Return a formatted expr.
        virtual string makeStr(const VarMap* varMap = 0) const;
        virtual string makeQuotedStr(string quote = "'",
                                     const VarMap* varMap = 0) const {
            return quote + makeStr(varMap) + quote;
        }
        virtual string getDescr() const {
            return makeQuotedStr();
        }

        // Count and return number of nodes at and below this.
        virtual int getNumNodes() const;

        // Use addr of this as a unique ID for this object.
        virtual size_t getId() const {
            return size_t(this);
        }
        virtual string getIdStr() const {
            return to_string(idx_t(this));
        }
        virtual string getQuotedId() const {
            return "\"" + to_string(idx_t(this)) + "\"";
        }

        // APIs.
        virtual string format_simple() const {
            return makeStr();
        }
        virtual int get_num_nodes() const {
            return getNumNodes();
        }
    };

    // Convert pointer to the given ptr type or die trying.
    template<typename T> shared_ptr<T> castExpr(exprPtr ep, const string& descrip) {
        auto tp = dynamic_pointer_cast<T>(ep);
        if (!tp) {
            THROW_YASK_EXCEPTION("Error: expression '" + ep->makeStr() +
                                 "' is not a " + descrip);
        }
        return tp;
    }

    // Compare 2 expr pointers and return whether the expressions are
    // equivalent.
    bool areExprsSame(const Expr* e1, const Expr* e2);
    inline bool areExprsSame(const exprPtr e1, const Expr* e2) {
        return areExprsSame(e1.get(), e2);
    }
    inline bool areExprsSame(const Expr* e1, const exprPtr e2) {
        return areExprsSame(e1, e2.get());
    }
    inline bool areExprsSame(const exprPtr e1, const exprPtr e2) {
        return areExprsSame(e1.get(), e2.get());
    }

    // Real or int value.
    class NumExpr : public Expr,
                    public virtual yc_number_node {
    public:

        // Return 'true' if this is a compile-time constant.
        virtual bool isConstVal() const {
            return false;
        }

        // Get the current value.
        // Exit with error if not known.
        virtual double getNumVal() const {
            THROW_YASK_EXCEPTION("Error: cannot evaluate '" + makeStr() +
                "' for a known numerical value");
        }

        // Get the value as an integer.
        // Exits with error if not an integer.
        virtual int getIntVal() const {
            double val = getNumVal();
            int ival = int(val);
            if (val != double(ival)) {
                THROW_YASK_EXCEPTION("Error: '" + makeStr() +
                    "' does not evaluate to an integer");
            }
            return ival;
        }

        // Return 'true' and set offset if this expr is of the form 'dim',
        // 'dim+const', or 'dim-const'.
        // TODO: make this more robust.
        virtual bool isOffsetFrom(string dim, int& offset) {
            return false;
        }

        // Create a deep copy of this expression.
        // For this to work properly, each derived type
        // should also implement a deep-copy copy ctor.
        virtual numExprPtr clone() const =0;
        virtual yc_number_node_ptr clone_ast() const {
            return clone();
        }
    };

    // Var index types.
    enum IndexType {
        STEP_INDEX,             // the step dim.
        DOMAIN_INDEX,           // a domain dim.
        MISC_INDEX,             // any other dim.
        FIRST_INDEX,            // first index value in domain.
        LAST_INDEX              // last index value in domain.
    };

    // Expression based on a dimension index.
    // This is an expression leaf-node.
    class IndexExpr : public NumExpr,
                      public virtual yc_index_node {
    protected:
        string _dimName;
        IndexType _type;

    public:
        IndexExpr(string dim, IndexType type) :
            _dimName(dim), _type(type) { }
        virtual ~IndexExpr() { }

        const string& getName() const { return _dimName; }
        IndexType getType() const { return _type; }
        string format(const VarMap* varMap = 0) const {
            switch (_type) {
            case FIRST_INDEX:
                return "FIRST_INDEX(" + _dimName + ")";
            case LAST_INDEX:
                return "LAST_INDEX(" + _dimName + ")";
            default:
                if (varMap && varMap->count(_dimName))
                    return varMap->at(_dimName);
                else
                    return _dimName;
            }
        }
        virtual string accept(ExprVisitor* ev);

        // Simple offset?
        virtual bool isOffsetFrom(string dim, int& offset);

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const {
            auto p = dynamic_cast<const IndexExpr*>(other);
            return p && _dimName == p->_dimName && _type == p->_type;
        }

        // Create a deep copy of this expression.
        virtual numExprPtr clone() const { return make_shared<IndexExpr>(*this); }

        // APIs.
        virtual const string& get_name() const {
            return _dimName;
        }
    };

    // Boolean value.
    class BoolExpr : public Expr,
                     public virtual yc_bool_node  {
    public:

        // Get the current value.
        // Exit with error if not known.
        virtual bool getBoolVal() const {
            THROW_YASK_EXCEPTION("Error: cannot evaluate '" + makeStr() +
                                 "' for a known boolean value");
        }

        // Create a deep copy of this expression.
        // For this to work properly, each derived type
        // should also implement a copy ctor.
        virtual boolExprPtr clone() const =0;
        virtual yc_bool_node_ptr clone_ast() const {
            return clone();
        }
    };

    // A simple constant value.
    // This is an expression leaf-node.
    class ConstExpr : public NumExpr,
                      public virtual yc_const_number_node {
    protected:
        double _f = 0.0;

    public:
        ConstExpr(double f) : _f(f) { }
        ConstExpr(idx_t i) : _f(i) {
            if (idx_t(_f) != i)
                FORMAT_AND_THROW_YASK_EXCEPTION("Error: integer value " << i <<
                                     " cannot be stored accurately as a double");
        }
        ConstExpr(int i) : ConstExpr(idx_t(i)) { }
        ConstExpr(const ConstExpr& src) : _f(src._f) { }
        virtual ~ConstExpr() { }

        virtual bool isConstVal() const { return true; }
        double getNumVal() const { return _f; }

        virtual string accept(ExprVisitor* ev);

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const {
            auto p = dynamic_cast<const ConstExpr*>(other);
            return p && _f == p->_f;
        }

        // Create a deep copy of this expression.
        virtual numExprPtr clone() const { return make_shared<ConstExpr>(*this); }

        // APIs.
        virtual void set_value(double val) { _f = val; }
        virtual double get_value() const { return _f; }
    };

    // Any expression that returns a real (not from a var).
    // This is an expression leaf-node.
    class CodeExpr : public NumExpr {
    protected:
        string _code;

    public:
        CodeExpr(const string& code) :
            _code(code) { }
        CodeExpr(const CodeExpr& src) :
            _code(src._code) { }
        virtual ~CodeExpr() { }

        const string& getCode() const {
            return _code;
        }

        virtual string accept(ExprVisitor* ev);

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const {
            auto p = dynamic_cast<const CodeExpr*>(other);
            return p && _code == p->_code;
        }

        // Create a deep copy of this expression.
        virtual numExprPtr clone() const { return make_shared<CodeExpr>(*this); }
    };

    // Base class for any generic unary operator.
    // Also extended for binary operators by adding a LHS.
    // Still pure virtual because clone() not implemented.
    template <typename BaseT,
              typename ArgT, typename ArgApiT>
    class UnaryExpr : public BaseT {
    protected:
        ArgT _rhs;
        string _opStr;

    public:
        UnaryExpr(const string& opStr, ArgT rhs) :
            _rhs(rhs),
            _opStr(opStr) { }
        UnaryExpr(const UnaryExpr& src) :
            _rhs(src._rhs->clone()),
            _opStr(src._opStr) { }

        ArgT& getRhs() { return _rhs; }
        const ArgT& getRhs() const { return _rhs; }
        const string& getOpStr() const { return _opStr; }

        virtual string accept(ExprVisitor* ev);

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const {
            auto p = dynamic_cast<const UnaryExpr*>(other);
            return p && _opStr == p->_opStr &&
                _rhs && _rhs->isSame(p->_rhs.get());
        }
    };

    // Various types of unary operators depending on input and output types.
    typedef UnaryExpr<NumExpr, numExprPtr, yc_number_node_ptr> UnaryNumExpr;
    typedef UnaryExpr<BoolExpr, numExprPtr, yc_number_node_ptr> UnaryNum2BoolExpr;
    typedef UnaryExpr<BoolExpr, boolExprPtr, yc_bool_node_ptr> UnaryBoolExpr;

    // Negate operator.
    class NegExpr : public UnaryNumExpr,
                    public virtual yc_negate_node {
    public:
        NegExpr(numExprPtr rhs) :
            UnaryNumExpr(opStr(), rhs) { }
        NegExpr(const NegExpr& src) :
            UnaryExpr(src) { }

        static string opStr() { return "-"; }
        virtual bool isConstVal() const {
            return _rhs->isConstVal();
        }
        virtual double getNumVal() const {
            double rhs = _rhs->getNumVal();
            return -rhs;
        }
        virtual numExprPtr clone() const {
            return make_shared<NegExpr>(*this);
        }

        // APIs.
        virtual yc_number_node_ptr get_rhs() {
            return _rhs;
        }
    };

    // Boolean inverse operator.
    class NotExpr : public UnaryBoolExpr,
                    public virtual yc_not_node {
    public:
        NotExpr(boolExprPtr rhs) :
            UnaryBoolExpr(opStr(), rhs) { }
        NotExpr(const NotExpr& src) :
            UnaryBoolExpr(src) { }

        static string opStr() { return "!"; }
        virtual bool getBoolVal() const {
            bool rhs = _rhs->getBoolVal();
            return !rhs;
        }
        virtual boolExprPtr clone() const {
            return make_shared<NotExpr>(*this);
        }
        virtual yc_bool_node_ptr get_rhs() {
            return getRhs();
        }
    };


    // Base class for any generic binary operator.
    // Still pure virtual because clone() not implemented.
    template <typename BaseT, typename BaseApiT,
              typename ArgT, typename ArgApiT>
    class BinaryExpr : public BaseT,
                       public virtual BaseApiT {
    protected:
        ArgT _lhs;              // RHS in BaseT which must be a UnaryExpr.

    public:
        BinaryExpr(ArgT lhs, const string& opStr, ArgT rhs) :
            BaseT(opStr, rhs),
            _lhs(lhs) { }
        BinaryExpr(const BinaryExpr& src) :
            BaseT(src._opStr, src._rhs->clone()),
            _lhs(src._lhs->clone()) { }

        ArgT& getLhs() { return _lhs; }
        const ArgT& getLhs() const { return _lhs; }
        virtual string accept(ExprVisitor* ev);

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const {
            auto p = dynamic_cast<const BinaryExpr*>(other);
            return p && BaseT::_opStr == p->_opStr &&
                _lhs->isSame(p->_lhs.get()) &&
                BaseT::_rhs->isSame(p->_rhs.get());
        }

        // APIs.
        virtual ArgApiT get_lhs() {
            return getLhs();
        }
        virtual ArgApiT get_rhs() {
            return BaseT::getRhs();
        }
    };

    // Various types of binary operators depending on input and output types.
    typedef BinaryExpr<UnaryNumExpr, yc_binary_number_node,
                       numExprPtr, yc_number_node_ptr> BinaryNumExpr; // fn(num, num) -> num.
    typedef BinaryExpr<UnaryBoolExpr, yc_binary_bool_node,
                       boolExprPtr, yc_bool_node_ptr> BinaryBoolExpr; // fn(bool, bool) -> bool.
    typedef BinaryExpr<UnaryNum2BoolExpr, yc_binary_comparison_node,
                       numExprPtr, yc_number_node_ptr> BinaryNum2BoolExpr; // fn(num, num) -> bool.

    // Numerical binary operators.
    // TODO: redo this with a template.
#define BIN_NUM_EXPR(type, implType, opstr, oper)       \
    class type : public BinaryNumExpr,                  \
                 public virtual implType {              \
    public:                                             \
        type(numExprPtr lhs, numExprPtr rhs) :          \
            BinaryNumExpr(lhs, opStr(), rhs) { }        \
        type(const type& src) :                         \
            BinaryNumExpr(src) { }                      \
        static string opStr() { return opstr; }         \
        virtual bool isOffsetFrom(string dim, int& offset); \
        virtual bool isConstVal() const {               \
            return _lhs->isConstVal() &&                \
                _rhs->isConstVal();                     \
        }                                               \
        virtual double getNumVal() const {              \
            double lhs = _lhs->getNumVal();             \
            double rhs = _rhs->getNumVal();             \
            return oper;                                \
        }                                               \
        virtual numExprPtr clone() const {              \
            return make_shared<type>(*this);            \
        }                                               \
    }
    BIN_NUM_EXPR(SubExpr, yc_subtract_node, "-", lhs - rhs);

    // TODO: add check for div-by-0.
    // TODO: handle division properly for integer indices.
    BIN_NUM_EXPR(DivExpr, yc_divide_node, "/", lhs / rhs);

    // TODO: add check for mod-by-0.
    BIN_NUM_EXPR(ModExpr, yc_mod_node, "%", imod_flr(idx_t(lhs), idx_t(rhs)));
#undef BIN_NUM_EXPR

// Boolean binary operators with numerical inputs.
// TODO: redo this with a template.
#define BIN_NUM2BOOL_EXPR(type, implType, opstr, oper)  \
    class type : public BinaryNum2BoolExpr,             \
                 public virtual implType {              \
    public:                                             \
    type(numExprPtr lhs, numExprPtr rhs) :              \
        BinaryNum2BoolExpr(lhs, opStr(), rhs) { }       \
    type(const type& src) :                             \
        BinaryNum2BoolExpr(src) { }                     \
    static string opStr() { return opstr; }             \
    virtual bool getBoolVal() const {                   \
        double lhs = _lhs->getNumVal();                 \
        double rhs = _rhs->getNumVal();                 \
        return oper;                                    \
    }                                                   \
    virtual boolExprPtr clone() const {                 \
        return make_shared<type>(*this);                \
    }                                                   \
    virtual yc_number_node_ptr get_lhs() {              \
        return getLhs();                                \
    }                                                   \
    virtual yc_number_node_ptr get_rhs() {              \
        return getRhs();                                \
    }                                                   \
    }
    BIN_NUM2BOOL_EXPR(IsEqualExpr, yc_equals_node, "==", lhs == rhs);
    BIN_NUM2BOOL_EXPR(NotEqualExpr, yc_not_equals_node, "!=", lhs != rhs);
    BIN_NUM2BOOL_EXPR(IsLessExpr, yc_less_than_node, "<", lhs < rhs);
    BIN_NUM2BOOL_EXPR(NotLessExpr, yc_not_less_than_node, ">=", lhs >= rhs);
    BIN_NUM2BOOL_EXPR(IsGreaterExpr, yc_greater_than_node, ">", lhs > rhs);
    BIN_NUM2BOOL_EXPR(NotGreaterExpr, yc_not_greater_than_node, "<=", lhs <= rhs);
#undef BIN_NUM2BOOL_EXPR

    // Boolean binary operators with boolean inputs.
    // TODO: redo this with a template.
#define BIN_BOOL_EXPR(type, implType, opstr, oper)      \
    class type : public BinaryBoolExpr, \
                 public virtual implType {              \
    public:                                             \
    type(boolExprPtr lhs, boolExprPtr rhs) :            \
        BinaryBoolExpr(lhs, opStr(), rhs) { }           \
    type(const type& src) :                             \
        BinaryBoolExpr(src) { }                         \
    static string opStr() { return opstr; }             \
    virtual bool getBoolVal() const {                   \
        bool lhs = _lhs->getBoolVal();                  \
        bool rhs = _rhs->getBoolVal();                  \
        return oper;                                    \
    }                                                   \
    virtual boolExprPtr clone() const {                 \
        return make_shared<type>(*this);                \
    }                                                   \
    virtual yc_bool_node_ptr get_lhs() {                \
        return getLhs();                                \
    }                                                   \
    virtual yc_bool_node_ptr get_rhs() {                \
        return getRhs();                                \
    }                                                   \
    }
    BIN_BOOL_EXPR(AndExpr, yc_and_node, "&&", lhs && rhs);
    BIN_BOOL_EXPR(OrExpr, yc_or_node, "||", lhs || rhs);
#undef BIN_BOOL_EXPR

    // A list of exprs with a common operator that can be rearranged,
    // e.g., 'a * b * c' or 'a + b + c'.
    // Still pure virtual because clone() not implemented.
    class CommutativeExpr : public NumExpr,
                            public virtual yc_commutative_number_node {
    protected:
        numExprPtrVec _ops;
        string _opStr;

    public:
        CommutativeExpr(const string& opStr) :
            _opStr(opStr) {
        }
        CommutativeExpr(numExprPtr lhs, const string& opStr, numExprPtr rhs) :
            _opStr(opStr) {
            _ops.push_back(lhs->clone());
            _ops.push_back(rhs->clone());
        }
        CommutativeExpr(const CommutativeExpr& src) :
            _opStr(src._opStr) {
            for(auto op : src._ops)
                _ops.push_back(op->clone());
        }
        virtual ~CommutativeExpr() { }

        // Accessors.
        numExprPtrVec& getOps() { return _ops; }
        const numExprPtrVec& getOps() const { return _ops; }
        const string& getOpStr() const { return _opStr; }

        // Clone and add an operand.
        virtual void appendOp(numExprPtr op) {
            _ops.push_back(op->clone());
        }

        // If op is another CommutativeExpr with the
        // same operator, add its operands to this.
        // Otherwise, just add the whole node.
        // Example: if 'this' is 'A+B', 'mergeExpr(C+D)'
        // returns 'A+B+C+D', and 'mergeExpr(E*F)'
        // returns 'A+B+(E*F)'.
        virtual void mergeExpr(numExprPtr op) {
            auto opp = dynamic_pointer_cast<CommutativeExpr>(op);
            if (opp && opp->getOpStr() == _opStr) {
                for(auto op2 : opp->_ops)
                    appendOp(op2);
            }
            else
                appendOp(op);
        }

        // Swap the contents w/another.
        virtual void swap(CommutativeExpr* ce) {
            _ops.swap(ce->_ops);
            _opStr.swap(ce->_opStr);
        }

        virtual string accept(ExprVisitor* ev);

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const;

        virtual bool isConstVal() const {
            for(auto op : _ops) {
                if (!op->isConstVal())
                    return false;
            }
            return true;
        }

        // APIs.
        virtual int get_num_operands() {
            return _ops.size();
        }
        virtual std::vector<yc_number_node_ptr> get_operands() {
            std::vector<yc_number_node_ptr> nv;
            for (int i = 0; i < get_num_operands(); i++)
                nv.push_back(_ops.at(i));
            return nv;
        }
        virtual void add_operand(yc_number_node_ptr node) {
            auto p = dynamic_pointer_cast<NumExpr>(node);
            assert(p);
            appendOp(p);
        }
    };

    // Commutative operators.
    // TODO: redo this with a template.
#define COMM_EXPR(type, implType, opstr, baseVal, oper)                 \
    class type : public CommutativeExpr, public virtual implType  {     \
    public:                                                             \
    type()  :                                                           \
        CommutativeExpr(opStr()) { }                                    \
    type(numExprPtr lhs, numExprPtr rhs) :                              \
        CommutativeExpr(lhs, opStr(), rhs) { }                          \
    type(const type& src) :                                             \
        CommutativeExpr(src) { }                                        \
    virtual ~type() { }                                                 \
    static string opStr() { return opstr; }                             \
    virtual bool isOffsetFrom(string dim, int& offset);                 \
    virtual double getNumVal() const {                                  \
        double val = baseVal;                                           \
        for(auto op : _ops) {                                           \
            double lhs = val;                                           \
            double rhs = op->getNumVal();                               \
            val = oper;                                                 \
        }                                                               \
        return val;                                                     \
    }                                                                   \
    virtual numExprPtr clone() const { return make_shared<type>(*this); } \
    };
    COMM_EXPR(MultExpr, yc_multiply_node, "*", 1.0, lhs * rhs)
    COMM_EXPR(AddExpr, yc_add_node, "+", 0.0, lhs + rhs)
#undef COMM_EXPR

    // An FP function call with an arbitrary number of FP args.
    // e.g., sin(a).
    // TODO: add APIs.
    class FuncExpr : public NumExpr {
    protected:
        string _opStr;          // name of function.
        numExprPtrVec _ops;     // args to function.

        // Special handler for pairable functions like sincos().
        FuncExpr* _paired = nullptr;     // ptr to counterpart.

    public:
        FuncExpr(const string& opStr, const std::initializer_list< const numExprPtr > & ops) :
            _opStr(opStr) {
            for (auto& op : ops)
                _ops.push_back(op->clone());
        }
        FuncExpr(const FuncExpr& src) :
            _opStr(src._opStr, {}) {

            // Deep copy.
            for (auto& op : src._ops)
                _ops.push_back(op->clone());
        }

        // Accessors.
        numExprPtrVec& getOps() { return _ops; }
        const numExprPtrVec& getOps() const { return _ops; }
        const string& getOpStr() const { return _opStr; }

        virtual string accept(ExprVisitor* ev);

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const;

        virtual bool makePair(Expr* other);
        virtual FuncExpr* getPair() { return _paired; }

        virtual bool isConstVal() const {
            for(auto op : _ops) {
                if (!op->isConstVal())
                    return false;
            }
            return true;
        }
        virtual numExprPtr clone() const {
            return make_shared<FuncExpr>(*this);
        }

        // APIs.
        virtual int get_num_operands() {
            return _ops.size();
        }
        virtual std::vector<yc_number_node_ptr> get_operands() {
            std::vector<yc_number_node_ptr> nv;
            for (int i = 0; i < get_num_operands(); i++)
                nv.push_back(_ops.at(i));
            return nv;
        }
    };

    // One specific point in a var.
    // This is an expression leaf-node.
    class VarPoint : public NumExpr,
                      public virtual yc_var_point_node {

    public:

        // What kind of vectorization can be done on this point.
        // Set via Eqs::analyzeVec().
        enum VecType { VEC_UNSET,
                       VEC_FULL, // vectorizable in all folded dims.
                       VEC_PARTIAL, // vectorizable in some folded dims.
                       VEC_NONE  // vectorizable in no folded dims.
        };

        // Analysis of this point for accesses via loops through the inner dim.
        // Set via Eqs::analyzeLoop().
        enum LoopType { LOOP_UNSET,
                        LOOP_INVARIANT, // not dependent on inner dim.
                        LOOP_OFFSET,    // only dep on inner dim +/- const in inner-dim posn.
                        LOOP_OTHER      // dep on inner dim in another way.
        };

    protected:
        Var* _var = 0;        // the var this point is from.

        // Index exprs for each dim, e.g.,
        // "3, x-5, y*2, z+4" for dims "n, x, y, z".
        numExprPtrVec _args;

        // Vars below are calculated from above.
        
        // Simple offset for each expr that is dim +/- offset, e.g.,
        // "x=-5, z=4" from above example.
        // Set in ctor and modified via setArgOffset/Const().
        IntTuple _offsets;

        // Simple value for each expr that is a const, e.g.,
        // "n=3" from above example.
        // Set in ctor and modified via setArgOffset/Const().
        IntTuple _consts;

        VecType _vecType = VEC_UNSET; // allowed vectorization.
        LoopType _loopType = LOOP_UNSET; // analysis for looping.

        // Cache the string repr.
        string _defStr;
        void _updateStr() {
            _defStr = _makeStr();
        }
        string _makeStr(const VarMap* varMap = 0) const;

    public:

        // Construct a point given a var and an arg for each dim.
        VarPoint(Var* var, const numExprPtrVec& args);

        // Dtor.
        virtual ~VarPoint() {}

        // Get parent var info.
        const Var* getVar() const { return _var; }
        Var* getVar() { return _var; }
        virtual const string& getVarName() const;
        virtual bool isVarFoldable() const;
        virtual const indexExprPtrVec& getDims() const;

        // Accessors.
        virtual const numExprPtrVec& getArgs() const { return _args; }
        virtual const IntTuple& getArgOffsets() const { return _offsets; }
        virtual const IntTuple& getArgConsts() const { return _consts; }
        virtual VecType getVecType() const {
            assert(_vecType != VEC_UNSET);
            return _vecType;
        }
        virtual void setVecType(VecType vt) {
            _vecType = vt;
        }
        virtual LoopType getLoopType() const {
            assert(_loopType != LOOP_UNSET);
            return _loopType;
        }
        virtual void setLoopType(LoopType vt) {
            _loopType = vt;
        }

        // Get arg for 'dim' or return null if none.
        virtual const numExprPtr getArg(const string& dim) const;
        
        // Set given arg to given offset; ignore if not in step or domain var dims.
        virtual void setArgOffset(const IntScalar& offset);

        // Set given args to be given offsets.
        virtual void setArgOffsets(const IntTuple& offsets) {
            for (auto ofs : offsets)
                setArgOffset(ofs);
        }

        // Set given arg to given const.
        virtual void setArgConst(const IntScalar& val);

        // Some comparisons.
        bool operator==(const VarPoint& rhs) const {
            return _defStr == rhs._defStr;
        }
        bool operator<(const VarPoint& rhs) const {
            return _defStr < rhs._defStr;
        }

        // Take ev to each value.
        virtual string accept(ExprVisitor* ev);

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const {
            auto p = dynamic_cast<const VarPoint*>(other);
            return p && *this == *p;
        }

        // Check for same logical var.
        // A logical var is defined by the var itself
        // and any const indices.
        virtual bool isSameLogicalVar(const VarPoint& rhs) const {
            return _var == rhs._var && _consts == rhs._consts;
        }

        // String w/name and parens around args, e.g., 'u(x, y+2)'.
        // Apply substitutions to indices using 'varMap' if provided.
        virtual string makeStr(const VarMap* varMap = 0) const {
            if (varMap)
                return _makeStr(varMap);
            return _defStr;
        }

        // String w/name and parens around const args, e.g., 'u(n=4)'.
        // Apply substitutions to indices using 'varMap' if provided.
        virtual string makeLogicalVarStr(const VarMap* varMap = 0) const;

        // String w/just comma-sep args, e.g., 'x, y+2'.
        // Apply substitutions to indices using 'varMap' if provided.
        virtual string makeArgStr(const VarMap* varMap = 0) const;

        // String v/vec-normalized args, e.g., 'x, y+(2/VLEN_Y)'.
        // This object has numerators; 'fold' object has denominators.
        // Apply substitutions to indices using 'varMap' if provided.
        virtual string makeNormArgStr(const Dimensions& dims,
                                      const VarMap* varMap = 0) const;

        // Make string like "x+(4/VLEN_X)" from original arg "x+4" in 'dname' dim.
        // This object has numerators; 'fold' object has denominators.
        // Apply substitutions to indices using 'varMap' if provided.
        virtual string makeNormArgStr(const string& dname,
                                      const Dimensions& dims,
                                      const VarMap* varMap = 0) const;

        // Make string like "g->_wrap_step(t+1)" from original arg "t+1"
        // if var uses step dim, "0" otherwise.
        virtual string makeStepArgStr(const string& varPtr, const Dimensions& dims) const;

        // Create a deep copy of this expression,
        // except pointed-to var is not copied.
        virtual numExprPtr clone() const { return make_shared<VarPoint>(*this); }
        virtual varPointPtr cloneVarPoint() const { return make_shared<VarPoint>(*this); }

        // APIs.
        virtual yc_var* get_var();
    };

    // Equality operator for a var point.
    // This defines the LHS as equal to the RHS; it is NOT
    // a comparison operator; it is NOT an assignment operator.
    // It also holds an optional condition.
    class EqualsExpr : public Expr,
                       public virtual yc_equation_node {
    protected:
        varPointPtr _lhs;
        numExprPtr _rhs;
        boolExprPtr _cond;
        boolExprPtr _step_cond;

    public:
        EqualsExpr(varPointPtr lhs, numExprPtr rhs,
                   boolExprPtr cond = nullptr,
                   boolExprPtr step_cond = nullptr) :
            _lhs(lhs), _rhs(rhs), _cond(cond), _step_cond(step_cond) { }
        EqualsExpr(const EqualsExpr& src) :
            _lhs(src._lhs->cloneVarPoint()),
            _rhs(src._rhs->clone()) {
            if (src._cond)
                _cond = src._cond->clone();
            else
                _cond = nullptr;
            if (src._step_cond)
                _step_cond = src._step_cond->clone();
            else
                _step_cond = nullptr;
        }

        varPointPtr& getLhs() { return _lhs; }
        const varPointPtr& getLhs() const { return _lhs; }
        numExprPtr& getRhs() { return _rhs; }
        const numExprPtr& getRhs() const { return _rhs; }
        boolExprPtr& getCond() { return _cond; }
        const boolExprPtr& getCond() const { return _cond; }
        void setCond(boolExprPtr cond) { _cond = cond; }
        boolExprPtr& getStepCond() { return _step_cond; }
        const boolExprPtr& getStepCond() const { return _step_cond; }
        void setStepCond(boolExprPtr step_cond) { _step_cond = step_cond; }
        virtual string accept(ExprVisitor* ev);
        static string exprOpStr() { return "EQUALS"; }
        static string condOpStr() { return "IF_DOMAIN"; }
        static string stepCondOpStr() { return "IF_STEP"; }

        // Get pointer to var on LHS or NULL if not set.
        virtual Var* getVar() {
            if (_lhs.get())
                return _lhs->getVar();
            return NULL;
        }

        // LHS is scratch var.
        virtual bool isScratch();

        // Check for equivalency.
        virtual bool isSame(const Expr* other) const;

        // Create a deep copy of this expression.
        virtual equalsExprPtr clone() const { return make_shared<EqualsExpr>(*this); }
        virtual yc_equation_node_ptr clone_ast() const {
            return clone();
        }

        // APIs.
        virtual yc_var_point_node_ptr get_lhs() { return _lhs; }
        virtual yc_number_node_ptr get_rhs() { return _rhs; }
        virtual yc_bool_node_ptr get_cond() { return _cond; }
        virtual yc_bool_node_ptr get_step_cond() { return _step_cond; }
        virtual void set_cond(yc_bool_node_ptr cond) {
            if (cond) {
                auto p = dynamic_pointer_cast<BoolExpr>(cond);
                assert(p);
                _cond = p;
            } else
                _cond = nullptr;
        }
        virtual void set_step_cond(yc_bool_node_ptr step_cond) {
            if (step_cond) {
                auto p = dynamic_pointer_cast<BoolExpr>(step_cond);
                assert(p);
                _step_cond = p;
            } else
                _step_cond = nullptr;
        }
    };

    typedef set<VarPoint> VarPointSet;
    typedef set<varPointPtr> varPointPtrSet;
    typedef vector<VarPoint> VarPointVec;

} // namespace yask.

// Define hash function for VarPoint for unordered_{set,map}.
namespace std {
    using namespace yask;

    template <> struct hash<VarPoint> {
        size_t operator()(const VarPoint& k) const {
            return hash<string>{}(k.makeStr());
        }
    };
}
