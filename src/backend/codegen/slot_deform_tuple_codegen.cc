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

#include "codegen/slot_deform_tuple_codegen.h"
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

using gpcodegen::SlotDeformTupleCodegen;

constexpr char SlotDeformTupleCodegen::kSlotDeformTupleNamePrefix[];

SlotDeformTupleCodegen::SlotDeformTupleCodegen(
    SlotDeformTupleFn regular_func_ptr,
    SlotDeformTupleFn* ptr_to_regular_func_ptr,
    TupleTableSlot* slot) :
        BaseCodegen(
            kSlotDeformTupleNamePrefix,
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

bool SlotDeformTupleCodegen::GenerateSlotDeformTuple(
    gpcodegen::CodegenUtils* codegen_utils) {

  llvm::Function* llvm_elog_wrapper = codegen_utils->RegisterExternalFunction(
		  ElogWrapper);
  assert(llvm_elog_wrapper != nullptr);

  TupleDesc tupleDesc = slot_->tts_tupleDescriptor;
  int tuple_desc_natts = tupleDesc->natts;

  auto irb = codegen_utils->ir_builder();

  COMPILE_ASSERT(sizeof(Datum) == sizeof(int64));

  llvm::Function* slot_deform_tuple_func = codegen_utils->
		  CreateFunction<SlotDeformTupleFn>(
				  GetUniqueFuncName());

  /* BasicBlock of function entry. */
  llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
      "entry", slot_deform_tuple_func);
  /* BasicBlock of falling back to regular slot_deform_tuple */
  llvm::BasicBlock* fallback_case = codegen_utils->CreateBasicBlock(
      "fallback_case", slot_deform_tuple_func);
  /* BasicBlock of slot_deform_tuple code generation */
  llvm::BasicBlock* gen_code_case = codegen_utils->CreateBasicBlock(
      "gen_code_case", slot_deform_tuple_func);

  /*
   * Entry Block
   */
  irb->SetInsertPoint(entry_block);

  llvm::Value* func_name_llvm = codegen_utils->GetConstant(
		  GetOrigFuncName().c_str());

  llvm_debug_elog(codegen_utils, llvm_elog_wrapper, "Executing jit slot_deform_tuple");

  llvm::Value* llvm_slot = codegen_utils->GetConstant(slot_);
  /* Extract heap tuple from slot */
  llvm::Value* llvm_heap_tup =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_heaptuple));

	/* Extract t_data field from heap tuple and store it into tup. */
  llvm::Value* llvm_t_data = irb->CreateLoad(
      codegen_utils->GetPointerToMember(llvm_heap_tup,
                                        &HeapTupleData::t_data));
  /* Extract t_hoff field from the tup */
  llvm::Value* llvm_t_hoff = irb->CreateLoad(
      codegen_utils->GetPointerToMember(llvm_t_data,
                                        &HeapTupleHeaderData::t_hoff));

  /* Find the start of input data byte array */
  llvm::Value* llvm_input_start = irb->CreateInBoundsGEP(
      llvm_t_data, {llvm_t_hoff});
  llvm::Value* llvm_t_infomask = codegen_utils->
      GetPointerToMember(llvm_t_data, &HeapTupleHeaderData::t_infomask);

  /* Find the start of the output datum array */
  llvm::Value* llvm_out_values = irb->CreateLoad(
      codegen_utils->GetPointerToMember(llvm_slot,
                                        &TupleTableSlot::PRIVATE_tts_values));
  llvm::Value* llvm_out_is_null =
      irb->CreateBitCast(irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_isnull)),
                         codegen_utils->GetType<void*>());

  /*
   * Examine if we tuple has nulls and fall back
   * to regular slot_deform_tuple accordingly.
   */
  llvm::Value* llvm_loaded_t_infomask = irb->CreateLoad(llvm_t_infomask);
  uint16 null_mask = HEAP_HASNULL;
  llvm::Value* llvm_heap_tuple_has_null = irb->CreateAnd(
      llvm_loaded_t_infomask, codegen_utils->GetConstant(null_mask));

  uint16 uint16_zero = 0;
  /* llvm_heap_tuple_has_null != 0 means we have nulls */
  llvm::Value* llvm_has_null = irb->CreateICmpNE(
      llvm_heap_tuple_has_null, codegen_utils->GetConstant(uint16_zero));

  /* Fallback if tuple has nulls*/
  irb->CreateCondBr(llvm_has_null, fallback_case, gen_code_case);

  /* Implement fallback */
  irb->SetInsertPoint(fallback_case);

  const char* fallback_log_msg = "Falling back to regular slot_deform_tuple.";
  llvm::Value* llvm_fallback_log_msg = codegen_utils->GetConstant(
      fallback_log_msg);
  codegen_utils->ir_builder()->CreateCall(llvm_elog_wrapper,
        { llvm_fallback_log_msg });

  auto regular_func_pointer = GetRegularFuncPointer();
  llvm::Function* llvm_regular_function =
		  codegen_utils->RegisterExternalFunction(regular_func_pointer);
  assert(llvm_regular_function != nullptr);

  std::vector<llvm::Value*> forwarded_args;

  for (llvm::Argument& arg : slot_deform_tuple_func->args()) {
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

  /* Generate code of slot_deform_tuple */
  irb->SetInsertPoint(gen_code_case);

  llvm::Value* true_const = codegen_utils->GetConstant(true);
  llvm::Value* datum_size_const = codegen_utils->GetConstant(sizeof(Datum));
  llvm::Value* bool_size_const = codegen_utils->GetConstant(sizeof(bool));

  int off = 0;
  Form_pg_attribute *att = tupleDesc->attrs;
  for (int attnum = 0; attnum < tuple_desc_natts; attnum++)
  {
    Form_pg_attribute thisatt = att[attnum];
    off = att_align(off, thisatt->attalign);

    // If any thisatt is varlen
    if (thisatt->attlen < 0)
    {
      /* Manager will clean up incomplete generated code. */
      return false;
    }

    // The next address of the input array where we need to read.
    llvm::Value* next_address_load =
        irb->CreateInBoundsGEP(llvm_input_start,
                               {codegen_utils->GetConstant(off)});

    llvm::Value* next_address_store =
        irb->CreateInBoundsGEP(llvm_out_values,
                               {codegen_utils->GetConstant(attnum)});

    llvm::Value* colVal = nullptr;

    if( thisatt->attbyval)
    {
      // Load the value from the calculated input address.
      switch(thisatt->attlen)
      {
        case sizeof(char):
          // Read 1 byte at next_address_load
          colVal = irb->CreateLoad(next_address_load);
          // store colVal into out_values[attnum]
          break;
        case sizeof(int16):
          colVal = irb->CreateLoad(
              codegen_utils->GetType<int16>(),irb->CreateBitCast(
                  next_address_load, codegen_utils->GetType<int16*>()));
          break;
        case sizeof(int32):
          colVal = irb->CreateLoad(
              codegen_utils->GetType<int32>(),
              irb->CreateBitCast(next_address_load,
                                 codegen_utils->GetType<int32*>()));
          break;
        case sizeof(int64):  /* Size of Datum */
          colVal = irb->CreateLoad(
              codegen_utils->GetType<int64>(), irb->CreateBitCast(
                  next_address_load, codegen_utils->GetType<int64*>()));
          break;
        default:
          /* Manager will clean up the incomplete generated code. */
          return false;
      }
    }
    else
    {
      /* Treat it as a pointer (PointerGetDatum). */
      colVal = irb->CreateLoad(
          codegen_utils->GetType<int64>(), irb->CreateBitCast(
              next_address_load, codegen_utils->GetType<int64*>()));
    }

    llvm::Value* int64ColVal = irb->CreateZExt(colVal, codegen_utils->GetType<int64>());
    irb->CreateStore(int64ColVal, next_address_store);

    llvm::LoadInst* load_instruction =
        irb->CreateLoad(next_address_load, "input");

    off += thisatt->attlen;
  }

  llvm::Function* llvm_memset = codegen_utils->RegisterExternalFunction(memset);
  assert(nullptr != llvm_memset);

  llvm::Value* llvm_fill_size = codegen_utils->GetConstant(
      tuple_desc_natts * sizeof(*slot_->PRIVATE_tts_isnull));
  llvm::Value* llvm_fill_val = codegen_utils->GetConstant(0);

  llvm::CallInst* call_llvm_memset = codegen_utils->ir_builder()->CreateCall(
      llvm_memset, {llvm_out_is_null, llvm_fill_val, llvm_fill_size});

  irb->CreateRetVoid();

  return true;
}


bool SlotDeformTupleCodegen::GenerateCodeInternal(CodegenUtils* codegen_utils) {
  bool isGenerated = GenerateSlotDeformTuple(codegen_utils);

  if (isGenerated)
  {
    elog(INFO, "slot_deform_tuple was generated successfully!");
    return true;
  }

  return false;
}
