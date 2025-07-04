// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: LValue module/signal name references
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// Copyright 2003-2025 by Wilson Snyder. This program is free software; you
// can redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************
// LinkLValue TRANSFORMATIONS:
//      Top-down traversal
//          Set lvalue() attributes on appropriate VARREFs.
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3LinkLValue.h"

#include "V3Task.h"

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// Link state, as a visitor of each AstNode

class LinkLValueVisitor final : public VNVisitor {
    // NODE STATE

    // STATE - for current visit position (use VL_RESTORER)
    bool m_setContinuously = false;  // Set that var has some continuous assignment
    bool m_setForcedByCode = false;  // Set that var is the target of an AstAssignForce/AstRelease
    bool m_setIfRand = false;  // Update VarRefs if var declared as rand
    bool m_setStrengthSpecified = false;  // Set that var has assignment with strength specified.
    bool m_inFunc = false;  // Set if inside AstNodeFTask
    bool m_inInitialStatic = false;  // Set if inside AstInitialStatic
    VAccess m_setRefLvalue;  // Set VarRefs to lvalues for pin assignments

    // VISITORS
    // Result handing
    void visit(AstNodeVarRef* nodep) override {
        // VarRef: LValue its reference
        if (m_setIfRand && !(nodep->varp() && nodep->varp()->isRand())) return;
        if (m_setRefLvalue != VAccess::NOCHANGE) nodep->access(m_setRefLvalue);
        if (nodep->varp() && nodep->access().isWriteOrRW()) {
            if (nodep->varp()->isParam()) {
                // All parameters that did get constified happened before now
                // as V3LinkLValue runs after V3Param
                nodep->v3error("Storing to parameter variable "
                               << nodep->prettyNameQ()
                               << " in a context that is determined only at runtime");
            }
            if (m_setContinuously) {
                nodep->varp()->isContinuously(true);
                // Strength may only be specified in continuous assignment,
                // so it is needed to check only if m_setContinuously is true
                if (m_setStrengthSpecified) nodep->varp()->hasStrengthAssignment(true);
            }
            if (const AstClockingItem* const itemp
                = VN_CAST(nodep->varp()->backp(), ClockingItem)) {
                UINFO(5, "ClkOut " << nodep);
                if (itemp->outputp()) nodep->varp(itemp->outputp()->varp());
            }
            if (m_setForcedByCode) {
                nodep->varp()->setForcedByCode();
            } else if (!nodep->varp()->isFuncLocal() && nodep->varp()->isReadOnly()) {
                // This is allowed with IEEE 1800-2009 module input with default value.
                // the checking now happens in V3Width::visit(AstNodeVarRef*)
                // If you were to check here, it would fail on module inputs with default value,
                // because Inputs are isReadOnly()=true, and we don't yet have visibility into
                // it being an Initial style procedure.
            }
        }
        iterateChildren(nodep);
    }

