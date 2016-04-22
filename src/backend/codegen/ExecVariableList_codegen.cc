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
#include "nodes/execnodes.h"
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
    ProjectionInfo* proj_info,
	TupleTableSlot* slot) :
        BaseCodegen(
            kExecVariableListPrefix,
            regular_func_ptr,
            ptr_to_regular_func_ptr),
		proj_info_(proj_info),
		slot_(slot){
}

static void ElogWrapperString(const char* format, const char* message) {
  elog(INFO, format, message);
}
static void ElogWrapperInt(const char* format, const int integer) {
  elog(INFO, format, integer);
}

int GetMax(int* numbers, int length) {
	int i, max=-1;
	for(i=0; i < length; i++){
		if ( numbers[i] > max ) {
			max = numbers[i];
		}
	}
	return max;
}

static llvm::Function* llvm_elog_string_wrapper_ = nullptr;
static llvm::Function* llvm_elog_int_wrapper_ = nullptr;
static llvm::Function* llvm_fflush_wrapper_ = nullptr;

static void SetUpElog(gpcodegen::CodegenUtils* codegen_utils) {
	if (nullptr == llvm_elog_string_wrapper_) {
	  llvm_elog_string_wrapper_ = codegen_utils->RegisterExternalFunction(
				  ElogWrapperString);
	  assert(llvm_elog_string_wrapper_ != nullptr);
	}
	if (nullptr == llvm_elog_int_wrapper_) {
	  llvm_elog_int_wrapper_ = codegen_utils->RegisterExternalFunction(
				  ElogWrapperInt);
	  assert(llvm_elog_int_wrapper_ != nullptr);
	}
	if (nullptr == llvm_fflush_wrapper_) {
	  llvm_fflush_wrapper_ = codegen_utils->RegisterExternalFunction(
		  		  fflush);
	  assert(llvm_fflush_wrapper_ != nullptr);
	}
}

static void CreateElogInt(gpcodegen::CodegenUtils* codegen_utils,
		const char* format,
		llvm::Value* llvm_int_value){
	SetUpElog(codegen_utils);
	llvm::Value* llvm_format = codegen_utils->GetConstant(format);
	std::vector<llvm::Value*> llvm_args;
	llvm_args.push_back(llvm_format);
	llvm_args.push_back(llvm_int_value);
	codegen_utils->ir_builder()->CreateCall(llvm_elog_int_wrapper_, llvm_args);
	codegen_utils->ir_builder()->CreateCall(llvm_fflush_wrapper_, { codegen_utils->GetConstant((FILE *) NULL) });
}
static void CreateElogString(gpcodegen::CodegenUtils* codegen_utils,
		const char* format,
		llvm::Value* llvm_string_value){
	SetUpElog(codegen_utils);
	llvm::Value* llvm_format = codegen_utils->GetConstant(format);
	std::vector<llvm::Value*> llvm_args;
	llvm_args.push_back(llvm_format);
	llvm_args.push_back(llvm_string_value);
	codegen_utils->ir_builder()->CreateCall(llvm_elog_string_wrapper_, llvm_args);
	codegen_utils->ir_builder()->CreateCall(llvm_fflush_wrapper_, { codegen_utils->GetConstant((FILE *) NULL) });
}




static std::string slot_str;

bool ExecVariableListCodegen::GenerateExecVariableList(
    gpcodegen::CodegenUtils* codegen_utils) {



  COMPILE_ASSERT(sizeof(Datum) == sizeof(int64));

  /*
   * Only do codegen if all the elements in this target list are on the same tuple slot
   * This is an OK assumption for scan nodes, but might fail when joins are involved.
   */
  if ( NULL == proj_info_->pi_varSlotOffsets ) {
	// Being in this state is ridiculous - we fucked up!
    elog(INFO, "Cannot codegen ExecVariableList because varSlotOffsets are null");
    return false;
  }
  for (int i = list_length(proj_info_->pi_targetlist) - 1; i > 0; i--){
	  if (proj_info_->pi_varSlotOffsets[i] != proj_info_->pi_varSlotOffsets[i-1]){
		  elog(INFO, "Cannot codegen ExecVariableList because multiple slots to deform.");
		  return false;
	  }
  }

  // Find the max attribute in projInfo->pi_targetlist
  int max_attr = GetMax(proj_info_->pi_varNumbers, list_length(proj_info_->pi_targetlist));

  /* System attribute */
  if(max_attr <= 0)
  {
	  elog(INFO, "Cannot generate code for ExecVariableList because max_attr is negative (i.e., system attribute).");
	  return false;
  }

  /* So looks like we're going to generate code */

  llvm::Function* ExecVariableList_func = codegen_utils->
		  CreateFunction<ExecVariableListFn>(
				  GetUniqueFuncName());

  char *slotptr = ((char *) proj_info_->pi_exprContext) + proj_info_->pi_varSlotOffsets[0];
  TupleTableSlot *slot = *((TupleTableSlot **) slotptr);
  llvm::Value* llvm_slot = codegen_utils->GetConstant(slot);

  elog(INFO, "proj_info_ = %x, slot = %x, pi_slot = %x", proj_info_, slot, proj_info_->pi_slot);


  auto irb = codegen_utils->ir_builder();
  /* BasicBlock of function entry. */
  llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
      "entry", ExecVariableList_func);
