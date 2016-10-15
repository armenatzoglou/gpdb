//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    op_expr_tree_generator.h
//
//  @doc:
//    Object that generate code for operator expression.
//
//---------------------------------------------------------------------------

#include <assert.h>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "codegen/expr_tree_generator.h"
#include "codegen/op_expr_tree_generator.h"
#include "codegen/pg_func_generator.h"
#include "codegen/pg_func_generator_interface.h"
#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/pg_arith_func_generator.h"
#include "codegen/pg_date_func_generator.h"

#include "llvm/IR/IRBuilder.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "c.h"  // NOLINT(build/include)
#include "nodes/execnodes.h"
#include "utils/elog.h"
#include "nodes/nodes.h"
#include "nodes/pg_list.h"
#include "nodes/primnodes.h"
}

namespace llvm {
class Value;
}  // namespace llvm

using gpcodegen::OpExprTreeGenerator;
using gpcodegen::ExprTreeGenerator;
using gpcodegen::GpCodegenUtils;
using gpcodegen::PGFuncGeneratorInterface;
using gpcodegen::PGFuncGenerator;
using gpcodegen::CodeGenFuncMap;
using llvm::IRBuilder;


CodeGenFuncMap
OpExprTreeGenerator::supported_function_;

void OpExprTreeGenerator::InitializeSupportedFunction() {
  if (!supported_function_.empty()) { return; }

  supported_function_[141] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t, int32_t>(
          141,
          "int4mul",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::MulWithOverflow,
          true));

  supported_function_[149] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGIRBuilderFuncGenerator<bool, int32_t, int32_t>(
          149,
          "int4le",
          &IRBuilder<>::CreateICmpSLE,
          true));

  supported_function_[177] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t, int32_t>(
          177,
          "int4pl",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::AddWithOverflow,
          true));

  supported_function_[1841] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t, int32_t>(
          1841,
          "int4_sum",
          &PGArithFuncGenerator<int64_t, int64_t, int32_t>::AddWithOverflow,
          false));

  supported_function_[181] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t, int32_t>(
          181,
          "int4mi",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::SubWithOverflow,
          true));

  supported_function_[463] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t, int64_t>(
          463,
          "int8pl",
          &PGArithFuncGenerator<int64_t, int64_t, int64_t>::AddWithOverflow,
          true));

  supported_function_[1219] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t>(
          1219,
          "int8inc",
          &PGArithUnaryFuncGenerator<int64_t, int64_t>::IncWithOverflow,
          true));

  supported_function_[2803] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t>(
          2803,
          "int8inc",
          &PGArithUnaryFuncGenerator<int64_t, int64_t>::IncWithOverflow,
          false));

  supported_function_[216] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8, float8>(
          216,
          "float8mul",
          &PGArithFuncGenerator<float8, float8, float8>::MulWithOverflow,
          true));

  supported_function_[218] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8, float8>(
          218,
          "float8pl",
          &PGArithFuncGenerator<float8, float8, float8>::AddWithOverflow,
          true));

  supported_function_[219] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8, float8>(
          219,
          "float8mi",
          &PGArithFuncGenerator<float8, float8, float8>::SubWithOverflow,
          true));

  supported_function_[1088] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGIRBuilderFuncGenerator<bool, int32_t, int32_t>(
          1088, "date_le", &IRBuilder<>::CreateICmpSLE,
          true));

  supported_function_[2339] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<bool, int32_t, int64_t>(
          2339,
          "date_le_timestamp",
          &PGDateFuncGenerator::DateLETimestamp,
          true));
}

PGFuncGeneratorInterface* OpExprTreeGenerator::GetPGFuncGenerator(
      unsigned int oid) {
  InitializeSupportedFunction();
  auto itr = supported_function_.find(oid);
  if (itr == supported_function_.end()) {
    return nullptr;
  }
  return itr->second.get();
}

OpExprTreeGenerator::OpExprTreeGenerator(
    const ExprState* expr_state,
    std::vector<
        std::unique_ptr<ExprTreeGenerator>>&& arguments)  // NOLINT(build/c++11)
    :  ExprTreeGenerator(expr_state, ExprTreeNodeType::kOperator),
       arguments_(std::move(arguments)) {
}

