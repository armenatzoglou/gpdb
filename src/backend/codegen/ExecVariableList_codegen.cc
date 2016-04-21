//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_utils.cpp
//
//  @doc:
//    Contains different code generators
//
//---------------------------------------------------------------------------
#include <cstdint>
#include <string>

#include "codegen/ExecVariableList_codegen.h"
#include "codegen/utils/clang_compiler.h"
#include "codegen/utils/utility.h"
#include "codegen/utils/instance_method_wrappers.h"
#include "codegen/utils/codegen_utils.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Casting.h"
#include "postgres.h"
#include "access/htup.h"
#include "executor/tuptable.h"

extern "C" {
#include "utils/elog.h"
}

using gpcodegen::ExecVariableListCodegen;

constexpr char ExecVariableListCodegen::kExecVariableListPrefix[];

ExecVariableListCodegen::ExecVariableListCodegen
(
    ExecVariableListFn regular_func_ptr,
    ExecVariableListFn* ptr_to_regular_func_ptr,
    TupleTableSlot* slot) :
        BaseCodegen(
            kExecVariableListPrefix,
            regular_func_ptr,
            ptr_to_regular_func_ptr), slot_(slot) {
}

static void ElogWrapper(const char* message) {
  elog(INFO, "%s\n", message);
}

static void llvm_debug_elog(gpcodegen::CodegenUtils* codegen_utils,
		llvm::Function* llvm_elog_wrapper,
		const char* log_msg){
	llvm::Value* llvm_log_msg = codegen_utils->GetConstant(
		log_msg);
	codegen_utils->ir_builder()->CreateCall(llvm_elog_wrapper,
		  { llvm_log_msg });
}

bool ExecVariableListCodegen::GenerateExecVariableList(
    gpcodegen::CodegenUtils* codegen_utils) {

  llvm::Function* llvm_elog_wrapper = codegen_utils->RegisterExternalFunction(
		  ElogWrapper);
  assert(llvm_elog_wrapper != nullptr);

  auto irb = codegen_utils->ir_builder();

  COMPILE_ASSERT(sizeof(Datum) == sizeof(int64));

  llvm::Function* ExecVariableList_func = codegen_utils->
		  CreateFunction<ExecVariableListFn>(
				  GetUniqueFuncName());

  /* BasicBlock of function entry. */
  llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
      "entry", ExecVariableList_func);

  /*
   * Entry Block
   */
  irb->SetInsertPoint(entry_block);

  const char* fallback_log_msg = "Falling back to regular ExecVariableList.";
  llvm::Value* llvm_fallback_log_msg = codegen_utils->GetConstant(
      fallback_log_msg);
  codegen_utils->ir_builder()->CreateCall(llvm_elog_wrapper,
        { llvm_fallback_log_msg });

  auto regular_func_pointer = GetRegularFuncPointer();
  llvm::Function* llvm_regular_function =
		  codegen_utils->RegisterExternalFunction(regular_func_pointer);
  assert(llvm_regular_function != nullptr);

  std::vector<llvm::Value*> forwarded_args;

  for (llvm::Argument& arg : ExecVariableList_func->args()) {
	  forwarded_args.push_back(&arg);
  }

  llvm::CallInst* call_fallback_func = codegen_utils->ir_builder()->CreateCall(
        llvm_regular_function, forwarded_args);

  /* Return the result of the call, or void if the function returns void. */
  if (std::is_same<
		  gpcodegen::codegen_utils_detail::FunctionTypeUnpacker<
		  decltype(regular_func_pointer)>::R, void>::value) {
	  codegen_utils->ir_builder()->CreateRetVoid();
  }
  else
  {
	  codegen_utils->ir_builder()->CreateRet(call_fallback_func);
  }


  return true;
}


bool ExecVariableListCodegen::GenerateCodeInternal(CodegenUtils* codegen_utils) {
  bool isGenerated = GenerateExecVariableList(codegen_utils);

  if (isGenerated)
  {
    elog(INFO, "ExecVariableList was generated successfully!");
    return true;
  }

  return false;
}
