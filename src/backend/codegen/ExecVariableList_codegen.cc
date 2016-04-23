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

extern "C" {
#include "postgres.h"
#include "utils/elog.h"
#include "access/htup.h"
#include "nodes/execnodes.h"
#include "executor/tuptable.h"
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
static llvm::Function* llvm_memset_wrapper_ = nullptr;

static void SetUpExternalFunctions(gpcodegen::CodegenUtils* codegen_utils) {
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
	if (nullptr == llvm_memset_wrapper_) {
	  llvm_memset_wrapper_ =
				codegen_utils->RegisterExternalFunction(memset);
	  assert(llvm_memset_wrapper_ != nullptr);
	}
}

static void TearDownExternalFunctions(gpcodegen::CodegenUtils* codegen_utils){
	llvm_fflush_wrapper_ = nullptr;
	llvm_elog_int_wrapper_ = nullptr;
	llvm_elog_string_wrapper_ = nullptr;
	llvm_memset_wrapper_ = nullptr;
}

static void CreateElogInt(gpcodegen::CodegenUtils* codegen_utils,
		const char* format,
		llvm::Value* llvm_int_value){
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
	llvm::Value* llvm_format = codegen_utils->GetConstant(format);
	std::vector<llvm::Value*> llvm_args;
	llvm_args.push_back(llvm_format);
	llvm_args.push_back(llvm_string_value);
	codegen_utils->ir_builder()->CreateCall(llvm_elog_string_wrapper_, llvm_args);
	codegen_utils->ir_builder()->CreateCall(llvm_fflush_wrapper_, { codegen_utils->GetConstant((FILE *) NULL) });
}

/* void *memset(void *s, int c, size_t n); */
static void CreateMemset(gpcodegen::CodegenUtils* codegen_utils,
		llvm::Value* llvm_ptr,
		llvm::Value* llvm_fill_val,
		llvm::Value* llvm_fill_size) {

  // TODO: Some asserts on the types of the input
  llvm::CallInst* call_llvm_memset = codegen_utils->ir_builder()->CreateCall(
      llvm_memset_wrapper_, {llvm_ptr, llvm_fill_val, llvm_fill_size});
}


bool ExecVariableListCodegen::GenerateExecVariableList(
    gpcodegen::CodegenUtils* codegen_utils) {

  SetUpExternalFunctions(codegen_utils);
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
  }else if (max_attr > slot_->tts_tupleDescriptor->natts) {
	  elog(INFO, "Cannot generate code for ExecVariableList because max_attr is greater than natts.");
	  return false;
  }

  /* So looks like we're going to generate code */
  llvm::Function* ExecVariableList_func = codegen_utils->
		  CreateFunction<ExecVariableListFn>(
				  GetUniqueFuncName());

  //elog(INFO, "proj_info_ = %x, slot = %x, pi_slot = %x", proj_info_, slot, proj_info_->pi_slot);

  auto irb = codegen_utils->ir_builder();

  /* BasicBlock of function entry. */
  llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
      "entry", ExecVariableList_func);
  /* BasicBlock for checking tuple type. */
  llvm::BasicBlock* tuple_type_check_block = codegen_utils->CreateBasicBlock(
        "tuple_type_check", ExecVariableList_func);
  /* BasicBlock for heap tuple check. */
  llvm::BasicBlock* heap_tuple_check_block = codegen_utils->CreateBasicBlock(
      "heap_tuple_check", ExecVariableList_func);
  /* BasicBlock for main. */
  llvm::BasicBlock* main_block = codegen_utils->CreateBasicBlock(
        "main", ExecVariableList_func);
  /* BasicBlock for fallback. */
  llvm::BasicBlock* fallback_block = codegen_utils->CreateBasicBlock(
      "fallback", ExecVariableList_func);

  // External functions
  llvm::Function* llvm__slot_getsomeattrs = codegen_utils->RegisterExternalFunction(_slot_getsomeattrs);
  llvm::Function* llvm_slot_deform_tuple = codegen_utils->RegisterExternalFunction(slot_deform_tuple);

  // Constants
  llvm::Value* llvm_max_attr = codegen_utils->GetConstant(max_attr);
  llvm::Value* llvm_slot = codegen_utils->GetConstant(slot_);

  // Method arguments
  llvm::Value* llvm_projInfo_arg = ArgumentByPosition(ExecVariableList_func, 0);
  llvm::Value* llvm_values_arg = ArgumentByPosition(ExecVariableList_func, 1);
  llvm::Value* llvm_isnull_arg = ArgumentByPosition(ExecVariableList_func, 2);

  /*
   * Entry Block
   */
  irb->SetInsertPoint(entry_block);
  llvm::Value* llvm_econtext =
  		  irb->CreateLoad(codegen_utils->GetPointerToMember(
  				  llvm_projInfo_arg, &ProjectionInfo::pi_exprContext));
  llvm::Value* llvm_varSlotOffsets =
  		  irb->CreateLoad(codegen_utils->GetPointerToMember(
  				  llvm_projInfo_arg, &ProjectionInfo::pi_varSlotOffsets));

  // We want to fall back when ExecVariableList is called with a slot that's different from the
  // one we generated the function (eg HashJoin). We also assume only 1 slot and that the slot is
  // in a scan node ie from exprContext->ecxt_scantuple or varOffset = 0
  llvm::Value* llvm_slot_arg =
		  irb->CreateLoad(codegen_utils->GetPointerToMember(
				  llvm_econtext, &ExprContext::ecxt_scantuple));

  //CreateElogString(codegen_utils, "%s\n", codegen_utils->GetConstant("OK in generated code!"));
  CreateElogInt(codegen_utils, "slot = %x ", irb->CreatePtrToInt(llvm_slot, codegen_utils->GetType<int>()));
  CreateElogInt(codegen_utils, "input_slot = %x \n", irb->CreatePtrToInt(llvm_slot_arg, codegen_utils->GetType<int>()));

  irb->CreateCondBr(
		  irb->CreateICmpEQ(llvm_slot, llvm_slot_arg),
		  tuple_type_check_block /* true */,
		  fallback_block /* false */
		  );

  /*
   * Tuple Type check
   * we fall back if we see a virtual tuple or mem tuple, but it's possible to partially handle those cases also
   */
  irb->SetInsertPoint(tuple_type_check_block);
  llvm::Value* llvm_slot_PRIVATE_tts_flags_ptr =
    		  codegen_utils->GetPointerToMember(
    				  llvm_slot, &TupleTableSlot::PRIVATE_tts_flags);
  llvm::Value* llvm_slot_PRIVATE_tts_memtuple =
      		  irb->CreateLoad(codegen_utils->GetPointerToMember(
      				  llvm_slot, &TupleTableSlot::PRIVATE_tts_memtuple));

  /* (slot->PRIVATE_tts_flags & TTS_VIRTUAL) != 0 */
  llvm::Value* llvm_tuple_is_virtual = irb->CreateICmpNE(
		  irb->CreateAnd(
				  irb->CreateLoad(llvm_slot_PRIVATE_tts_flags_ptr),
				  codegen_utils->GetConstant(TTS_VIRTUAL)),
		  codegen_utils->GetConstant(0));

  /* slot->PRIVATE_tts_memtuple != NULL */
  llvm::Value* llvm_tuple_has_memtuple = irb->CreateICmpNE(
		  llvm_slot_PRIVATE_tts_memtuple, codegen_utils->GetConstant((MemTuple) NULL));

  /* Fallback if tuple is virtual or memtuple is null */
  irb->CreateCondBr(
	   irb->CreateOr(llvm_tuple_is_virtual, llvm_tuple_has_memtuple),
	   fallback_block /*true*/, heap_tuple_check_block /*false*/);

  /*
   * HeapTuple check
   * _slot_getsomeattrs: TupGetHeapTuple(slot) != NULL
   */
  irb->SetInsertPoint(heap_tuple_check_block);
  llvm::Value* llvm_slot_PRIVATE_tts_heaptuple =
        		  irb->CreateLoad(codegen_utils->GetPointerToMember(
        				  llvm_slot, &TupleTableSlot::PRIVATE_tts_heaptuple));
  llvm::Value* llvm_tuple_has_heaptuple = irb->CreateICmpNE(
	   llvm_slot_PRIVATE_tts_heaptuple, codegen_utils->GetConstant((HeapTuple) NULL));
  irb->CreateCondBr(llvm_tuple_has_heaptuple, main_block /*true*/, fallback_block /*false*/);


  /*
   * Main block
   */
  irb->SetInsertPoint(main_block);
  CreateElogString(codegen_utils, "%s\n", codegen_utils->GetConstant("In Main block."));

  /*
   * attno = HeapTupleHeaderGetNatts(tuple->t_data);
   * attno = Min(attno, attnum);
   */
  llvm::Value* llvm_heaptuple_t_data =
		  irb->CreateLoad(codegen_utils->GetPointerToMember(
				  llvm_slot_PRIVATE_tts_heaptuple,
				  &HeapTupleData::t_data
				  ));
  llvm::Value* llvm_heaptuple_t_data_t_infomask2 =
		  irb->CreateLoad(codegen_utils->GetPointerToMember(
				  llvm_heaptuple_t_data,
				  &HeapTupleHeaderData::t_infomask2));
  llvm::Value* llvm_attno_t0 =
		  irb->CreateZExt(
				  irb->CreateAnd(llvm_heaptuple_t_data_t_infomask2,
					  codegen_utils->GetConstant<int16>(HEAP_NATTS_MASK)),
				  codegen_utils->GetType<int>());

  llvm::Value* llvm_attno =
		  irb->CreateSelect(
				  irb->CreateICmpSLT(llvm_attno_t0, llvm_max_attr),
				  llvm_attno_t0,
				  llvm_max_attr
				  );

  irb->CreateCall(llvm_slot_deform_tuple, { llvm_slot, llvm_attno });

  // slot's PRIVATE variables
  llvm::Value* llvm_slot_PRIVATE_tts_isnull /* bool* */=
    		  irb->CreateLoad(codegen_utils->GetPointerToMember(
    				  llvm_slot, &TupleTableSlot::PRIVATE_tts_isnull));
  llvm::Value* llvm_slot_PRIVATE_tts_values /* Datum* */=
      		  irb->CreateLoad(codegen_utils->GetPointerToMember(
      				  llvm_slot, &TupleTableSlot::PRIVATE_tts_values));
  llvm::Value* llvm_slot_PRIVATE_tts_nvalid_ptr /* int* */=
        		  codegen_utils->GetPointerToMember(llvm_slot, &TupleTableSlot::PRIVATE_tts_nvalid);

  /* The loop unrolling magic */
  int  *varNumbers = proj_info_->pi_varNumbers;
  for (int i = list_length(proj_info_->pi_targetlist) - 1; i >= 0; i--)
  {
//  	values[i] = slot_getattr(varSlot, varNumbers[i] -1 +1, &(isnull[i]));
  	//*isnull = slot->PRIVATE_tts_isnull[attnum-1];
//  	*(&(isnull[i])) = slot->PRIVATE_tts_isnull[ varNumbers[i] - 1];
//  	values[i] = varSlot->PRIVATE_tts_values[varNumbers[i] -1];

	  llvm::Value* llvm_isnull_from_slot_val =
			  irb->CreateLoad(irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_isnull,
					  {codegen_utils->GetConstant(varNumbers[i] - 1)}));
	  llvm::Value* llvm_isnull_ptr =
			  irb->CreateInBoundsGEP(llvm_isnull_arg, {codegen_utils->GetConstant(i)});
	  irb->CreateStore(llvm_isnull_from_slot_val, llvm_isnull_ptr);

	  llvm::Value* llvm_value_from_slot_val =
			  irb->CreateLoad(irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_values,
					  {codegen_utils->GetConstant(varNumbers[i] - 1)}));
	  llvm::Value* llvm_values_ptr =
			  irb->CreateInBoundsGEP(llvm_values_arg, {codegen_utils->GetConstant(i)});
	  irb->CreateStore(llvm_value_from_slot_val, llvm_values_ptr);
	  CreateElogInt(codegen_utils, "isnull = %d", irb->CreateZExt(llvm_isnull_from_slot_val, codegen_utils->GetType<int>()));
  }


  // TODO: we can avoid this
  CreateMemset(
		  codegen_utils,
		  irb->CreateBitCast(
				  irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_values, {llvm_attno}),
				  codegen_utils->GetType<void*>()),
		  codegen_utils->GetConstant(0),
		  irb->CreateMul(codegen_utils->GetConstant(sizeof(Datum)),
				  irb->CreateZExtOrTrunc(irb->CreateSub(llvm_max_attr, llvm_attno), codegen_utils->GetType<size_t>()))
		  );

  CreateMemset(
  		  codegen_utils,
  		  irb->CreateBitCast(
  				  irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_isnull, {llvm_attno}),
  				  codegen_utils->GetType<void*>()),
  		  codegen_utils->GetConstant((int) true),
		  irb->CreateMul(codegen_utils->GetConstant(sizeof(Datum)),
				  irb->CreateZExtOrTrunc(irb->CreateSub(llvm_max_attr, llvm_attno), codegen_utils->GetType<size_t>()))
  		  );

  /* slot->PRIVATE_tts_nvalid = attnum; */
  irb->CreateStore(llvm_max_attr, llvm_slot_PRIVATE_tts_nvalid_ptr);

  /* TupSetVirtualTuple(slot); */
  irb->CreateStore(
		  irb->CreateOr(
				  irb->CreateLoad(llvm_slot_PRIVATE_tts_flags_ptr),
				  codegen_utils->GetConstant<int>(TTS_VIRTUAL)),
		  llvm_slot_PRIVATE_tts_flags_ptr);

  codegen_utils->ir_builder()->CreateRetVoid();

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


  TearDownExternalFunctions(codegen_utils);
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
