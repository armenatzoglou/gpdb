//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    advance_aggregates_codegen.cc
//
//  @doc:
//    Generates code for AdvanceAggregates function.
//
//---------------------------------------------------------------------------
#include <assert.h>
#include <stddef.h>
#include <cstdint>
#include <memory>
#include <string>

#include "codegen/advance_aggregates_codegen.h"
#include "codegen/base_codegen.h"
#include "codegen/codegen_wrapper.h"
#include "codegen/op_expr_tree_generator.h"
#include "codegen/pg_func_generator_interface.h"


#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/utils/utility.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "nodes/execnodes.h"
#include "executor/nodeAgg.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "executor/tuptable.h"
#include "executor/executor.h"
#include "nodes/nodes.h"
}

namespace llvm {
class BasicBlock;
class Function;
class Value;
}  // namespace llvm

using gpcodegen::AdvanceAggregatesCodegen;

constexpr char AdvanceAggregatesCodegen::kAdvanceAggregatesPrefix[];

AdvanceAggregatesCodegen::AdvanceAggregatesCodegen(
    CodegenManager* manager,
    AdvanceAggregatesFn regular_func_ptr,
    AdvanceAggregatesFn* ptr_to_regular_func_ptr,
    AggState *aggstate)
: BaseCodegen(manager,
              kAdvanceAggregatesPrefix,
              regular_func_ptr,
              ptr_to_regular_func_ptr),
              aggstate_(aggstate) {
}

