//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    pg_date_func_generator.cc
//
//  @doc:
//    Base class for date functions to generate code
//
//---------------------------------------------------------------------------


#include "codegen/pg_date_func_generator.h"

extern "C" {
#include "postgres.h"
#include "utils/elog.h"
#include "utils/date.h"
#include "utils/timestamp.h"
}


using gpcodegen::CodegenUtils;
using gpcodegen::PGDateFuncGenerator;


class ElogWrapper {
 public:
  explicit ElogWrapper(gpcodegen::CodegenUtils* codegen_utils) :
  codegen_utils_(codegen_utils) {
    SetupElog();
  }
  ~ElogWrapper() {
    TearDownElog();
  }

  template<typename... V>
  void CreateElog(
      llvm::Value* llvm_elevel,
      llvm::Value* llvm_fmt,
      V ... args ) {
    assert(NULL != llvm_elevel);
    assert(NULL != llvm_fmt);

    codegen_utils_->ir_builder()->CreateCall(
        llvm_elog_start_, {
            codegen_utils_->GetConstant(""),  // Filename
            codegen_utils_->GetConstant(0),   // line number
            codegen_utils_->GetConstant("")   // function name
    });
    codegen_utils_->ir_builder()->CreateCall(
        llvm_elog_finish_, {
            llvm_elevel,
            llvm_fmt,
            args...
    });
  }
  template<typename... V>
  void CreateElog(
      int elevel,
      const char* fmt,
      V ... args ) {
    CreateElog(codegen_utils_->GetConstant(elevel),
               codegen_utils_->GetConstant(fmt),
               args...);
  }

 private:
  llvm::Function* llvm_elog_start_;
  llvm::Function* llvm_elog_finish_;

  gpcodegen::CodegenUtils* codegen_utils_;

  void SetupElog() {
    assert(codegen_utils_ != nullptr);
    llvm_elog_start_ = codegen_utils_->RegisterExternalFunction(elog_start);
    assert(llvm_elog_start_ != nullptr);
    llvm_elog_finish_ = codegen_utils_->RegisterExternalFunction(elog_finish);
    assert(llvm_elog_finish_ != nullptr);
  }

  void TearDownElog(){
    llvm_elog_start_ = nullptr;
    llvm_elog_finish_ = nullptr;
  }
};


bool PGDateFuncGenerator::DateLETimestamp(
    gpcodegen::CodegenUtils* codegen_utils,
    llvm::Function* llvm_main_func,
    llvm::BasicBlock* llvm_error_block,
    std::vector<llvm::Value*>& llvm_args,
    llvm::Value* & llvm_out_value) {

  llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

  llvm::Value* llvm_arg0_DateADT = codegen_utils->
      CreateCast<DateADT>(llvm_args[0]);
  /* TODO: Use CreateCast*/
  llvm::Value* llvm_arg1_Timestamp = irb->CreateSExt(
      llvm_args[1], codegen_utils->GetType<Timestamp>());

  llvm::Value* llvm_arg0_Timestamp = GenerateDate2Timestamp(
      codegen_utils, llvm_main_func, llvm_arg0_DateADT, llvm_error_block);

  // timestamp_cmp_internal {{{
#ifdef HAVE_INT64_TIMESTAMP
  llvm_out_value = irb->CreateICmpSLE(llvm_arg0_Timestamp, llvm_arg1_Timestamp);
#else
  //TODO: We do not support NaNs.
  ElogWrapper elogwrapper(codegen_utils);
  elogwrapper.CreateElog(ERROR,"Timestamp != int_64: NaNs are not supported.");
  return false;
#endif
  //}}}

  return true;
}

llvm::Value* PGDateFuncGenerator::GenerateDate2Timestamp(
    CodegenUtils* codegen_utils,
    llvm::Function* llvm_main_func,
    llvm::Value* llvm_arg,
    llvm::BasicBlock* llvm_error_block) {

  assert(nullptr != llvm_arg && nullptr != llvm_arg->getType());
  llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

#ifdef HAVE_INT64_TIMESTAMP

  llvm::BasicBlock* llvm_non_overflow_block = codegen_utils->CreateBasicBlock(
      "date_non_overflow_block", llvm_main_func);
  llvm::BasicBlock* llvm_overflow_block = codegen_utils->CreateBasicBlock(
      "date_overflow_block", llvm_main_func);

  llvm::Value *llvm_USECS_PER_DAY = codegen_utils->
      GetConstant<int64_t>(USECS_PER_DAY);

  /* TODO: Use CreateCast*/
  llvm::Value* llvm_arg_64 = irb->CreateSExt(
      llvm_arg, codegen_utils->GetType<int64_t>());

  llvm::Value* llvm_mul_output = codegen_utils->CreateMulOverflow<int64_t>(
      llvm_USECS_PER_DAY, llvm_arg_64);

  llvm::Value* llvm_overflow_flag = irb->CreateExtractValue(llvm_mul_output, 1);

  irb->CreateCondBr(
      irb->CreateICmpEQ(llvm_overflow_flag,
                        codegen_utils->GetConstant<bool>(true)),
                        llvm_overflow_block,
                        llvm_non_overflow_block );

  irb->SetInsertPoint(llvm_overflow_block);
  ElogWrapper elogwrapper(codegen_utils);
  elogwrapper.CreateElog(ERROR, "date out of range for timestamp");
  irb->CreateBr(llvm_error_block);

  irb->SetInsertPoint(llvm_non_overflow_block);

  return irb->CreateExtractValue(llvm_mul_output, 0);
#else
  llvm::Value *llvm_USECS_PER_DAY = codegen_utils->
      GetConstant<int64_t>(USECS_PER_DAY);
  /*TODO: Use CreateCast*/
  llvm::Value *llvm_arg_64 = irb->CreateSExt(llvm_arg,
                                             codegen_utils->GetType<int64_t>());
  return irb->CreateMul(llvm_arg_64, llvm_USECS_PER_DAY);
#endif
}