bool OpExprTreeGenerator::VerifyAndCreateExprTree(
    const ExprState* expr_state,
    ExprTreeGeneratorInfo* gen_info,
    std::unique_ptr<ExprTreeGenerator>* expr_tree) {
  assert(nullptr != expr_state &&
         nullptr != expr_state->expr &&
         T_OpExpr == nodeTag(expr_state->expr) &&
         nullptr != expr_tree);

  OpExpr* op_expr = reinterpret_cast<OpExpr*>(expr_state->expr);
  expr_tree->reset(nullptr);
  PGFuncGeneratorInterface* pg_func_gen = GetPGFuncGenerator(op_expr->opfuncid);
  if (nullptr == pg_func_gen) {
    elog(DEBUG1, "Unsupported operator %d.", op_expr->opfuncid);
    return false;
  }

  List *arguments = reinterpret_cast<const FuncExprState*>(expr_state)->args;
  assert(nullptr != arguments);
  // In ExecEvalFuncArgs
  assert(list_length(arguments) ==
        static_cast<int>(pg_func_gen->GetTotalArgCount()));

  ListCell   *arg = nullptr;
  bool supported_tree = true;
  std::vector<std::unique_ptr<ExprTreeGenerator>> expr_tree_arguments;
  foreach(arg, arguments) {
    // retrieve argument's ExprState
    ExprState  *argstate = reinterpret_cast<ExprState*>(lfirst(arg));
    assert(nullptr != argstate);
    std::unique_ptr<ExprTreeGenerator> arg(nullptr);
    supported_tree &= ExprTreeGenerator::VerifyAndCreateExprTree(argstate,
                                                                 gen_info,
                                                                 &arg);
    if (!supported_tree) {
      break;
    }
    assert(nullptr != arg);
    expr_tree_arguments.push_back(std::move(arg));
  }
  if (!supported_tree) {
    return supported_tree;
  }
  expr_tree->reset(new OpExprTreeGenerator(expr_state,
                                           std::move(expr_tree_arguments)));
  return true;
}