//  /* BasicBlock for main. */
//    llvm::BasicBlock* main_block = codegen_utils->CreateBasicBlock(
//        "main", ExecVariableList_func);
  /* BasicBlock for fallback. */
  llvm::BasicBlock* fallback_block = codegen_utils->CreateBasicBlock(
      "fallback", ExecVariableList_func);

  // Method arguments
  llvm::Value* llvm_projInfo = ArgumentByPosition(ExecVariableList_func, 0);
  llvm::Value* llvm_values = ArgumentByPosition(ExecVariableList_func, 1);
  llvm::Value* llvm_is_null = ArgumentByPosition(ExecVariableList_func, 2);

  /*
   * Entry Block
   */
  irb->SetInsertPoint(entry_block);
  CreateElogString(codegen_utils,"%s\n", codegen_utils->GetConstant("OK in generated code!"));
  irb->CreateBr(fallback_block);


//  llvm::Value* llvm_econtext =
//  		  irb->CreateLoad(codegen_utils->GetPointerToMember(
//  				  llvm_projInfo, &ProjectionInfo::pi_exprContext));
//  llvm::Value* llvm_varSlotOffsets =
//  		  irb->CreateLoad(codegen_utils->GetPointerToMember(
//  				  llvm_projInfo, &ProjectionInfo::pi_varSlotOffsets));
//  llvm::Value* llvm_varNumbers =
//  		  irb->CreateLoad(codegen_utils->GetPointerToMember(
//  				  llvm_projInfo, &ProjectionInfo::pi_varNumbers));
//  CreateElogMessage(codegen_utils, llvm_elog_wrapper, llvm_fflush_wrapper, "P");
//  llvm::Value* llvm_slot_PRIVATE_tts_flags =
//    		  irb->CreateLoad(codegen_utils->GetPointerToMember(
//    				  llvm_slot, &TupleTableSlot::PRIVATE_tts_flags));
//  CreateElogMessage(codegen_utils, llvm_elog_wrapper, llvm_fflush_wrapper, "Q");
//  llvm::Value* llvm_slot_PRIVATE_tts_memtuple =
//      		  irb->CreateLoad(codegen_utils->GetPointerToMember(
//      				  llvm_slot, &TupleTableSlot::PRIVATE_tts_memtuple));

  /*
   * implementing slot_getattr
   */
//  CreateElogMessage(codegen_utils, llvm_elog_wrapper, llvm_fflush_wrapper, "R");
//
//  /* (slot->PRIVATE_tts_flags & TTS_VIRTUAL) != 0 */
//  llvm::Value* llvm_tuple_is_virtual = irb->CreateICmpNE(
//		  irb->CreateAnd(llvm_slot_PRIVATE_tts_flags, codegen_utils->GetConstant(TTS_VIRTUAL)),
//		  codegen_utils->GetConstant(0));
//  CreateElogMessage(codegen_utils, llvm_elog_wrapper, llvm_fflush_wrapper, "S");
//
//  /* slot->PRIVATE_tts_memtuple != NULL */
//  llvm::Value* llvm_tuple_has_memtuple = irb->CreateICmpNE(
//		  llvm_slot_PRIVATE_tts_memtuple, codegen_utils->GetConstant((MemTuple) NULL));
//
//  CreateElogMessage(codegen_utils, llvm_elog_wrapper, llvm_fflush_wrapper, "T");
//
//  /* Fallback if tuple has nulls*/
//   irb->CreateCondBr(
//		   irb->CreateOr(llvm_tuple_is_virtual, llvm_tuple_has_memtuple),
//		   fallback_block /*true*/, main_block /*false*/);

//  /*
//   * Main block
//   */
//  irb->SetInsertPoint(main_block);
//  // Go straight to the fallback block
//  CreateElogMessage(codegen_utils, llvm_elog_wrapper, llvm_fflush_wrapper, "In Main block.");
//  irb->CreateBr(fallback_block);


  /*
   * Fallback block
   */
  irb->SetInsertPoint(fallback_block);
  CreateElogString(codegen_utils, "%s\n", codegen_utils->GetConstant("Falling back to regular ExecVariableList"));

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
  else
  {
    elog(INFO, "ExecVariableList generation failed!");
    return false;
  }
}
