//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    slot_deform_tuple_codegen.h
//
//  @doc:
//    Headers for slot_deform_tuple codegen
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_EXECVARIABLELIST_CODEGEN_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_EXECVARIABLELIST_CODEGEN_H_

#include "codegen/codegen_wrapper.h"
#include "codegen/base_codegen.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

class ExecVariableListCodegen: public BaseCodegen<ExecVariableListFn> {
 public:
  /**
   * @brief Constructor
   *
   * @param regular_func_ptr       Regular version of the target function.
   * @param ptr_to_chosen_func_ptr Reference to the function pointer that the caller will call.
   * @param slot         The slot to use for generating code.
   *
   * @note 	The ptr_to_chosen_func_ptr can refer to either the generated function or the
   * 			corresponding regular version.
   *
   **/
  explicit ExecVariableListCodegen(ExecVariableListFn regular_func_ptr,
                                  ExecVariableListFn* ptr_to_regular_func_ptr,
                                  ProjectionInfo* proj_info,
								  TupleTableSlot* slot);

  virtual ~ExecVariableListCodegen() = default;

 protected:
  bool GenerateCodeInternal(gpcodegen::CodegenUtils* codegen_utils) final;

 private:
  ProjectionInfo* proj_info_;
  TupleTableSlot* slot_;

  static constexpr char kExecVariableListPrefix[] = "ExecVariableList";

  /**
   * @brief Generates runtime code that implements slot_deform_tuple.
   *
   * @param codegen_utils Utility to ease the code generation process.
   * @return true on successful generation.
   **/
  bool GenerateExecVariableList(gpcodegen::CodegenUtils* codegen_utils);
};

/** @} */

}  // namespace gpcodegen
#endif  // GPCODEGEN_EXECVARIABLELIST_CODEGEN_H_
