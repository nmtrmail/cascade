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

#ifndef CASCADE_SRC_VERILOG_ANALYZE_CONSTANT_H
#define CASCADE_SRC_VERILOG_ANALYZE_CONSTANT_H

#include "verilog/ast/ast.h"
#include "verilog/ast/visitors/visitor.h"

namespace cascade {

// This class is used to determine whether an expression represents a compile-
// time constant as defined by the Verilog 2k5 Spec. It does not store any
// decorations in the AST, but does require up-to-date resolution decorations
// (see resolve.h) to function correctly.

class Constant : public Visitor {
  public:
    Constant();
    ~Constant() override = default;

    // Returns true if this variable can be statically evaluated at compile
    // time.  This implies that it contains only parameters and numeric
    // constants.
    bool is_static_constant(const Expression* e);
    // Returns true if this variable can be statically evaluated at elaboration
    // time. This implies that it contains only parameters, numeric constants,
    // and genvars.
    bool is_genvar_constant(const Expression* e);

  private:
    bool res_;
    bool genvar_ok_;

    // Visitor Interface:
    void visit(const FeofExpression* fe) override;
    void visit(const Identifier* i) override;
};

} // namespace cascade

#endif
