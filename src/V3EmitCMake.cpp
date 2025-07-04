// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Emit CMake file list
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// Copyright 2004-2025 by Wilson Snyder. This program is free software; you
// can redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3EmitCMake.h"

#include "V3EmitCBase.h"
#include "V3HierBlock.h"
#include "V3Os.h"

#include <memory>

VL_DEFINE_DEBUG_FUNCTIONS;

// ######################################################################
//  Emit statements

class CMakeEmitter final {

    // METHODS

    // STATIC FUNCTIONS

    // Concatenate all strings in 'strs' with ' ' between them.
    template <typename T_List>
    static string cmake_list(const T_List& strs) {
        string s;
        for (auto& itr : strs) {
            if (!s.empty()) s += ' ';
            s += '"';
            s += V3OutFormatter::quoteNameControls(itr);
            s += '"';
        }
        return s;
    }
    static string cmake_list(const VFileLibList& strs) {
        string s;
        for (auto& itr : strs) {
            if (!s.empty()) s += ' ';
            s += '"';
            s += V3OutFormatter::quoteNameControls(itr.filename());
            s += '"';
        }
        return s;
    }

    // Print CMake variable set command: output raw_value unmodified
    // cache_type should be empty for a normal variable
    // "BOOL", "FILEPATH", "PATH", "STRING" or "INTERNAL" for a CACHE variable
    // See https://cmake.org/cmake/help/latest/command/set.html
    static void cmake_set_raw(std::ofstream& of, const string& name, const string& raw_value,
                              const string& cache_type = "", const string& docstring = "") {
        of << "set(" << name << " " << raw_value;
        if (!cache_type.empty()) of << " CACHE " << cache_type << " \"" << docstring << '"';
        of << ")\n";
    }

    static void cmake_set(std::ofstream& of, const string& name, const string& value,
                          const string& cache_type = "", const string& docstring = "") {
        const string raw_value = '"' + value + '"';
        cmake_set_raw(of, name, raw_value, cache_type, docstring);
    }