    // Nodes that start propagating down lvalues
    void visit(AstPin* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        if (nodep->modVarp() && nodep->modVarp()->isWritable()) {
            // When the varref's were created, we didn't know the I/O state
            // Now that we do, and it's from a output, we know it's a lvalue
            m_setRefLvalue = VAccess::WRITE;
            iterateChildren(nodep);
            m_setRefLvalue = VAccess::NOCHANGE;
        } else {
            iterateChildren(nodep);
        }
    }
    void visit(AstNodeAssign* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        VL_RESTORER(m_setContinuously);
        VL_RESTORER(m_setStrengthSpecified);
        {
            m_setRefLvalue = VAccess::WRITE;
            m_setContinuously = VN_IS(nodep, AssignW) || VN_IS(nodep, AssignAlias);
            if (AstAssignW* assignwp = VN_CAST(nodep, AssignW)) {
                if (assignwp->strengthSpecp()) m_setStrengthSpecified = true;
            }
            {
                VL_RESTORER(m_setForcedByCode);
                m_setForcedByCode = VN_IS(nodep, AssignForce);
                iterateAndNextNull(nodep->lhsp());
            }
            m_setRefLvalue = VAccess::NOCHANGE;
            m_setContinuously = false;
            m_setStrengthSpecified = false;
            iterateAndNextNull(nodep->rhsp());
        }

        if (m_inInitialStatic && m_inFunc) {
            const bool rhsHasIO = nodep->rhsp()->exists([](const AstNodeVarRef* const refp) {
                // Exclude module I/O referenced from a function/task.
                return refp->varp() && refp->varp()->isIO()
                       && refp->varp()->lifetime() != VLifetime::NONE;
            });
            if (rhsHasIO) {
                nodep->rhsp()->v3warn(E_UNSUPPORTED,
                                      "Static variable initializer\n"
                                          << nodep->rhsp()->warnMore()
                                          << "is dependent on function/task I/O variable");
            } else {
                const bool rhsHasAutomatic
                    = nodep->rhsp()->exists([](const AstNodeVarRef* const refp) {
                          return refp->varp() && refp->varp()->lifetime() == VLifetime::AUTOMATIC;
                      });
                if (rhsHasAutomatic) {
                    nodep->rhsp()->v3error("Static variable initializer\n"
                                           << nodep->rhsp()->warnMore()
                                           << "is dependent on automatic variable");
                }
            }
        }
    }
    void visit(AstInitialStatic* nodep) override {
        VL_RESTORER(m_inInitialStatic);
        m_inInitialStatic = true;
        iterateChildren(nodep);
    }
    void visit(AstRelease* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        VL_RESTORER(m_setContinuously);
        VL_RESTORER(m_setForcedByCode);
        m_setRefLvalue = VAccess::WRITE;
        m_setContinuously = false;
        m_setForcedByCode = true;
        iterateAndNextNull(nodep->lhsp());
    }
    void visit(AstFireEvent* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->operandp());
    }
    void visit(AstCastDynamic* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->fromp());
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->top());
    }
    void visit(AstFError* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        iterateAndNextNull(nodep->filep());
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->strp());
    }
    void visit(AstFGetS* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        iterateAndNextNull(nodep->filep());
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->strgp());
    }
    void visit(AstFRead* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        iterateAndNextNull(nodep->filep());
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->memp());
    }
    void visit(AstFScanF* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        iterateAndNextNull(nodep->filep());
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->exprsp());
    }
    void visit(AstFUngetC* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        iterateAndNextNull(nodep->filep());
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->rhsp());
    }
    void visit(AstSScanF* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->exprsp());
    }
    void visit(AstSysIgnore* nodep) override {
        // Can't know if lvalue or not; presume not
        iterateChildren(nodep);
    }
    void visit(AstRand* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        if (!nodep->urandom()) m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->seedp());
    }
    void visit(AstReadMem* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->memp());
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->filenamep());
        iterateAndNextNull(nodep->lsbp());
        iterateAndNextNull(nodep->msbp());
    }
    void visit(AstTestPlusArgs* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->searchp());
    }
    void visit(AstValuePlusArgs* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->searchp());
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->outp());
    }
    void visit(AstSFormat* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->lhsp());
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->fmtp());
    }
    void visit(AstNodeDistBiop* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->lhsp());
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->rhsp());
    }
    void visit(AstNodeDistTriop* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->lhsp());
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->rhsp());
        iterateAndNextNull(nodep->thsp());
    }
    void prepost_visit(AstNodeTriop* nodep) {
        VL_RESTORER(m_setRefLvalue);
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->lhsp());
        iterateAndNextNull(nodep->rhsp());
        m_setRefLvalue = VAccess::WRITE;
        iterateAndNextNull(nodep->thsp());
    }
    void visit(AstPreAdd* nodep) override { prepost_visit(nodep); }
    void visit(AstPostAdd* nodep) override { prepost_visit(nodep); }
    void visit(AstPreSub* nodep) override { prepost_visit(nodep); }
    void visit(AstPostSub* nodep) override { prepost_visit(nodep); }

    // Nodes that change LValue state
    void visit(AstSel* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        iterateAndNextNull(nodep->fromp());
        // Only set lvalues on the from
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->lsbp());
    }
    void visit(AstNodeSel* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        // Only set lvalues on the from
        iterateAndNextNull(nodep->fromp());
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->bitp());
    }
    void visit(AstCellArrayRef* nodep) override {
        VL_RESTORER(m_setRefLvalue);
        // selp is not an lvalue
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->selp());
    }
    void visit(AstNodePreSel* nodep) override {
        if (AstSelBit* const selbitp = VN_CAST(nodep, SelBit)) selbitp->access(m_setRefLvalue);
        VL_RESTORER(m_setRefLvalue);
        // Only set lvalues on the from
        iterateAndNextNull(nodep->fromp());
        m_setRefLvalue = VAccess::NOCHANGE;
        iterateAndNextNull(nodep->rhsp());
        iterateAndNextNull(nodep->thsp());
    }
    void visit(AstMemberSel* nodep) override {
        if (m_setRefLvalue != VAccess::NOCHANGE) {
            nodep->access(m_setRefLvalue);
            if (nodep->varp() && nodep->access().isWriteOrRW()) {
                if (const AstClockingItem* const itemp
                    = VN_CAST(nodep->varp()->backp(), ClockingItem)) {
                    UINFO(5, "ClkOut " << nodep);
                    if (itemp->outputp()) nodep->varp(itemp->outputp()->varp());
                }
            }
        } else {
            // It is the only place where the access is set to member select nodes.
            // If it doesn't have to be set to WRITE, it means that it is READ.
            nodep->access(VAccess::READ);
        }
        iterateChildren(nodep);
    }
    void visit(AstNodeFTaskRef* nodep) override {
        const AstNodeFTask* const taskp = nodep->taskp();
        // We'll deal with mismatching pins later
        if (!taskp) return;
        const V3TaskConnects tconnects
            = V3Task::taskConnects(nodep, taskp->stmtsp(), nullptr, false);
        for (const auto& tconnect : tconnects) {
            const AstVar* const portp = tconnect.first;
            const AstArg* const argp = tconnect.second;
            if (!argp) continue;
            AstNodeExpr* const pinp = argp->exprp();
            if (!pinp) continue;
            if (portp->isWritable()) {
                VL_RESTORER(m_setRefLvalue);
                m_setRefLvalue = VAccess::WRITE;
                iterate(pinp);
            } else {
                iterate(pinp);
            }
        }
    }
    void visit(AstConstraint* nodep) override {
        VL_RESTORER(m_setIfRand);
        m_setIfRand = true;
        iterateChildren(nodep);
    }
    void visit(AstNodeFTask* nodep) override {
        VL_RESTORER(m_inFunc);
        m_inFunc = true;
        iterateChildren(nodep);
    }

    void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    // CONSTRUCTORS
    LinkLValueVisitor(AstNode* nodep, VAccess start)
        : m_setRefLvalue{start} {
        iterate(nodep);
    }
    ~LinkLValueVisitor() override = default;
};

//######################################################################
// Link class functions

void V3LinkLValue::linkLValue(AstNetlist* nodep) {
    UINFO(4, __FUNCTION__ << ": ");
    { LinkLValueVisitor{nodep, VAccess::NOCHANGE}; }  // Destruct before checking
    V3Global::dumpCheckGlobalTree("linklvalue", 0, dumpTreeEitherLevel() >= 6);
}
void V3LinkLValue::linkLValueSet(AstNode* nodep) {
    // Called by later link functions when it is known a node needs
    // to be converted to a lvalue.
    UINFO(9, __FUNCTION__ << ": ");
    { LinkLValueVisitor{nodep, VAccess::WRITE}; }
}
