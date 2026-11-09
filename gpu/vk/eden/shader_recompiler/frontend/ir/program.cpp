// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <map>
#include <string>

#include <fmt/ranges.h>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

std::string DumpProgram(const Program& program) {
    size_t index{0};
    std::map<const IR::Inst*, size_t> inst_to_index;
    std::map<const IR::Block*, size_t> block_to_index;

    for (const IR::Block* const block : program.blocks) {
        block_to_index.emplace(block, index);
        ++index;
    }
    std::string ret;
    for (const auto& block : program.blocks) {
        ret += IR::DumpBlock(*block, block_to_index, inst_to_index, index) + '\n';
    }
    // (local test, 2026-11-08) the structured control flow: which block runs under which condition
    auto blk = [&](const IR::Block* b) {
        const auto it = block_to_index.find(b);
        return it != block_to_index.end() ? fmt::format("${}", it->second) : std::string("$?");
    };
    auto cond = [&](const IR::U1& c) {
        const IR::Value v{c};
        if (v.IsImmediate()) return fmt::format("#{}", v.U1());
        const IR::Inst* inst{v.InstRecursive()};
        const auto it = inst_to_index.find(inst);
        return fmt::format("[{:016x}] %{}", reinterpret_cast<u64>(inst),
                           it != inst_to_index.end() ? std::to_string(it->second) : std::string("?"));
    };
    ret += "Syntax\n";
    for (const auto& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::Block: ret += fmt::format("  block {}\n", blk(node.data.block)); break;
        case IR::AbstractSyntaxNode::Type::If:
            ret += fmt::format("  if {} then {} merge {}\n", cond(node.data.if_node.cond), blk(node.data.if_node.body),
                               blk(node.data.if_node.merge));
            break;
        case IR::AbstractSyntaxNode::Type::EndIf: ret += fmt::format("  endif merge {}\n", blk(node.data.end_if.merge)); break;
        case IR::AbstractSyntaxNode::Type::Loop:
            ret += fmt::format("  loop body {} continue {} merge {}\n", blk(node.data.loop.body), blk(node.data.loop.continue_block),
                               blk(node.data.loop.merge));
            break;
        case IR::AbstractSyntaxNode::Type::Repeat:
            ret += fmt::format("  repeat if {} to {} merge {}\n", cond(node.data.repeat.cond), blk(node.data.repeat.loop_header),
                               blk(node.data.repeat.merge));
            break;
        case IR::AbstractSyntaxNode::Type::Break:
            ret += fmt::format("  break if {} merge {} skip {}\n", cond(node.data.break_node.cond), blk(node.data.break_node.merge),
                               blk(node.data.break_node.skip));
            break;
        case IR::AbstractSyntaxNode::Type::Return: ret += "  return\n"; break;
        case IR::AbstractSyntaxNode::Type::Unreachable: ret += "  unreachable\n"; break;
        }
    }
    return ret;
}

} // namespace Shader::IR