bool AdvanceAggregatesCodegen::GenerateAdvanceAggregates(
    gpcodegen::GpCodegenUtils* codegen_utils) {

  assert(NULL != codegen_utils);
  if (nullptr == aggstate_) {
    return false;
  }

  auto irb = codegen_utils->ir_builder();

  llvm::Function* advance_aggregates_func = CreateFunction<AdvanceAggregatesFn>(
      codegen_utils, GetUniqueFuncName());

  // BasicBlock of function entry.
  llvm::BasicBlock* llvm_entry_block = codegen_utils->CreateBasicBlock(
      "entry block", advance_aggregates_func);
  llvm::BasicBlock* llvm_error_block = codegen_utils->CreateBasicBlock(
      "error block", advance_aggregates_func);
  llvm::BasicBlock* llvm_advance_transition_function_block = codegen_utils->CreateBasicBlock(
      "advance_transition_function block", advance_aggregates_func);


  // External functions
  llvm::Function* llvm_ExecVariableList =
      codegen_utils->GetOrRegisterExternalFunction(ExecVariableList,
                                                   "ExecVariableList");
  llvm::Function* llvm_ExecTargetList =
      codegen_utils->GetOrRegisterExternalFunction(ExecTargetList,
                                                   "ExecTargetList");
  llvm::Function* llvm_MemoryContextSwitchTo =
      codegen_utils->GetOrRegisterExternalFunction(MemoryContextSwitchTo,
                                                   "MemoryContextSwitchTo");

  // Function argument to advance_aggregates
  llvm::Value* llvm_pergroup = ArgumentByPosition(advance_aggregates_func, 1);

  // Generation-time constants
  llvm::Value *llvm_tuplecontext = codegen_utils->GetConstant<MemoryContext>(
      aggstate_->tmpcontext->ecxt_per_tuple_memory);

  irb->SetInsertPoint(llvm_entry_block);

  codegen_utils->CreateElog(INFO, "Codegen'ed expression evaluation called!");

  // Temporary variables that replace the use of slot and FunctionInfoData
  llvm::Value *llvm_arg = irb->CreateAlloca(
      codegen_utils->GetType<Datum>(), 0, "llvm_arg");
  llvm::Value *llvm_argnull = irb->CreateAlloca(
      codegen_utils->GetType<bool>(), 0, "llvm_argnull");

  for (int aggno = 0; aggno < aggstate_->numaggs; aggno++)
  {
    AggStatePerAgg peraggstate = &aggstate_->peragg[aggno];

    if (peraggstate->numSortCols > 0) {
      elog(INFO, "We don't codegen DISTINCT and/or ORDER by case");
      return false;
    }

    Aggref *aggref = peraggstate->aggref;
    if (!aggref)
    {
      elog (INFO, "We don't codegen non-aggref functions");
      return false;
    }

    int nargs = list_length(aggref->args);
    Assert(nargs == peraggstate->numArguments);
    Assert(peraggstate->evalproj);

    if (peraggstate->evalproj->pi_isVarList)
    {
      // TODO(nikos): call the one with the pointer from previous codegen
      irb->CreateCall(llvm_ExecVariableList, {
          codegen_utils->GetConstant<ProjectionInfo *>(peraggstate->evalproj),
          llvm_arg,
          llvm_argnull
      });

      codegen_utils->CreateElog(INFO, "Variable = %d",
                                irb->CreateLoad(llvm_arg));
    }
    else
    {
      irb->CreateCall(llvm_ExecTargetList, {
          codegen_utils->GetConstant(peraggstate->evalproj->pi_targetlist),
          codegen_utils->GetConstant(peraggstate->evalproj->pi_exprContext),
          llvm_arg,
          llvm_argnull,
          codegen_utils->GetConstant(peraggstate->evalproj->pi_itemIsDone),
          codegen_utils->GetConstant<ExprDoneCond *>(nullptr)
      });

      codegen_utils->CreateElog(INFO, "Variable = %d", llvm_arg);
    }

    // Fallback if attribute is NULL.
    // TODO(nikos): Support null attributes.
    irb->CreateCondBr(irb->CreateLoad(llvm_argnull),
                      llvm_error_block /*true*/,
                      llvm_advance_transition_function_block /*false*/);

    // advance_transition_function block
    // ----------
    // We generate code for advance_transition_function.
    irb->SetInsertPoint(llvm_advance_transition_function_block);

    //    advance_transition_function(aggstate, peraggstate, pergroupstate,
    //                        &fcinfo, mem_manager); {{{

    //
    // TODO(nikos): Currently we do not support NULL attributes. However, if we
    // support NULL attributes and transition function is
    // strict (transfn->fn_strict), then we have to initiate transValue,
    // noTransvalue, transValueIsNull accordingly (see invoke_agg_trans_func).

    //oldContext = MemoryContextSwitchTo(tuplecontext);
    llvm::Value *llvm_oldContext = irb->CreateCall(llvm_MemoryContextSwitchTo,
                                                   {llvm_tuplecontext});

    // Retrieve pergroup's useful members
    llvm::Value* llvm_pergroupstate = irb->CreateGEP(
        llvm_pergroup, {codegen_utils->GetConstant(aggno)});
    llvm::Value* llvm_pergroupstate_transValue =
        codegen_utils->GetPointerToMember(
            llvm_pergroupstate, &AggStatePerGroupData::transValue);
    llvm::Value* llvm_pergroupstate_transValueIsNull =
        codegen_utils->GetPointerToMember(
            llvm_pergroupstate, &AggStatePerGroupData::transValueIsNull);
    llvm::Value* llvm_pergroupstate_noTransValue =
        codegen_utils->GetPointerToMember(
            llvm_pergroupstate, &AggStatePerGroupData::noTransValue);

    //newVal = FunctionCallInvoke(fcinfo); {{
    PGFuncGeneratorInfo pg_func_info(
        advance_aggregates_func,
        llvm_error_block,
        {irb->CreateLoad(llvm_pergroupstate_transValue),
            irb->CreateLoad(llvm_arg)}
    );

    PGFuncGeneratorInterface* pg_func_gen = OpExprTreeGenerator::
        GetPGFuncGenerator(peraggstate->transfn.fn_oid);
    if (nullptr == pg_func_gen)
    {
      elog(INFO, "We do not support function with oid = %d",
           peraggstate->transfn.fn_oid);
      return false;
    }

    llvm::Value *newVal = nullptr;
    pg_func_gen->GenerateCode(codegen_utils, pg_func_info, &newVal);
    llvm::Value *result = codegen_utils->CreateCppTypeToDatumCast(newVal);
    // }} FunctionCallInvoke

    // MemoryContextSwitchTo(oldContext);
    irb->CreateCall(llvm_MemoryContextSwitchTo, {llvm_oldContext});

    // }}} advance_transition_function


    if (!peraggstate->transtypeByVal) {
      elog(DEBUG1, "We do not support pass-by-ref datatypes.");
      return false;
    }

    // pergroupstate->transValue = newval {{{
    irb->CreateStore(result, llvm_pergroupstate_transValue);
    // }}}


    // Currently we do not support null attributes.
    // Thus we set transValueIsNull and noTransValue to false by default.
    // TODO(nikos): Support null attributes.
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_pergroupstate_transValueIsNull);
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_pergroupstate_noTransValue);

    codegen_utils->CreateElog(INFO, "transValue = %d",
                              irb->CreateLoad(llvm_pergroupstate_transValue));

  } // End of for loop

  irb->CreateRetVoid();

  irb->SetInsertPoint(llvm_error_block);

  codegen_utils->CreateElog(INFO, "An error has occurred and we are falling back");

  codegen_utils->CreateFallback<AdvanceAggregatesFn>(
      codegen_utils->GetOrRegisterExternalFunction(advance_aggregates,
                                                   "advance_aggregates"),
                                                   advance_aggregates_func);

  return true;
}


bool AdvanceAggregatesCodegen::GenerateCodeInternal(GpCodegenUtils* codegen_utils) {
  bool isGenerated = GenerateAdvanceAggregates(codegen_utils);

  if (isGenerated) {
    elog(DEBUG1, "AdvanceAggregates was generated successfully!");
    return true;
  } else {
    elog(DEBUG1, "AdvanceAggregates generation failed!");
    return false;
  }
}