bool OpExprTreeGenerator::GenerateCode(GpCodegenUtils* codegen_utils,
                                       const ExprTreeGeneratorInfo& gen_info,
                                       llvm::Value* llvm_isnull_ptr,
                                       llvm::Value** llvm_out_value) {
  assert(nullptr != llvm_out_value);
  *llvm_out_value = nullptr;
  auto irb = codegen_utils->ir_builder();
  OpExpr* op_expr = reinterpret_cast<OpExpr*>(expr_state()->expr);
  int op_id = op_expr->opfuncid;
  CodeGenFuncMap::iterator itr =  supported_function_.find(op_id);

  if (itr == supported_function_.end()) {
    // Operators are stored in pg_proc table.
    // See postgres.bki for more details.
    elog(WARNING, "Unsupported operator %d.", op_id);
    return false;
  }

  // Get the interface to generate code for operator function
  PGFuncGeneratorInterface* pg_func_interface = itr->second.get();
  assert(nullptr != pg_func_interface);

  llvm::Value* llvm_op_value = nullptr;

  // Blocks that will be used if function is strict for examining NULL cases.
  // Block for generating the code of the function.
  llvm::BasicBlock* generate_function_block = nullptr;
  // Block that returns (Datum) 0 if there is NULL argument.
  llvm::BasicBlock* null_argument_block = nullptr;
  // Block that sets the final value of the result.
  llvm::BasicBlock* set_llvm_out_value_block = nullptr;
  // Block that continues the implementation of the caller.
  llvm::BasicBlock* continue_to_parent_node_block = nullptr;
  // Block that includes code which implements the current argument during
  // the iteration over all arguments.
  llvm::BasicBlock* current_arrgument_block = nullptr;
  // Block that will include the code for the next argument.
  llvm::BasicBlock* next_argument_block = nullptr;

  // Create the blocks only if function is strict
  if (pg_func_interface->IsStrict() && arguments_.size() > 0) {
    generate_function_block = codegen_utils->CreateBasicBlock(
        "generate_function_block_opid_"+ std::to_string(op_id),
        gen_info.llvm_main_func);
    null_argument_block = codegen_utils->CreateBasicBlock(
        "null_argument_block_opid_"+ std::to_string(op_id),
        gen_info.llvm_main_func);
    set_llvm_out_value_block = codegen_utils->CreateBasicBlock(
        "set_llvm_out_value_block_opid_"+ std::to_string(op_id),
        gen_info.llvm_main_func);
    continue_to_parent_node_block = codegen_utils->CreateBasicBlock(
            "continue_to_parent_node_block_opid_"+ std::to_string(op_id),
            gen_info.llvm_main_func);

    current_arrgument_block = codegen_utils->CreateBasicBlock(
        "argument_0_block_opid_"+ std::to_string(op_id),
        gen_info.llvm_main_func);

    irb->CreateBr(current_arrgument_block);
  }

  if (arguments_.size() != pg_func_interface->GetTotalArgCount()) {
    elog(WARNING, "Expected argument size to be %lu\n",
         pg_func_interface->GetTotalArgCount());
    return false;
  }
  bool arg_generated = true;
  std::vector<llvm::Value*> llvm_arguments;
  int argnum = 0;
  for (auto& arg : arguments_) {
    if (pg_func_interface->IsStrict()) {
      // -------- Current Argument Block ------- //
      irb->SetInsertPoint(current_arrgument_block);

      next_argument_block = codegen_utils->CreateBasicBlock(
                "argument_" + std::to_string(argnum+1) + "_block_opid_"+ std::to_string(op_id),
                gen_info.llvm_main_func);
    }

    llvm::Value* llvm_arg_isNull_ptr = irb->CreateAlloca(
            codegen_utils->GetType<bool>(), nullptr, "isNull");
     irb->CreateStore(codegen_utils->GetConstant<bool>(false), llvm_arg_isNull_ptr);

    llvm::Value* llvm_arg = nullptr;
    arg_generated &= arg->GenerateCode(codegen_utils,
                                       gen_info,
                                       llvm_arg_isNull_ptr,
                                       &llvm_arg);

    if (!arg_generated) {
      return false;
    }
    llvm_arguments.push_back(llvm_arg);
    argnum++;

    if (pg_func_interface->IsStrict()) {
      irb->CreateCondBr(
          irb->CreateLoad(llvm_arg_isNull_ptr),
          null_argument_block, /* true */
          next_argument_block /* false */);

      current_arrgument_block = next_argument_block;
    }

  }

  if (pg_func_interface->IsStrict()) {
    irb->SetInsertPoint(next_argument_block);
    irb->CreateBr(generate_function_block);

    irb->SetInsertPoint(generate_function_block);
  }

  PGFuncGeneratorInfo pg_func_info(gen_info.llvm_main_func,
                                   gen_info.llvm_error_block,
                                   llvm_arguments,
                                   llvm_isnull_ptr);
  bool retval = pg_func_interface->GenerateCode(codegen_utils,
                                                pg_func_info,
                                                &llvm_op_value);

  // convert return type to Datum
  llvm::Value* llvm_out_value_tmp = codegen_utils->CreateCppTypeToDatumCast(llvm_op_value);

  if (pg_func_interface->IsStrict()) {

    llvm::BasicBlock* last_block = irb->GetInsertBlock();

    irb->CreateBr(set_llvm_out_value_block);

    irb->SetInsertPoint(null_argument_block);

    //*isNull = true;
    irb->CreateStore(
        codegen_utils->GetConstant<bool>(true),
        llvm_isnull_ptr);

    //return (Datum) 0;
    *llvm_out_value = codegen_utils->GetConstant<Datum>(0);

    irb->CreateBr(set_llvm_out_value_block);

    irb->SetInsertPoint(set_llvm_out_value_block);

    llvm::PHINode* llvm_out_value_phinode = irb->CreatePHI(
        codegen_utils->GetType<Datum>(), 2);
    llvm_out_value_phinode->addIncoming(codegen_utils->GetConstant<Datum>(0), null_argument_block);
    llvm_out_value_phinode->addIncoming(llvm_out_value_tmp, last_block);

    *llvm_out_value = llvm_out_value_phinode;

    irb->CreateBr(continue_to_parent_node_block);

    irb->SetInsertPoint(continue_to_parent_node_block);

  }

  return retval;
}