    static void emitOverallCMake() {
        const std::unique_ptr<std::ofstream> of{
            V3File::new_ofstream(v3Global.opt.makeDir() + "/" + v3Global.opt.prefix() + ".cmake")};
        const string name = v3Global.opt.prefix();

        *of << "# Verilated -*- CMake -*-\n";
        *of << "# DESCR"
               "IPTION: Verilator output: CMake include script with class lists\n";
        *of << "#\n";
        *of << "# This CMake script lists generated Verilated files, for "
               "including in higher level CMake scripts.\n";
        *of << "# This file is meant to be consumed by the verilate() function,\n";
        *of << "# which becomes available after executing `find_package(verilator).\n";

        *of << "\n### Constants...\n";
        cmake_set(*of, "PERL", V3OutFormatter::quoteNameControls(V3Options::getenvPERL()),
                  "FILEPATH", "Perl executable (from $PERL, defaults to 'perl' if not set)");
        cmake_set(*of, "PYTHON3", V3OutFormatter::quoteNameControls(V3Options::getenvPYTHON3()),
                  "FILEPATH",
                  "Python3 executable (from $PYTHON3, defaults to 'python3' if not set)");
        cmake_set(*of, "VERILATOR_ROOT",
                  V3OutFormatter::quoteNameControls(V3Options::getenvVERILATOR_ROOT()), "PATH",
                  "Path to Verilator kit (from $VERILATOR_ROOT)");
        cmake_set(*of, "VERILATOR_SOLVER",
                  V3OutFormatter::quoteNameControls(V3Options::getenvVERILATOR_SOLVER()), "STRING",
                  "Default SMT solver for constrained randomization (from $VERILATOR_SOLVER)");

        *of << "\n### Compiler flags...\n";

        *of << "# User CFLAGS (from -CFLAGS on Verilator command line)\n";
        cmake_set_raw(*of, name + "_USER_CFLAGS", cmake_list(v3Global.opt.cFlags()));

        *of << "# User LDLIBS (from -LDFLAGS on Verilator command line)\n";
        cmake_set_raw(*of, name + "_USER_LDLIBS", cmake_list(v3Global.opt.ldLibs()));

        *of << "\n### Switches...\n";

        *of << "# SystemC output mode?  0/1 (from --sc)\n";
        cmake_set_raw(*of, name + "_SC", v3Global.opt.systemC() ? "1" : "0");
        *of << "# Coverage output mode?  0/1 (from --coverage)\n";
        cmake_set_raw(*of, name + "_COVERAGE", v3Global.opt.coverage() ? "1" : "0");
        *of << "# Timing mode?  0/1\n";
        cmake_set_raw(*of, name + "_TIMING", v3Global.usesTiming() ? "1" : "0");
        *of << "# Threaded output mode?  1/N threads (from --threads)\n";
        cmake_set_raw(*of, name + "_THREADS", cvtToStr(v3Global.opt.threads()));
        *of << "# FST Tracing output mode? 0/1 (from --trace-fst)\n";
        cmake_set_raw(*of, name + "_TRACE_FST", (v3Global.opt.traceEnabledFst()) ? "1" : "0");
        *of << "# SAIF Tracing output mode? 0/1 (from --trace-saif)\n";
        cmake_set_raw(*of, name + "_TRACE_SAIF", (v3Global.opt.traceEnabledSaif()) ? "1" : "0");
        *of << "# VCD Tracing output mode?  0/1 (from --trace-vcd)\n";
        cmake_set_raw(*of, name + "_TRACE_VCD", (v3Global.opt.traceEnabledVcd()) ? "1" : "0");

        *of << "\n### Sources...\n";
        std::vector<string> classes_fast;
        std::vector<string> classes_slow;
        std::vector<string> support_fast;
        std::vector<string> support_slow;
        std::vector<string> global;
        for (AstNodeFile* nodep = v3Global.rootp()->filesp(); nodep;
             nodep = VN_AS(nodep->nextp(), NodeFile)) {
            const AstCFile* const cfilep = VN_CAST(nodep, CFile);
            if (cfilep && cfilep->source()) {
                if (cfilep->support()) {
                    if (cfilep->slow()) {
                        support_slow.push_back(cfilep->name());
                    } else {
                        support_fast.push_back(cfilep->name());
                    }
                } else {
                    if (cfilep->slow()) {
                        classes_slow.push_back(cfilep->name());
                    } else {
                        classes_fast.push_back(cfilep->name());
                    }
                }
            }
        }

        for (const string& cpp : v3Global.verilatedCppFiles())
            global.emplace_back("${VERILATOR_ROOT}/include/"s + cpp);

        if (!v3Global.opt.libCreate().empty()) {
            global.emplace_back(v3Global.opt.makeDir() + "/" + v3Global.opt.libCreate() + ".cpp");
        }

        *of << "# Global classes, need linked once per executable\n";
        cmake_set_raw(*of, name + "_GLOBAL", cmake_list(global));
        *of << "# Generated module classes, non-fast-path, compile with low/medium optimization\n";
        cmake_set_raw(*of, name + "_CLASSES_SLOW", cmake_list(classes_slow));
        *of << "# Generated module classes, fast-path, compile with highest optimization\n";
        cmake_set_raw(*of, name + "_CLASSES_FAST", cmake_list(classes_fast));
        *of << "# Generated support classes, non-fast-path, compile with "
               "low/medium optimization\n";
        cmake_set_raw(*of, name + "_SUPPORT_SLOW", cmake_list(support_slow));
        *of << "# Generated support classes, fast-path, compile with highest optimization\n";
        cmake_set_raw(*of, name + "_SUPPORT_FAST", cmake_list(support_fast));

        *of << "# All dependencies\n";
        cmake_set_raw(*of, name + "_DEPS", cmake_list(V3File::getAllDeps()));

        *of << "# User .cpp files (from .cpp's on Verilator command line)\n";
        cmake_set_raw(*of, name + "_USER_CLASSES", cmake_list(v3Global.opt.cppFiles()));
        if (const V3HierBlockPlan* const planp = v3Global.hierPlanp()) {
            *of << "# Verilate hierarchical blocks\n";
            // Sorted hierarchical blocks in order of leaf-first.
            const V3HierBlockPlan::HierVector& hierBlocks = planp->hierBlocksSorted();
            *of << "get_target_property(TOP_TARGET_NAME \"${TARGET}\" NAME)\n";
            for (V3HierBlockPlan::HierVector::const_iterator it = hierBlocks.begin();
                 it != hierBlocks.end(); ++it) {
                const V3HierBlock* hblockp = *it;
                const V3HierBlock::HierBlockSet& children = hblockp->children();
                const string prefix = hblockp->hierPrefix();
                *of << "add_library(" << prefix << " STATIC)\n";
                *of << "target_link_libraries(${TOP_TARGET_NAME}  PRIVATE " << prefix << ")\n";
                if (!children.empty()) {
                    *of << "target_link_libraries(" << prefix << " INTERFACE";
                    for (const auto& childr : children) *of << " " << (childr)->hierPrefix();
                    *of << ")\n";
                }
                *of << "verilate(" << prefix << " PREFIX " << prefix << " TOP_MODULE "
                    << hblockp->modp()->name() << " DIRECTORY "
                    << v3Global.opt.makeDir() + "/" + prefix << " SOURCES ";
                for (const auto& childr : children) {
                    *of << " " << v3Global.opt.makeDir() + "/" + childr->hierWrapperFilename(true);
                }
                *of << " ";
                const string vFile = hblockp->vFileIfNecessary();
                if (!vFile.empty()) *of << vFile << " ";
                for (const auto& i : v3Global.opt.vFiles())
                    *of << V3Os::filenameRealPath(i.filename()) << " ";
                *of << " VERILATOR_ARGS ";
                *of << "-f " << hblockp->commandArgsFilename(true)
                    << " -CFLAGS -fPIC"  // hierarchical block will be static, but may be linked
                                         // with .so
                    << ")\n";
            }
            *of << "\n# Verilate the top module that refers to lib-create wrappers of above\n";
            *of << "verilate(${TOP_TARGET_NAME} PREFIX " << v3Global.opt.prefix() << " TOP_MODULE "
                << v3Global.rootp()->topModulep()->name() << " DIRECTORY "
                << v3Global.opt.makeDir() << " SOURCES ";
            for (const auto& itr : *planp) {
                *of << " " << v3Global.opt.makeDir() + "/" + itr.second->hierWrapperFilename(true);
            }
            *of << " " << cmake_list(v3Global.opt.vFiles());
            *of << " VERILATOR_ARGS ";
            *of << "-f " << planp->topCommandArgsFilename(true);
            *of << ")\n";
        }
    }

public:
    explicit CMakeEmitter() { emitOverallCMake(); }
    virtual ~CMakeEmitter() = default;
};

void V3EmitCMake::emit() {
    UINFO(2, __FUNCTION__ << ":");
    const CMakeEmitter emitter;
}
