// Copyright 2017-2019 VMware, Inc.
// SPDX-License-Identifier: BSD-2-Clause
//
// The BSD-2 license (the License) set forth below applies to all parts of the
// Cascade project.  You may not use this file except in compliance with the
// License.
//
// BSD-2 License
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "verilog/program/type_check.h"

#include <sstream>
#include "common/log.h"
#include "verilog/analyze/constant.h"
#include "verilog/analyze/evaluate.h"
#include "verilog/analyze/module_info.h"
#include "verilog/analyze/navigate.h"
#include "verilog/analyze/read_set.h"
#include "verilog/analyze/resolve.h"
#include "verilog/ast/ast.h"
#include "verilog/parse/parser.h"
#include "verilog/print/print.h"
#include "verilog/program/elaborate.h"
#include "verilog/program/program.h"

using namespace std;

namespace cascade {

TypeCheck::TypeCheck(const Program* program, Log* log, const Parser* parser) : Visitor() { 
  program_ = program;
  log_ = log;
  parser_ = parser;

  deactivate(false);
  declaration_check(true);
  local_only(true);

  outermost_loop_ = nullptr;
  instantiation_ = nullptr;
  net_lval_ = false;

  exists_bad_id_ = false;
}

void TypeCheck::deactivate(bool val) {
  deactivated_ = val;
}

void TypeCheck::declaration_check(bool val) {
  decl_check_ = val;
}

void TypeCheck::local_only(bool val) {
  local_only_ = val;
}

void TypeCheck::pre_elaboration_check(const ModuleInstantiation* mi) {
  if (deactivated_) {
    return;
  }

  // RECURSE: Implicit params and ports
  const auto backup = exists_bad_id_;
  for (auto i = mi->begin_params(), ie = mi->end_params(); i != ie; ++i) {
    (*i)->accept_imp(this);
  }
  for (auto i = mi->begin_ports(), ie = mi->end_ports(); i != ie; ++i) {
    (*i)->accept_imp(this);
  }
  // EXIT: Arity checking will fail without access to resolvable ports
  if (exists_bad_id_) {
    exists_bad_id_ = backup;
    return;
  }

  // CHECK: Array properties
  check_width(mi->get_range());

  // CHECK: Duplicate definition for instantiations other than the root
  if (!Navigate(mi).lost()) {
    if (Navigate(mi).find_duplicate_name(mi->get_iid()->back_ids())) {
      multiple_def(mi->get_iid());
    }
    if (Navigate(mi).find_child_ignore_subscripts(mi->get_iid()->back_ids())) {
      error("A nested scope with this name already exists in this scope", mi);
    }
  }
  // CHECK: TODO(eschkufz) Recursive instantiation

  // CHECK: Undefined type
  const auto itr = program_->decl_find(mi->get_mid());
  if (itr == program_->decl_end()) {
    if (decl_check_) {
      warn("Instantiation refers to an undeclared module, this may result in an error during elaboration", mi);
    } else {
      error("Instantiation refers to an undeclared module", mi);
    }
    // EXIT: Can't continue checking without access to source
    return;
  }
  const auto* src = itr->second;

  // CHECK: More overrides than declared parameters
  if (mi->size_params() > ModuleInfo(src).ordered_params().size()) {
    error("Instantiation contains more parameter overrides than appear in module declaration", mi);
  }
  // CHECK: Duplicate or unrecognized parameters
  if (mi->uses_named_params()) {
    const auto& np = ModuleInfo(src).named_params();
    unordered_set<const Identifier*, HashId, EqId> params;
    for (auto i = mi->begin_params(), ie = mi->end_params(); i != ie; ++i) {
      if (!params.insert((*i)->get_exp()).second) {
        error("Instantiation contains duplicate named params", mi);
      }
      if (np.find((*i)->get_exp()) == np.end()) {
        error("Instantiation contains a reference to unresolvable parameter", mi);
      }
    }
  }

  // CHECK: More connections than declared ports
  if (mi->size_ports() > ModuleInfo(src).ordered_ports().size()) {
    error("Instantiation contains more connections than appear in module declaration", mi);
  }
  // CHECK: Duplicate or unrecognized port connections 
  // CHECK: expressions connected to outputs
  // CHECK: Arity mismatch for instance arrays
  if (mi->uses_named_ports()) {
    const auto& np = ModuleInfo(src).named_ports();
    unordered_set<const Identifier*, HashId, EqId> ports;
    for (auto i = mi->begin_ports(), ie = mi->end_ports(); i != ie; ++i) {
      if (!ports.insert((*i)->get_exp()).second) {
        error("Instantiation contains duplicate named connections", mi);
      }
      const auto nitr = np.find((*i)->get_exp());
      if (nitr == np.end()) {
        error("Instantiation contains a reference to an unresolvable explicit port", mi);
        continue;
      }
      if ((*i)->is_null_imp()) {
        continue;
      }
      if (ModuleInfo(src).is_output(*nitr) && !(*i)->get_imp()->is(Node::Tag::identifier)) {
        error("Instantiation contains a connection between an expression and a named output port", mi);
      }
      check_arity(mi, *nitr, (*i)->get_imp());
    }
  } 
  if (!mi->uses_named_ports()) {
    const auto& op = ModuleInfo(src).ordered_ports();
    for (size_t i = 0, ie = min(op.size(), mi->size_ports()); i < ie; ++i) {
      const auto* p = mi->get_ports(i);
      if (p->is_null_imp()) {
        continue;
      }
      if (ModuleInfo(src).is_output(op[i]) && !p->get_imp()->is(Node::Tag::identifier)) {
        error("Instantiation contains a connection between an expression and an ordered output port", mi);
      }
      check_arity(mi, op[i], p->get_imp());
    }
  }
}

void TypeCheck::pre_elaboration_check(const CaseGenerateConstruct* cgc) {
  if (deactivated_) {
    return;
  }
  // CHECK: Constant guard expression 
  if (!Constant().is_static_constant(cgc->get_cond())) {
    error("Non-constant expression appears in the guard for a case generate construct", cgc->get_cond());
  }
  // RECURSE: condition
  cgc->accept_cond(this);
}

void TypeCheck::pre_elaboration_check(const IfGenerateConstruct* igc) {
  if (deactivated_) {
    return;
  }
  // CHECK: Constant guard expression 
  for (auto i = igc->begin_clauses(), ie = igc->end_clauses(); i != ie; ++i) {
    if (!Constant().is_static_constant((*i)->get_if())) {
      error("Non-constant expression appears in the guard for an if generate construct", (*i)->get_if());
    }
    // RECURSE: condition
    (*i)->accept_if(this);
  }
}

void TypeCheck::pre_elaboration_check(const LoopGenerateConstruct* lgc) {
  // If this is the outermost loop, set the location
  if (outermost_loop_ == nullptr) {
    outermost_loop_ = lgc;
  }

  if (!deactivated_) {
    // RECURSE: Iteration space
    lgc->accept_init(this);
    lgc->accept_cond(this);
    lgc->accept_update(this);

    // CHECK: Duplicate name
    if (lgc->get_block()->is_non_null_id()) {
      const auto* id = lgc->get_block()->get_id();
      Navigate nav(id);
      nav.up();
      if (nav.find_name(id->back_ids())) {
        multiple_def(id);
      }
      if (nav.find_child_ignore_subscripts(id->back_ids())) {
        error("A nested scope with this name already exists in this scope", id);
      }
    }

    // CHECK: Const loop guard
    if (!Constant().is_genvar_constant(lgc->get_cond())) {
      error("Non-constant expression appears in the guard for a loop generate construct", lgc->get_cond());
    }
    // CHECK: Initialization and update refer to same variable
    const auto* r1 = Resolve().get_resolution(lgc->get_init()->get_lhs());
    const auto* r2 = Resolve().get_resolution(lgc->get_update()->get_lhs());
    if (r1 != r2) {
      error("Initialization and update statements refer to different variables in loop generate construct", lgc->get_update()->get_lhs());
    }
  }

  // If this is the outermost loop, we're about to emerge from it
  if (outermost_loop_ == lgc) {
    outermost_loop_ = nullptr;
  }
}

void TypeCheck::post_elaboration_check(const Node* n) {
  if (deactivated_) {
    return;
  }
  n->accept(this);
}

void TypeCheck::warn(const string& s, const Node* n) {
  stringstream ss;

  auto* ptr = n;
  if (decl_check_) {
    ss << "In module declaration ";
  } else if (instantiation_ != nullptr) {
    ss << "In module instantiation ";
    ptr = instantiation_;
  } else {
    ss << "In module item ";
  }

  if (parser_ == nullptr) {
    ss << "in <unable to access location --- contact developers>: ";
  } else {
    const auto loc = parser_->get_loc(ptr);
    if (loc.first == "<top>") {
      ss << "in final line of user input: ";
    } else {
      ss << "in " << loc.first << " on line " << loc.second << ": ";
    }
  }
  
  ss << ptr << "\n" << s;
  if (ptr != n) {
    ss << ", see previous warnings for more information";
  }

  log_->warn(ss.str());
}

void TypeCheck::error(const string& s, const Node* n) {
  stringstream ss;

  auto* ptr = n;
  if (decl_check_) {
    ss << "In module declaration ";
  } else if (instantiation_ != nullptr) {
    ss << "In module instantiation ";
    ptr = instantiation_;
  } else {
    ss << "In module item ";
  }

  if (parser_ == nullptr) {
    ss << "in <unable to access location --- contact developers>: ";
  } else {
    const auto loc = parser_->get_loc(ptr);
    if (loc.first == "<top>") {
      ss << "in final line of user input: ";
    } else {
      ss << "in " << loc.first << " on line " << loc.second << ": ";
    }
  }
  
  ss << ptr << "\n" << s;
  if (ptr != n) {
    ss << ", see previous warnings for more information";
  }

  log_->error(ss.str());
}

void TypeCheck::multiple_def(const Node* n) {
  error("A variable with this name already appears in this scope", n);
}

void TypeCheck::visit(const Event* e) {
  Visitor::visit(e);
  if (!e->get_expr()->is(Node::Tag::identifier)) {
    error("Cascade does not currently support expressions inside event controls", e);
  }
}

void TypeCheck::visit(const Identifier* id) {
  // CHECK: Are instance selects constant expressions?
  for (auto i = id->begin_ids(), ie = id->end_ids(); i != ie; ++i) {
    if ((*i)->is_non_null_isel() && !Constant().is_static_constant((*i)->get_isel())) {
      error("Found non-constant expression in instance select", id);
    }
  }

  // RECURSE: ids and dim
  auto backup = exists_bad_id_;
  id->accept_ids(this);
  id->accept_dim(this);
  // EXIT: Resolution will fail if there's a bad id below here
  if (exists_bad_id_) {
    exists_bad_id_ = backup;
    return;
  }

  // CHECK: Reference to undefined identifier
  const auto* r = Resolve().get_resolution(id);
  if (r == nullptr) {
    exists_bad_id_ = true;
    if (decl_check_) {
      warn("Found reference to unresolvable identifier, this may result in an error during instantiation", id);
    } else {
      error("Found reference to an unresolvable identifier", id);
    }
  }
  // CHECK: Is this a reference to a genvar outside of a loop generate construct?
  if ((outermost_loop_ == nullptr) && (r != nullptr) && r->get_parent()->is(Node::Tag::genvar_declaration)) {
    error("Found reference to a genvar outside of a loop generate construct", id);
  }
  // EXIT: Can't continue checking if id can't be resolved
  if (r == nullptr) {
    return;
  }

  // CHECK: Are subscripts valid, if provided?
  auto cdr = check_deref(r, id);
  // Nothing else to do if we're out of dimensions
  if (cdr == id->end_dim()) {
    return;
  }

  // CHECK: Are we providing a bit-select for a single-bit value?
  if (Evaluate().get_width(r) == 1) {
    return error("Found bit- or part-select in dereference of variable which was declared scalar", id);
  }
  // CHECK: Are we providing a bit-select for a real value?
  if (static_cast<const Declaration*>(r->get_parent())->get_type() == Declaration::Type::REAL) {
    return error("Found bit- or part-select in dereference of variable which was declared real", id);
  }

  // CHECK: Range expression bit-selects
  if ((*cdr)->is(Node::Tag::range_expression)) {
    const auto* re = static_cast<const RangeExpression*>(*cdr);
    if (re->get_type() == RangeExpression::Type::CONSTANT) {
      // CHECK: Non-constant values, values out of range, little-endian ranges
      // EXIT: We can't continue checking if we can't evaluate this range
      if (!Constant().is_static_constant(re)) {
        return error("Found non-constant value in constant part-select", id);
      }
      const auto rng = Evaluate().get_range(re);
      if (rng.first < rng.second) {
        return error("Cascade does not currently support big-endian part-selects", id);
      } 
    } else {
      if (!Constant().is_static_constant(re->get_lower())) {
        return error("Found non-constant width in indexed part-select", id);
      }
    }
  }
  if (!Constant().is_static_constant(*cdr)) {
    // ERROR: Is this a non-constant in a net_lval?
    if (net_lval_) {
      error("Found non-constant bit- or part-select in target of continuous assignment", *cdr);
    }
  }
  else {
    // WARN: Can we say for sure that selects are out of range
    const auto rng = Evaluate().get_range(*cdr);
    if (((rng.first > Evaluate().get_msb(r)) || (rng.second > Evaluate().get_msb(r))) && !decl_check_) {
      warn("Found bit- or part-select outside the upper-bound of the declared indices for this variable", id);
    }
    if (((rng.first < Evaluate().get_lsb(r)) || (rng.second < Evaluate().get_lsb(r))) && !decl_check_) {
      warn("Found bit- or part-select outside the lower-bound of the declared indices for this variable", id);
    }
  } 
}

void TypeCheck::visit(const GenerateBlock* gb) {
  // RECURSE: items
  gb->accept_items(this);
}

void TypeCheck::visit(const AlwaysConstruct* ac) {
  if (!ac->get_stmt()->is(Node::Tag::timing_control_statement)) {
    return error("Cascade does not currently support the use of always blocks which don't contain timing control statements", ac);
  }
  // RECURSE:
  Visitor::visit(ac);
}

void TypeCheck::visit(const ModuleDeclaration* md) {
  // CHECK: implicit or explict ports that are not simple ids
  for (auto i = md->begin_ports(), ie = md->end_ports(); i != ie; ++i) {
    if ((*i)->is_non_null_exp()) {
      error("Cascade does not currently support the use of explicit ports in module declarations", *i);
    }
    if ((*i)->is_null_imp()) {
      error("Found a missing implicit port in module declaration", md);
    }
    if (!(*i)->get_imp()->is(Node::Tag::identifier)) {
      error("Cascade does not currently support the use of implicit ports which are not identifiers", *i);
    } else {
      // RECURSE: implicit port
      static_cast<const Identifier*>((*i)->get_imp())->accept(this);
    }
  }

  // RECURSE: items
  md->accept_items(this);
}

void TypeCheck::visit(const CaseGenerateConstruct* cgc) {
  if (local_only_) {
    return;
  }
  if (Elaborate().is_elaborated(cgc)) {
    Elaborate().get_elaboration(cgc)->accept(this);
  }
}

void TypeCheck::visit(const IfGenerateConstruct* igc) {
  if (local_only_) {
    return;
  }
  if (Elaborate().is_elaborated(igc)) {
    Elaborate().get_elaboration(igc)->accept(this);
  }
}

void TypeCheck::visit(const LoopGenerateConstruct* lgc) {
  if (local_only_) {
    return;
  }
  if (Elaborate().is_elaborated(lgc)) {
    for (auto* b : Elaborate().get_elaboration(lgc)) {
      b->accept(this);
    }
  }
}

void TypeCheck::visit(const InitialConstruct* ic) {
  // RECURSE: body
  ic->accept_stmt(this);
}

void TypeCheck::visit(const ContinuousAssign* ca) {
  net_lval_ = true;
  ca->accept_lhs(this);
  net_lval_ = false;
  ca->accept_rhs(this);

  ReadSet rs(ca->get_rhs());
  for (auto i = ca->begin_lhs(), ie = ca->end_lhs(); i != ie; ++i) {
    const auto* l = Resolve().get_resolution(*i);
    if (l == nullptr) {
      return;
    }
    if (!l->get_parent()->is(Node::Tag::net_declaration)) {
      error("Continuous assignments are only permitted for variables with type wire", ca);
    }

    // CHECK: Recursive assignment
    for (auto j = rs.begin(), je = rs.end(); j != je; ++j) {
      if ((*j)->is(Node::Tag::identifier)) {
        // If this identifier resolves to the left-hand side, this might be a
        // recursive definition.
        const auto* id = static_cast<const Identifier*>(*j);
        const auto* r = Resolve().get_resolution(id);
        if ((r == nullptr) || (r != l)) {
          continue;
        }

        // If both sides are scalar, it's definitely recursive.
        if (l->empty_dim() && id->empty_dim()) {
          error("Cannot assign a wire to itself", ca);
        } 
        // The alternative is more complicated. And there are some cases we can
        // check statically, but for now let's just emit a blanket warning.
        else {
          warn("Found a potentially zero-time assignment from a variable to iteself", ca);
        }
      }
    }
  }
}

void TypeCheck::visit(const GenvarDeclaration* gd) {
  // CHECK: Duplicate definition
  if (Navigate(gd).find_duplicate_name(gd->get_id()->back_ids())) {
    multiple_def(gd->get_id());
  }
  if (Navigate(gd).find_child_ignore_subscripts(gd->get_id()->back_ids())) {
    error("A nested scope with this name already exists in this scope", gd);
  }
}

void TypeCheck::visit(const LocalparamDeclaration* ld) {
  // RECURSE: Check for unsupported language features in initial value
  ld->accept_val(this);

  // CHECK: Duplicate definition
  if (Navigate(ld).find_duplicate_name(ld->get_id()->back_ids())) {
    multiple_def(ld->get_id());
  }
  if (Navigate(ld).find_child_ignore_subscripts(ld->get_id()->back_ids())) {
    error("A nested scope with this name already exists in this scope", ld);
  }
  // CHECK: Width properties
  check_width(ld->get_dim());
  // CHECK: Parameter initialized to constant value
  if (!Constant().is_static_constant(ld->get_val())) {
    error("Localparam initialization requires constant value", ld);
  }

  // CHECK: Recursive definition
  // Iterate over identifiers in the RHS
  ReadSet rs(ld->get_val());
  for (auto i = rs.begin(), ie = rs.end(); i != ie; ++i) {
    if ((*i)->is(Node::Tag::identifier)) {
      // If this identifier resolves to the left-hand side, this is a recursive definition
      const auto* id = static_cast<const Identifier*>(*i);
      const auto* r = Resolve().get_resolution(id);
      assert(r != nullptr);
      if (r == ld->get_id()) {
        error("Cannot define a localparam to be equal to itself", ld);
      }
    }
  }
}

void TypeCheck::visit(const NetDeclaration* nd) {
  // CHECK: Duplicate definition
  if (Navigate(nd).find_duplicate_name(nd->get_id()->back_ids())) {
    multiple_def(nd->get_id());
  }
  if (Navigate(nd).find_child_ignore_subscripts(nd->get_id()->back_ids())) {
    error("A nested scope with this name already exists in this scope", nd);
  }
  // CHECK: Width and array properties
  check_width(nd->get_dim());
  check_array(nd->get_id()->begin_dim(), nd->get_id()->end_dim());
}

void TypeCheck::visit(const ParameterDeclaration* pd) {
  // RECURSE: Check for unsupported language features in initial value
  pd->accept_val(this);

  // CHECK: Duplicate definition
  if (Navigate(pd).find_duplicate_name(pd->get_id()->back_ids())) {
    multiple_def(pd->get_id());
  }
  if (Navigate(pd).find_child_ignore_subscripts(pd->get_id()->back_ids())) {
    error("A nested scope with this name already exists in this scope", pd);
  }
  // CHECK: Width properties
  check_width(pd->get_dim());
  // CHECK: Parameter initialized to constant value
  if (!Constant().is_static_constant(pd->get_val())) {
    error("Parameter initialization requires constant value", pd);
  }

  // CHECK: Recursive definition
  // Iterate over identifiers in the RHS
  ReadSet rs(pd->get_val());
  for (auto i = rs.begin(), ie = rs.end(); i != ie; ++i) {
    if ((*i)->is(Node::Tag::identifier)) {
      // If this identifier resolves to the left-hand side, this is a recursive definition
      const auto* id = static_cast<const Identifier*>(*i);
      const auto* r = Resolve().get_resolution(id);
      assert(r != nullptr);
      if (r == pd->get_id()) {
        error("Cannot define a parameter to be equal to itself", pd);
      }
    }
  }
}

void TypeCheck::visit(const RegDeclaration* rd) {
  // RECURSE: Check for unsupported language features in initial value
  rd->accept_val(this);

  // CHECK: Duplicate definition
  if (Navigate(rd).find_duplicate_name(rd->get_id()->back_ids())) {
    multiple_def(rd->get_id());
  }
  if (Navigate(rd).find_child_ignore_subscripts(rd->get_id()->back_ids())) {
    error("A nested scope with this name already exists in this scope", rd);
  }
  // CHECK: Width and array properties
  check_width(rd->get_dim());
  check_array(rd->get_id()->begin_dim(), rd->get_id()->end_dim());
  // CHECK: Registers initialized to constant value
  if (rd->is_non_null_val() && !Constant().is_static_constant(rd->get_val())) {
    error("Register initialization requires constant value", rd);
  }
}

void TypeCheck::visit(const ModuleInstantiation* mi) {
  if (local_only_) {
    return;
  }
  if (Elaborate().is_elaborated(mi)) {
    instantiation_ = mi;
    Elaborate(program_).get_elaboration(mi)->accept(this);
    instantiation_ = nullptr;
  }
}

void TypeCheck::visit(const ParBlock* pb) {
  // CHECK: Upper bound on number of statements imposed by sw_logic
  if (pb->size_stmts() > 0xffff) {
    error("Cascade does not currently support parallel blocks with more than 2^16 statements", pb);
  }

  // CHECK: TODO(eschkufz) Duplicate definition
  // RECURSE: decls and body
  pb->accept_decls(this);
  pb->accept_stmts(this);
}

void TypeCheck::visit(const SeqBlock* sb) {
  // CHECK: Upper bound on number of statements imposed by sw_logic
  if (sb->size_stmts() > 0xffff) {
    error("Cascade does not currently support sequential blocks with more than 2^16 statements", sb);
  }

  // CHECK: TODO(eschkufz) Duplicate definition
  // RECURSE: decls and body
  sb->accept_decls(this);
  sb->accept_stmts(this);
}

void TypeCheck::visit(const BlockingAssign* ba) {
  // RECURSE: 
  Visitor::visit(ba);
  // CHECK: Target must be register or integer
  for (auto i = ba->begin_lhs(), ie = ba->end_lhs(); i != ie; ++i) {
    const auto* r = Resolve().get_resolution(*i);
    if ((r != nullptr) && !r->get_parent()->is(Node::Tag::reg_declaration)) {
      error("Found a blocking assignments to a variable with type other than reg", ba);
    }
  }
}

void TypeCheck::visit(const NonblockingAssign* na) {
  // RECURSE:
  Visitor::visit(na);
  // CHECK: Target must be register or integer
  for (auto i = na->begin_lhs(), ie = na->end_lhs(); i != ie; ++i) {
    const auto* r = Resolve().get_resolution(*i);
    if ((r != nullptr) && !r->get_parent()->is(Node::Tag::reg_declaration)) {
      error("Found a non-blocking assignments to a variable with type other than reg", na);
    }
  }
}

void TypeCheck::visit(const CaseStatement* cs) {
  Visitor::visit(cs);
}

void TypeCheck::visit(const ConditionalStatement* cs) {
  Visitor::visit(cs);
}

void TypeCheck::visit(const ForStatement* fs) {
  warn("Cascade attempts to statically unroll all loop statements and may hang if it is not possible to do so", fs);
  Visitor::visit(fs);
}

void TypeCheck::visit(const RepeatStatement* rs) {
  warn("Cascade attempts to statically unroll all loop statements and may hang if it is not possible to do so", rs);
  Visitor::visit(rs);
}

void TypeCheck::visit(const WhileStatement* ws) {
  warn("Cascade attempts to statically unroll all loop statements and may hang if it is not possible to do so", ws);
  Visitor::visit(ws);
}

void TypeCheck::visit(const DebugStatement* ds) {
  // Don't descend beyond here
  (void) ds;
}

void TypeCheck::visit(const GetStatement* gs) {
  gs->accept_fd(this);
  // Don't descend on format string
  gs->accept_var(this);
  if (gs->is_null_var()) {
    return;
  }

  // Can't continue checking if pointers are unresolvable
  const auto* r = Resolve().get_resolution(gs->get_var());
  if (r == nullptr) {
    return;
  }
  // CHECK: var is a stateful variable
  const auto* src = Resolve().get_parent(r);
  ModuleInfo info(src);
  if (!r->get_parent()->is(Node::Tag::reg_declaration)) {
    error("The target of a $get() statement must be a variable of type reg", gs);
  }
}

void TypeCheck::visit(const PutStatement* ps) {
  ps->accept_fd(this);
  // Don't descend on format string.
  ps->accept_expr(this);
}

void TypeCheck::visit(const VariableAssign* va) {
  // RECURSE:
  Visitor::visit(va);
  // CHECK: Aassignments which target concatenations
  if (va->size_lhs() > 1) {
    return error("Cascade does not currently support the use of variable assigns which target concatenations!", va);
  }
}

void TypeCheck::check_width(const RangeExpression* re) {
  if (re == nullptr) {
    return;
  }

  // RECURSE: First check the contents of this expression
  const auto backup = exists_bad_id_;
  re->accept(this);
  // EXIT: Evaluation will fail if there's a bad id below here
  if (exists_bad_id_) {
    exists_bad_id_ = backup;
    return;
  }

  const auto rng = Evaluate().get_range(re);
  if (rng.first <= rng.second) {
    error("Cascade does not currently support little-endian vector declarations", re);
  }
}

void TypeCheck::check_array(Identifier::const_iterator_dim begin, Identifier::const_iterator_dim end) {
  for (auto i = begin; i != end; ++i) {
    // CHECK: Array bounds must be constants
    if (!Constant().is_static_constant(*i)) {
      return error("Found a non-constant expression in an array declaration", (*i)->get_parent()->get_parent());
    }
    // RECURSE: Check the contents of this array bound
    const auto backup = exists_bad_id_;
    (*i)->accept(this);
    // EXIT: Evaluation will fail if there's a bad id below here
    if (exists_bad_id_) {
      exists_bad_id_ = backup;
      return;
    }
   
    const auto rng = Evaluate().get_range(*i);
    if (rng.first <= rng.second) {
      return error("Cascade does not currently support little-endian array declarations", (*i)->get_parent()->get_parent());
    } 
  }
}

Identifier::const_iterator_dim TypeCheck::check_deref(const Identifier* r, const Identifier* i) {
  const int diff = i->size_dim() - r->size_dim();

  // We need to have at least as many subscripts on i as r is declared with
  if (diff < 0) {
    error("Found an array dereference with more subscripts than appear in the declaration for this variable", i);
    return i->end_dim();
  }
  // If we have the same number, the last subscript on i can't be a range
  // There's also nothing to do if the subscripts are both empty
  else if (diff == 0) {
    if (i->empty_dim()) {
      return i->end_dim();
    } else if (i->back_dim()->is(Node::Tag::range_expression)) {
      error("Found a range expression found where a scalar subscript was expected", i);
      return i->end_dim();
    }
  }
  // And if i has more than one too many, that's an error as well
  else if (diff > 1) {
    error("Found an array dereference with more subscripts than appear in the declaration for this variable", i);
    return i->end_dim();
  } 
  
  // Iterate over subscripts
  auto iitr = i->begin_dim();
  auto ritr = r->begin_dim();
  for (auto re = r->end_dim(); ritr != re; ++iitr, ++ritr) {
    if (!Constant().is_static_constant(*iitr)) {
      // ERROR: Is this a non-constant subscript in a net-lval?
      if (net_lval_) {
        error("Found non-constant array subscript in target of continuous assignment", *iitr);
      } 
    }
    // WARN: Can we say for sure that this value is out of range?
    else {
      const auto rng = Evaluate().get_range(*ritr);
      if ((Evaluate().get_value(*iitr).to_uint() > rng.first) && !decl_check_) {
        warn("Array subscript is outside the upper-bound of a declared dimension for this variable", *iitr);
      }
      if ((Evaluate().get_value(*iitr).to_uint() < rng.second) && !decl_check_) {
        warn("Array subscript is outside the lower-bound of a declared dimension for this variable", *iitr);
      }
    }
  }    
  return iitr;
}

void TypeCheck::check_arity(const ModuleInstantiation* mi, const Identifier* port, const Expression* arg) {
  // Nothing to do if this is a scalar instantiation
  if (mi->is_null_range()) {
    return;
  }

  // TODO(eschkufz) REMOVE THIS CHECK TO ACTIVATE SUPPORT FOR INSTANTIATION ARRAYS
  return error("Cascade does not currently provide support for instantiation arrays", mi);

  // Ports and arguments with matching widths are okay
  const auto pw = Evaluate().get_width(port);
  const auto aw = Evaluate().get_width(arg);
  if (pw == aw) {
    return;
  }
  // Arguments that divide evenly by number of instances are okay
  const auto mw = Evaluate().get_range(mi->get_range()).first + 1;
  if (((aw % mw) == 0) && ((aw / mw) == pw)) {
    return;
  }

  // Anything else is an error
  error("Found an arity mismatch between module array instantiation and the declared width of one of its ports", mi);
}

} // namespace cascade
