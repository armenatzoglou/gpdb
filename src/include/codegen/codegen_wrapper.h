//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.
//
//	@filename:
//		codegen_wrapper.h
//
//	@doc:
//		C wrappers for initialization of code generation.
//
//---------------------------------------------------------------------------
#ifndef CODEGEN_WRAPPER_H_
#define CODEGEN_WRAPPER_H_

#include "pg_config.h"
#include "c.h"

typedef int64 Datum;

/*
 * Code that needs to be shared irrespective of whether USE_CODEGEN is enabled or not.
 */
struct TupleTableSlot;
struct ProjectionInfo;

typedef void (*SlotDeformTupleFn) (struct TupleTableSlot *slot, int natts);
typedef void (*ExecVariableListFn) (struct ProjectionInfo *projInfo, Datum *values, bool *isnull);

#ifndef USE_CODEGEN

#define InitCodegen();
#define CodeGeneratorManagerCreate(module_name) NULL
#define CodeGeneratorManagerGenerateCode(manager);
#define CodeGeneratorManagerPrepareGeneratedFunctions(manager) 1
#define CodeGeneratorManagerNotifyParameterChange(manager) 1
#define CodeGeneratorManagerDestroy(manager);
#define GetActiveCodeGeneratorManager() NULL
#define SetActiveCodeGeneratorManager(manager);

#define START_CODE_GENERATOR_MANAGER(newManager)
#define END_CODE_GENERATOR_MANAGER()

#define init_codegen()
#define call_slot_deform_tuple(slot, attno) slot_deform_tuple(slot, attno)
#define call_ExecVariableList(projInfo, values, isnull) ExecVariableList(projInfo, values, isnull)
#define enroll_slot_deform_tuple_codegen(regular_func, ptr_to_chosen_func, slot)
#define enroll_ExecVariableList(regular_func, ptr_to_chosen_func, proj_info)

#else

/*
 * Forward extern declaration of slot deform tuple if code gen is enabled
 */
extern void slot_deform_tuple(struct TupleTableSlot *slot, int natts);

extern void ExecVariableList(struct ProjectionInfo *projInfo, Datum *values, bool *isnull);

/*
 * @brief Life span of Code generator instance
 *
 * @note Each code generator is responsible to generate code for one specific function.
 *       Each generated function has a life span to indicate to the manager about when to
 *       invalidate and regenerate this function. The enroller is responsible to know
 *       how long a generated code should be valid.
 *
 *
 */
typedef enum CodegenFuncLifespan
{
	// Does not depend on parameter changes
	CodegenFuncLifespan_Parameter_Invariant,
	// Has to be regenerated as the parameter changes
	CodegenFuncLifespan_Parameter_Variant
} CodegenFuncLifespan;


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Do one-time global initialization of LLVM library. Returns 1
 * on success, 0 on error.
 */
unsigned int
InitCodegen();

/*
 * Creates a manager for an operator
 */
void*
CodeGeneratorManagerCreate(const char* module_name);

/*
 * Calls all the registered CodegenInterface to generate code
 */
unsigned int
CodeGeneratorManagerGenerateCode(void* manager);

/*
 * Compiles and prepares all the Codegen function pointers. Returns
 * number of successfully generated functions
 */
unsigned int
CodeGeneratorManagerPrepareGeneratedFunctions(void* manager);

/*
 * Notifies a manager that the underlying operator has a parameter change
 */
unsigned int
CodeGeneratorManagerNotifyParameterChange(void* manager);

/*
 * Destroys a manager for an operator
 */
void
CodeGeneratorManagerDestroy(void* manager);

/*
 * Get the active code generator manager
 */
void*
GetActiveCodeGeneratorManager();

/*
 * Set the active code generator manager
 */
void
SetActiveCodeGeneratorManager(void* manager);

/*
 * returns the pointer to the SlotDeformTupleCodegen
 */
void*
SlotDeformTupleCodegenEnroll(SlotDeformTupleFn regular_func_ptr,
                              SlotDeformTupleFn* ptr_to_regular_func_ptr,
                              struct TupleTableSlot* slot);


void*
ExecVariableListCodegenEnroll(ExecVariableListFn regular_func_ptr,
                              ExecVariableListFn* ptr_to_regular_func_ptr,
                              struct ProjectionInfo* proj_info);

#ifdef __cplusplus
}  // extern "C"
#endif

/*
 * START_CODE_GENERATOR_MANAGER would switch to the specified code generator manager,
 * saving the oldCodeGeneratorManager. Must be paired with END_CODE_GENERATOR_MANAGER
 */
#define START_CODE_GENERATOR_MANAGER(newManager)  \
	do { \
		void *oldManager = NULL; \
		oldManager = GetActiveCodeGeneratorManager(); \
		if (codegen) \
			Assert(newManager != NULL); \
		SetActiveCodeGeneratorManager(newManager);\
/*
 * END_CODE_GENERATOR_MANAGER would restore the previous code generator manager that was
 * active at the time of START_CODE_GENERATOR_MANAGER call
 */
#define END_CODE_GENERATOR_MANAGER()  \
		SetActiveCodeGeneratorManager(oldManager);\
	} while (0);


/*
 * Initialize LLVM library
 */
#define init_codegen() \
	if (init_codegen) { \
			if (InitCodegen() == 0) { \
				ereport(FATAL, \
					(errcode(ERRCODE_INTERNAL_ERROR), \
					errmsg("failed to initialize LLVM library"), \
					errhint("LLVM library for code generation failed " \
						"to initialize. You may wish to disable " \
						"code generation by turning off the " \
						"\"codegen\" GUC."))); \
			} \
		} \

/*
 * Call slot_deform_tuple using function pointer slot_deform_tuple_fn.
 * Function pointer may point to regular version or generated function
 */
#define call_slot_deform_tuple(slot, attno) \
		slot->slot_deform_tuple_gen_info.slot_deform_tuple_fn(slot, attno)

#define call_ExecVariableList(projInfo, values, isnull) \
		projInfo->ExecVariableList_gen_info.ExecVariableList_fn(projInfo, values, isnull)
/*
 * Enroll given slot to codegen manager.
 * The enrollment process also ensures that the slot_deform_tuple_fn pointer
 * is set to the regular version initially
 */
#define enroll_slot_deform_tuple_codegen(regular_func, ptr_to_regular_func_ptr, slot) \
		slot->slot_deform_tuple_gen_info.code_generator = SlotDeformTupleCodegenEnroll( \
				regular_func, ptr_to_regular_func_ptr, slot); \
		Assert(slot->slot_deform_tuple_gen_info.slot_deform_tuple_fn == regular_func); \

#define enroll_ExecVariableList_codegen(regular_func, ptr_to_regular_func_ptr, proj_info) \
		proj_info->ExecVariableList_gen_info.code_generator = ExecVariableListCodegenEnroll( \
				regular_func, ptr_to_regular_func_ptr, proj_info); \
		Assert(proj_info->ExecVariableList_gen_info.ExecVariableList_fn == regular_func); \

#endif //USE_CODEGEN

#endif  // CODEGEN_WRAPPER_H_
