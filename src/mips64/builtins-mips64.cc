// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                CFunctionId id,
                                BuiltinExtraArguments extra_args) {
  // ----------- S t a t e -------------
  //  -- a0                 : number of arguments excluding receiver
  //  -- a1                 : target
  //  -- a3                 : new.target
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[8 * (argc - 1)] : first argument
  //  -- sp[8 * agrc]       : receiver
  // -----------------------------------
  __ AssertFunction(a1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // Insert extra arguments.
  int num_extra_args = 0;
  switch (extra_args) {
    case BuiltinExtraArguments::kTarget:
      __ Push(a1);
      ++num_extra_args;
      break;
    case BuiltinExtraArguments::kNewTarget:
      __ Push(a3);
      ++num_extra_args;
      break;
    case BuiltinExtraArguments::kTargetAndNewTarget:
      __ Push(a1, a3);
      num_extra_args += 2;
      break;
    case BuiltinExtraArguments::kNone:
      break;
  }

  // JumpToExternalReference expects a0 to contain the number of arguments
  // including the receiver and the extra arguments.
  __ Daddu(a0, a0, num_extra_args + 1);

  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the InternalArray function from the native context.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, result);
}


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the Array function from the native context.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, result);
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, a1);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ ld(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, a4);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction,
              a4, Operand(zero_reg));
    __ GetObjectType(a2, a3, a4);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction,
              a4, Operand(MAP_TYPE));
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // Tail call a stub.
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, a1);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ ld(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, a4);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction1,
              a4, Operand(zero_reg));
    __ GetObjectType(a2, a3, a4);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction2,
              a4, Operand(MAP_TYPE));
  }

  // Run the native code for the Array function called as a normal function.
  // Tail call a stub.
  __ mov(a3, a1);
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


// static
void Builtins::Generate_MathMaxMin(MacroAssembler* masm, MathMaxMinKind kind) {
  // ----------- S t a t e -------------
  //  -- a0                 : number of arguments
  //  -- ra                 : return address
  //  -- sp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- sp[(argc + 1) * 8] : receiver
  // -----------------------------------
  Heap::RootListIndex const root_index =
      (kind == MathMaxMinKind::kMin) ? Heap::kInfinityValueRootIndex
                                     : Heap::kMinusInfinityValueRootIndex;

  // Load the accumulator with the default return value (either -Infinity or
  // +Infinity), with the tagged value in a1 and the double value in f0.
  __ LoadRoot(a1, root_index);
  __ ldc1(f0, FieldMemOperand(a1, HeapNumber::kValueOffset));
  __ Addu(a3, a0, 1);

  Label done_loop, loop;
  __ bind(&loop);
  {
    // Check if all parameters done.
    __ Dsubu(a0, a0, Operand(1));
    __ Branch(&done_loop, lt, a0, Operand(zero_reg));

    // Load the next parameter tagged value into a2.
    __ Dlsa(at, sp, a0, kPointerSizeLog2);
    __ ld(a2, MemOperand(at));

    // Load the double value of the parameter into f2, maybe converting the
    // parameter to a number first using the ToNumberStub if necessary.
    Label convert, convert_smi, convert_number, done_convert;
    __ bind(&convert);
    __ JumpIfSmi(a2, &convert_smi);
    __ ld(a4, FieldMemOperand(a2, HeapObject::kMapOffset));
    __ JumpIfRoot(a4, Heap::kHeapNumberMapRootIndex, &convert_number);
    {
      // Parameter is not a Number, use the ToNumberStub to convert it.
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(a0);
      __ SmiTag(a3);
      __ Push(a0, a1, a3);
      __ mov(a0, a2);
      ToNumberStub stub(masm->isolate());
      __ CallStub(&stub);
      __ mov(a2, v0);
      __ Pop(a0, a1, a3);
      {
        // Restore the double accumulator value (f0).
        Label restore_smi, done_restore;
        __ JumpIfSmi(a1, &restore_smi);
        __ ldc1(f0, FieldMemOperand(a1, HeapNumber::kValueOffset));
        __ jmp(&done_restore);
        __ bind(&restore_smi);
        __ SmiToDoubleFPURegister(a1, f0, a4);
        __ bind(&done_restore);
      }
      __ SmiUntag(a3);
      __ SmiUntag(a0);
    }
    __ jmp(&convert);
    __ bind(&convert_number);
    __ ldc1(f2, FieldMemOperand(a2, HeapNumber::kValueOffset));
    __ jmp(&done_convert);
    __ bind(&convert_smi);
    __ SmiToDoubleFPURegister(a2, f2, a4);
    __ bind(&done_convert);

    // Perform the actual comparison with using Min/Max macro instructions the
    // accumulator value on the left hand side (f0) and the next parameter value
    // on the right hand side (f2).
    // We need to work out which HeapNumber (or smi) the result came from.
    Label compare_nan;
    __ BranchF(nullptr, &compare_nan, eq, f0, f2);
    __ Move(a4, f0);
    if (kind == MathMaxMinKind::kMin) {
      __ MinNaNCheck_d(f0, f0, f2);
    } else {
      DCHECK(kind == MathMaxMinKind::kMax);
      __ MaxNaNCheck_d(f0, f0, f2);
    }
    __ Move(at, f0);
    __ Branch(&loop, eq, a4, Operand(at));
    __ mov(a1, a2);
    __ jmp(&loop);

    // At least one side is NaN, which means that the result will be NaN too.
    __ bind(&compare_nan);
    __ LoadRoot(a1, Heap::kNanValueRootIndex);
    __ ldc1(f0, FieldMemOperand(a1, HeapNumber::kValueOffset));
    __ jmp(&loop);
  }

  __ bind(&done_loop);
  __ Dlsa(sp, sp, a3, kPointerSizeLog2);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a1);  // In delay slot.
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(a0, a0, Operand(1));
    __ Dlsa(sp, sp, a0, kPointerSizeLog2);
    __ ld(a0, MemOperand(sp));
    __ Drop(2);
  }

  // 2a. Convert first argument to number.
  ToNumberStub stub(masm->isolate());
  __ TailCallStub(&stub);

  // 2b. No arguments, return +0.
  __ bind(&no_arguments);
  __ Move(v0, Smi::FromInt(0));
  __ DropAndRet(1);
}


void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- a3                     : new target
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // 2. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(a0, a0, Operand(1));
    __ Dlsa(sp, sp, a0, kPointerSizeLog2);
    __ ld(a0, MemOperand(sp));
    __ Drop(2);
    __ jmp(&done);
    __ bind(&no_arguments);
    __ Move(a0, Smi::FromInt(0));
    __ Drop(1);
    __ bind(&done);
  }

  // 3. Make sure a0 is a number.
  {
    Label done_convert;
    __ JumpIfSmi(a0, &done_convert);
    __ GetObjectType(a0, a2, a2);
    __ Branch(&done_convert, eq, t0, Operand(HEAP_NUMBER_TYPE));
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(a1, a3);
      ToNumberStub stub(masm->isolate());
      __ CallStub(&stub);
      __ Move(a0, v0);
      __ Pop(a1, a3);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label new_object;
  __ Branch(&new_object, ne, a1, Operand(a3));

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(v0, a1, a0, a2, t0, &new_object);
  __ Ret();

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a0);
    FastNewObjectStub stub(masm->isolate());
    __ CallStub(&stub);
    __ Pop(a0);
  }
  __ Ret(USE_DELAY_SLOT);
  __ sd(a0, FieldMemOperand(v0, JSValue::kValueOffset));  // In delay slot.
}


// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(a0, a0, Operand(1));
    __ Dlsa(sp, sp, a0, kPointerSizeLog2);
    __ ld(a0, MemOperand(sp));
    __ Drop(2);
  }

  // 2a. At least one argument, return a0 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(a0, &to_string);
    __ GetObjectType(a0, a1, a1);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ Subu(a1, a1, Operand(FIRST_NONSTRING_TYPE));
    __ Branch(&symbol_descriptive_string, eq, a1, Operand(zero_reg));
    __ Branch(&to_string, gt, a1, Operand(zero_reg));
    __ Ret(USE_DELAY_SLOT);
    __ mov(v0, a0);
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(v0, Heap::kempty_stringRootIndex);
    __ DropAndRet(1);
  }

  // 3a. Convert a0 to a string.
  __ bind(&to_string);
  {
    ToStringStub stub(masm->isolate());
    __ TailCallStub(&stub);
  }

  // 3b. Convert symbol in a0 to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ Push(a0);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }
}


void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- a3                     : new target
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // 2. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(a0, a0, Operand(1));
    __ Dlsa(sp, sp, a0, kPointerSizeLog2);
    __ ld(a0, MemOperand(sp));
    __ Drop(2);
    __ jmp(&done);
    __ bind(&no_arguments);
    __ LoadRoot(a0, Heap::kempty_stringRootIndex);
    __ Drop(1);
    __ bind(&done);
  }

  // 3. Make sure a0 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(a0, &convert);
    __ GetObjectType(a0, a2, a2);
    __ And(t0, a2, Operand(kIsNotStringMask));
    __ Branch(&done_convert, eq, t0, Operand(zero_reg));
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ Push(a1, a3);
      __ CallStub(&stub);
      __ Move(a0, v0);
      __ Pop(a1, a3);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label new_object;
  __ Branch(&new_object, ne, a1, Operand(a3));

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(v0, a1, a0, a2, t0, &new_object);
  __ Ret();

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a0);
    FastNewObjectStub stub(masm->isolate());
    __ CallStub(&stub);
    __ Pop(a0);
  }
  __ Ret(USE_DELAY_SLOT);
  __ sd(a0, FieldMemOperand(v0, JSValue::kValueOffset));  // In delay slot.
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(a2, FieldMemOperand(a2, SharedFunctionInfo::kCodeOffset));
  __ Daddu(at, a2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a1 : target function (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the function onto the stack.
    // Push a copy of the target function and the new target.
    __ SmiTag(a0);
    __ Push(a0, a1, a3, a1);

    __ CallRuntime(function_id, 1);
    // Restore target function and new target.
    __ Pop(a0, a1, a3);
    __ SmiUntag(a0);
  }

  __ Daddu(at, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ LoadRoot(a4, Heap::kStackLimitRootIndex);
  __ Branch(&ok, hs, sp, Operand(a4));

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool create_implicit_receiver,
                                           bool check_derived_construct) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a2     : allocation site or undefined
  //  -- a3     : new target
  //  -- cp     : context
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(a2, t0);
    __ SmiTag(a0);
    __ Push(cp, a2, a0);

    if (create_implicit_receiver) {
      __ Push(a1, a3);
      FastNewObjectStub stub(masm->isolate());
      __ CallStub(&stub);
      __ mov(t0, v0);
      __ Pop(a1, a3);

      // ----------- S t a t e -------------
      // -- a1: constructor function
      // -- a3: new target
      // -- t0: newly allocated object
      // -----------------------------------
      __ ld(a0, MemOperand(sp));
    }
    __ SmiUntag(a0);

    if (create_implicit_receiver) {
      // Push the allocated receiver to the stack. We need two copies
      // because we may have to return the original one and the calling
      // conventions dictate that the called function pops the receiver.
      __ Push(t0, t0);
    } else {
      __ PushRoot(Heap::kTheHoleValueRootIndex);
    }

    // Set up pointer to last argument.
    __ Daddu(a2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // a0: number of arguments
    // a1: constructor function
    // a2: address of last argument (caller sp)
    // a3: new target
    // t0: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: number of arguments (smi-tagged)
    Label loop, entry;
    __ mov(t0, a0);
    __ jmp(&entry);
    __ bind(&loop);
    __ Dlsa(a4, a2, t0, kPointerSizeLog2);
    __ ld(a5, MemOperand(a4));
    __ push(a5);
    __ bind(&entry);
    __ Daddu(t0, t0, Operand(-1));
    __ Branch(&loop, greater_equal, t0, Operand(zero_reg));

    // Call the function.
    // a0: number of arguments
    // a1: constructor function
    // a3: new target
    ParameterCount actual(a0);
    __ InvokeFunction(a1, a3, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // Store offset of return address for deoptimizer.
    if (create_implicit_receiver && !is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    __ ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    if (create_implicit_receiver) {
      // If the result is an object (in the ECMA sense), we should get rid
      // of the receiver and use the result; see ECMA-262 section 13.2.2-7
      // on page 74.
      Label use_receiver, exit;

      // If the result is a smi, it is *not* an object in the ECMA sense.
      // v0: result
      // sp[0]: receiver (newly allocated object)
      // sp[1]: number of arguments (smi-tagged)
      __ JumpIfSmi(v0, &use_receiver);

      // If the type of the result (stored in its map) is less than
      // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
      __ GetObjectType(v0, a1, a3);
      __ Branch(&exit, greater_equal, a3, Operand(FIRST_JS_RECEIVER_TYPE));

      // Throw away the result of the constructor invocation and use the
      // on-stack receiver as the result.
      __ bind(&use_receiver);
      __ ld(v0, MemOperand(sp));

      // Remove receiver from the stack, remove caller arguments, and
      // return.
      __ bind(&exit);
      // v0: result
      // sp[0]: receiver (newly allocated object)
      // sp[1]: number of arguments (smi-tagged)
      __ ld(a1, MemOperand(sp, 1 * kPointerSize));
    } else {
      __ ld(a1, MemOperand(sp));
    }

    // Leave construct frame.
  }

  // ES6 9.2.2. Step 13+
  // Check that the result is not a Smi, indicating that the constructor result
  // from a derived class is neither undefined nor an Object.
  if (check_derived_construct) {
    Label dont_throw;
    __ JumpIfNotSmi(v0, &dont_throw);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowDerivedConstructorReturnedNonObject);
    }
    __ bind(&dont_throw);
  }

  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  __ Daddu(sp, sp, kPointerSize);
  if (create_implicit_receiver) {
    __ IncrementCounter(isolate->counters()->constructed_objects(), 1, a1, a2);
  }
  __ Ret();
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, true, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true, false, false);
}


void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false, false);
}


void Builtins::Generate_JSBuiltinsConstructStubForDerived(
    MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false, true);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : the value to pass to the generator
  //  -- a1 : the JSGeneratorObject to resume
  //  -- a2 : the resume mode (tagged)
  //  -- ra : return address
  // -----------------------------------
  __ AssertGeneratorObject(a1);

  // Store input value into generator object.
  __ sd(v0, FieldMemOperand(a1, JSGeneratorObject::kInputOffset));
  __ RecordWriteField(a1, JSGeneratorObject::kInputOffset, v0, a3,
                      kRAHasNotBeenSaved, kDontSaveFPRegs);

  // Store resume mode into generator object.
  __ sd(a2, FieldMemOperand(a1, JSGeneratorObject::kResumeModeOffset));

  // Load suspended function and context.
  __ ld(cp, FieldMemOperand(a1, JSGeneratorObject::kContextOffset));
  __ ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  // Flood function if we are stepping.
  Label skip_flooding;
  ExternalReference step_in_enabled =
      ExternalReference::debug_step_in_enabled_address(masm->isolate());
  __ li(t1, Operand(step_in_enabled));
  __ lb(t1, MemOperand(t1));
  __ Branch(&skip_flooding, eq, t1, Operand(zero_reg));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, a2, a4);
    __ CallRuntime(Runtime::kDebugPrepareStepInIfStepping);
    __ Pop(a1, a2);
    __ ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  }
  __ bind(&skip_flooding);

  // Push receiver.
  __ ld(a5, FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
  __ Push(a5);

  // ----------- S t a t e -------------
  //  -- a1    : the JSGeneratorObject to resume
  //  -- a2    : the resume mode (tagged)
  //  -- a4    : generator function
  //  -- cp    : generator context
  //  -- ra    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ ld(a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a3,
        FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ Dsubu(a3, a3, Operand(1));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg));
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Dispatch on the kind of generator object.
  Label old_generator;
  __ ld(a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
  __ ld(a3, FieldMemOperand(a3, SharedFunctionInfo::kFunctionDataOffset));
  __ GetObjectType(a3, a3, a3);
  __ Branch(&old_generator, ne, a3, Operand(BYTECODE_ARRAY_TYPE));

  // New-style (ignition/turbofan) generator object.
  {
    __ ld(a0, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a0,
         FieldMemOperand(a0, SharedFunctionInfo::kFormalParameterCountOffset));
    __ SmiUntag(a0);
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(a3, a1);
    __ Move(a1, a4);
    __ ld(a2, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
    __ Jump(a2);
  }

  // Old-style (full-codegen) generator object
  __ bind(&old_generator);
  {
    // Enter a new JavaScript frame, and initialize its slots as they were when
    // the generator was suspended.
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Push(ra, fp);
    __ Move(fp, sp);
    __ Push(cp, a4);

    // Restore the operand stack.
    __ ld(a0, FieldMemOperand(a1, JSGeneratorObject::kOperandStackOffset));
    __ ld(a3, FieldMemOperand(a0, FixedArray::kLengthOffset));
    __ SmiUntag(a3);
    __ Daddu(a0, a0, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ Dlsa(a3, a0, a3, kPointerSizeLog2);
    {
      Label done_loop, loop;
      __ bind(&loop);
      __ Branch(&done_loop, eq, a0, Operand(a3));
      __ ld(a5, MemOperand(a0));
      __ Push(a5);
      __ Branch(USE_DELAY_SLOT, &loop);
      __ daddiu(a0, a0, kPointerSize);  // In delay slot.
      __ bind(&done_loop);
    }

    // Reset operand stack so we don't leak.
    __ LoadRoot(a5, Heap::kEmptyFixedArrayRootIndex);
    __ sd(a5, FieldMemOperand(a1, JSGeneratorObject::kOperandStackOffset));

    // Resume the generator function at the continuation.
    __ ld(a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
    __ ld(a3, FieldMemOperand(a3, SharedFunctionInfo::kCodeOffset));
    __ Daddu(a3, a3, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ ld(a2, FieldMemOperand(a1, JSGeneratorObject::kContinuationOffset));
    __ SmiUntag(a2);
    __ Daddu(a3, a3, Operand(a2));
    __ li(a2, Operand(Smi::FromInt(JSGeneratorObject::kGeneratorExecuting)));
    __ sd(a2, FieldMemOperand(a1, JSGeneratorObject::kContinuationOffset));
    __ Move(v0, a1);  // Continuation expects generator object in v0.
    __ Jump(a3);
  }
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(a1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}


enum IsTagged { kArgcIsSmiTagged, kArgcIsUntaggedInt };


// Clobbers a2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        IsTagged argc_is_tagged) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
  // Make a2 the space we have left. The stack might already be overflowed
  // here which will cause r2 to become negative.
  __ dsubu(a2, sp, a2);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ SmiScale(a7, v0, kPointerSizeLog2);
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ dsll(a7, argc, kPointerSizeLog2);
  }
  __ Branch(&okay, gt, a2, Operand(a7));  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from JSEntryStub::GenerateBody

  // ----------- S t a t e -------------
  //  -- a0: new.target
  //  -- a1: function
  //  -- a2: receiver_pointer
  //  -- a3: argc
  //  -- s0: argv
  // -----------------------------------
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ li(cp, Operand(context_address));
    __ ld(cp, MemOperand(cp));

    // Push the function and the receiver onto the stack.
    __ Push(a1, a2);

    // Check if we have enough stack space to push all arguments.
    // Clobbers a2.
    Generate_CheckStackOverflow(masm, a3, kArgcIsUntaggedInt);

    // Remember new.target.
    __ mov(a5, a0);

    // Copy arguments to the stack in a loop.
    // a3: argc
    // s0: argv, i.e. points to first arg
    Label loop, entry;
    __ Dlsa(a6, s0, a3, kPointerSizeLog2);
    __ b(&entry);
    __ nop();   // Branch delay slot nop.
    // a6 points past last arg.
    __ bind(&loop);
    __ ld(a4, MemOperand(s0));  // Read next parameter.
    __ daddiu(s0, s0, kPointerSize);
    __ ld(a4, MemOperand(a4));  // Dereference handle.
    __ push(a4);  // Push parameter.
    __ bind(&entry);
    __ Branch(&loop, ne, s0, Operand(a6));

    // Setup new.target and argc.
    __ mov(a0, a3);
    __ mov(a3, a5);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(a4, Heap::kUndefinedValueRootIndex);
    __ mov(s1, a4);
    __ mov(s2, a4);
    __ mov(s3, a4);
    __ mov(s4, a4);
    __ mov(s5, a4);
    // s6 holds the root address. Do not clobber.
    // s7 is cp. Do not init.

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Leave internal frame.
  }
  __ Jump(ra);
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o a1: the JS function object being called.
//   o a3: the new target
//   o cp: our context
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(a1);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ ld(a0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  Label load_debug_bytecode_array, bytecode_array_loaded;
  Register debug_info = kInterpreterBytecodeArrayRegister;
  DCHECK(!debug_info.is(a0));
  __ ld(debug_info, FieldMemOperand(a0, SharedFunctionInfo::kDebugInfoOffset));
  __ Branch(&load_debug_bytecode_array, ne, debug_info,
            Operand(DebugInfo::uninitialized()));
  __ ld(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(a0, SharedFunctionInfo::kFunctionDataOffset));
  __ bind(&bytecode_array_loaded);

  // Check function data field is actually a BytecodeArray object.
  Label bytecode_array_not_present;
  __ JumpIfRoot(kInterpreterBytecodeArrayRegister,
                Heap::kUndefinedValueRootIndex, &bytecode_array_not_present);
  if (FLAG_debug_code) {
    __ SmiTst(kInterpreterBytecodeArrayRegister, a4);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a4,
              Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a4, a4);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a4,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Push new.target, bytecode array and zero for bytecode array offset.
  __ Push(a3, kInterpreterBytecodeArrayRegister, zero_reg);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size (word) from the BytecodeArray object.
    __ lw(a4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ Dsubu(a5, sp, Operand(a4));
    __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
    __ Branch(&ok, hs, a5, Operand(a2));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(a5, Heap::kUndefinedValueRootIndex);
    __ Branch(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(a5);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ Dsubu(a4, a4, Operand(kPointerSize));
    __ Branch(&loop_header, ge, a4, Operand(zero_reg));
  }

  // Load bytecode offset and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ Daddu(a7, fp, Operand(InterpreterFrameConstants::kRegisterFileFromFp));
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ li(kInterpreterDispatchTableRegister,
        Operand(ExternalReference::interpreter_dispatch_table_address(
            masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ Daddu(a0, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ lbu(a0, MemOperand(a0));
  __ Dlsa(at, kInterpreterDispatchTableRegister, a0, kPointerSizeLog2);
  __ ld(at, MemOperand(at));
  __ Call(at);

  // Even though the first bytecode handler was called, we will never return.
  __ Abort(kUnexpectedReturnFromBytecodeHandler);

  // Load debug copy of the bytecode array.
  __ bind(&load_debug_bytecode_array);
  __ ld(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(debug_info, DebugInfo::kAbstractCodeIndex));
  __ Branch(&bytecode_array_loaded);

  // If the bytecode array is no longer present, then the underlying function
  // has been switched to a different kind of code and we heal the closure by
  // switching the code entry field over to the new code object as well.
  __ bind(&bytecode_array_not_present);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  __ ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(a4, FieldMemOperand(a4, SharedFunctionInfo::kCodeOffset));
  __ Daddu(a4, a4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ sd(a4, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(a1, a4, a5);
  __ Jump(a4);
}


void Builtins::Generate_InterpreterExitTrampoline(MacroAssembler* masm) {
  // The return value is in accumulator, which is already in v0.

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments and return.
  __ lw(at, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                            BytecodeArray::kParameterSizeOffset));
  __ Daddu(sp, sp, at);
  __ Jump(ra);
}


// static
void Builtins::Generate_InterpreterPushArgsAndCallImpl(
    MacroAssembler* masm, TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  // Find the address of the last argument.
  __ Daddu(a3, a0, Operand(1));  // Add one for receiver.
  __ dsll(a3, a3, kPointerSizeLog2);
  __ Dsubu(a3, a2, Operand(a3));

  // Push the arguments.
  Label loop_header, loop_check;
  __ Branch(&loop_check);
  __ bind(&loop_header);
  __ ld(t0, MemOperand(a2));
  __ Daddu(a2, a2, Operand(-kPointerSize));
  __ push(t0);
  __ bind(&loop_check);
  __ Branch(&loop_header, gt, a2, Operand(a3));

  // Call the target.
  __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                            tail_call_mode),
          RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_InterpreterPushArgsAndConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  // -- a0 : argument count (not including receiver)
  // -- a3 : new target
  // -- a1 : constructor to call
  // -- a2 : address of the first argument
  // -----------------------------------

  // Find the address of the last argument.
  __ dsll(t0, a0, kPointerSizeLog2);
  __ Dsubu(t0, a2, Operand(t0));

  // Push a slot for the receiver.
  __ push(zero_reg);

  // Push the arguments.
  Label loop_header, loop_check;
  __ Branch(&loop_check);
  __ bind(&loop_header);
  __ ld(t1, MemOperand(a2));
  __ Daddu(a2, a2, Operand(-kPointerSize));
  __ push(t1);
  __ bind(&loop_check);
  __ Branch(&loop_header, gt, a2, Operand(t0));

  // Call the constructor with a0, a1, and a3 unmodified.
  __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}


static void Generate_EnterBytecodeDispatch(MacroAssembler* masm) {
  // Initialize the dispatch table register.
  __ li(kInterpreterDispatchTableRegister,
        Operand(ExternalReference::interpreter_dispatch_table_address(
            masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ ld(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister, at);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, at,
              Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a1, a1);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a1,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Get the target bytecode offset from the frame.
  __ ld(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ Daddu(a1, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ lbu(a1, MemOperand(a1));
  __ Dlsa(a1, kInterpreterDispatchTableRegister, a1, kPointerSizeLog2);
  __ ld(a1, MemOperand(a1));
  __ Jump(a1);
}


static void Generate_InterpreterNotifyDeoptimizedHelper(
    MacroAssembler* masm, Deoptimizer::BailoutType type) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Pass the deoptimization type to the runtime system.
    __ li(a1, Operand(Smi::FromInt(static_cast<int>(type))));
    __ push(a1);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
    // Tear down internal frame.
  }

  // Drop state (we don't use these for interpreter deopts) and and pop the
  // accumulator value into the accumulator register.
  __ Drop(1);
  __ Pop(kInterpreterAccumulatorRegister);

  // Enter the bytecode dispatch.
  Generate_EnterBytecodeDispatch(masm);
}


void Builtins::Generate_InterpreterNotifyDeoptimized(MacroAssembler* masm) {
  Generate_InterpreterNotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_InterpreterNotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_InterpreterNotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}


void Builtins::Generate_InterpreterNotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_InterpreterNotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  // Set the address of the interpreter entry trampoline as a return address.
  // This simulates the initial call to bytecode handlers in interpreter entry
  // trampoline. The return will never actually be taken, but our stack walker
  // uses this address to determine whether a frame is interpreted.
  __ li(ra, Operand(masm->isolate()->builtins()->InterpreterEntryTrampoline()));

  Generate_EnterBytecodeDispatch(masm);
}


void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  //  -- a1 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime, gotta_call_runtime_no_stack;
  Label maybe_call_runtime;
  Label try_shared;
  Label loop_top, loop_bottom;

  Register argument_count = a0;
  Register closure = a1;
  Register new_target = a3;
  __ push(argument_count);
  __ push(new_target);
  __ push(closure);

  Register map = a0;
  Register index = a2;
  __ ld(map, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ ld(map, FieldMemOperand(map, SharedFunctionInfo::kOptimizedCodeMapOffset));
  __ ld(index, FieldMemOperand(map, FixedArray::kLengthOffset));
  __ Branch(&gotta_call_runtime, lt, index, Operand(Smi::FromInt(2)));

  // Find literals.
  // a3  : native context
  // a2  : length / index
  // a0  : optimized code map
  // stack[0] : new target
  // stack[4] : closure
  Register native_context = a3;
  __ ld(native_context, NativeContextMemOperand());

  __ bind(&loop_top);
  Register temp = a1;
  Register array_pointer = a5;

  // Does the native context match?
  __ SmiScale(at, index, kPointerSizeLog2);
  __ Daddu(array_pointer, map, Operand(at));
  __ ld(temp, FieldMemOperand(array_pointer,
                              SharedFunctionInfo::kOffsetToPreviousContext));
  __ ld(temp, FieldMemOperand(temp, WeakCell::kValueOffset));
  __ Branch(&loop_bottom, ne, temp, Operand(native_context));
  // OSR id set to none?
  __ ld(temp, FieldMemOperand(array_pointer,
                              SharedFunctionInfo::kOffsetToPreviousOsrAstId));
  const int bailout_id = BailoutId::None().ToInt();
  __ Branch(&loop_bottom, ne, temp, Operand(Smi::FromInt(bailout_id)));
  // Literals available?
  __ ld(temp, FieldMemOperand(array_pointer,
                              SharedFunctionInfo::kOffsetToPreviousLiterals));
  __ ld(temp, FieldMemOperand(temp, WeakCell::kValueOffset));
  __ JumpIfSmi(temp, &gotta_call_runtime);

  // Save the literals in the closure.
  __ ld(a4, MemOperand(sp, 0));
  __ sd(temp, FieldMemOperand(a4, JSFunction::kLiteralsOffset));
  __ push(index);
  __ RecordWriteField(a4, JSFunction::kLiteralsOffset, temp, index,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(index);

  // Code available?
  Register entry = a4;
  __ ld(entry,
        FieldMemOperand(array_pointer,
                        SharedFunctionInfo::kOffsetToPreviousCachedCode));
  __ ld(entry, FieldMemOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &maybe_call_runtime);

  // Found literals and code. Get them into the closure and return.
  __ pop(closure);
  // Store code entry in the closure.
  __ Daddu(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));

  Label install_optimized_code_and_tailcall;
  __ bind(&install_optimized_code_and_tailcall);
  __ sd(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, entry, a5);

  // Link the closure into the optimized function list.
  // a4 : code entry
  // a3 : native context
  // a1 : closure
  __ ld(a5,
        ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ sd(a5, FieldMemOperand(closure, JSFunction::kNextFunctionLinkOffset));
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, a5, a0,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ sd(closure,
        ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  // Save closure before the write barrier.
  __ mov(a5, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, closure, a0,
                            kRAHasNotBeenSaved, kDontSaveFPRegs);
  __ mov(closure, a5);
  __ pop(new_target);
  __ pop(argument_count);
  __ Jump(entry);

  __ bind(&loop_bottom);
  __ Dsubu(index, index,
           Operand(Smi::FromInt(SharedFunctionInfo::kEntryLength)));
  __ Branch(&loop_top, gt, index, Operand(Smi::FromInt(1)));

  // We found neither literals nor code.
  __ jmp(&gotta_call_runtime);

  __ bind(&maybe_call_runtime);
  __ pop(closure);

  // Last possibility. Check the context free optimized code map entry.
  __ ld(entry, FieldMemOperand(map, FixedArray::kHeaderSize +
                                        SharedFunctionInfo::kSharedCodeIndex));
  __ ld(entry, FieldMemOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Store code entry in the closure.
  __ Daddu(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(&install_optimized_code_and_tailcall);

  __ bind(&try_shared);
  __ pop(new_target);
  __ pop(argument_count);
  // Is the full code valid?
  __ ld(entry, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ ld(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ lw(a5, FieldMemOperand(entry, Code::kFlagsOffset));
  __ And(a5, a5, Operand(Code::KindField::kMask));
  __ dsrl(a5, a5, Code::KindField::kShift);
  __ Branch(&gotta_call_runtime_no_stack, eq, a5, Operand(Code::BUILTIN));
  // Yes, install the full code.
  __ Daddu(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ sd(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, entry, a5);
  __ Jump(entry);

  __ bind(&gotta_call_runtime);
  __ pop(closure);
  __ pop(new_target);
  __ pop(argument_count);
  __ bind(&gotta_call_runtime_no_stack);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

void Builtins::Generate_CompileBaseline(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileBaseline);
}

void Builtins::Generate_CompileOptimized(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm,
                                 Runtime::kCompileOptimized_NotConcurrent);
}

void Builtins::Generate_CompileOptimizedConcurrent(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileOptimized_Concurrent);
}


static void GenerateMakeCodeYoungAgainCommon(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Set a0 to point to the head of the PlatformCodeAge sequence.
  __ Dsubu(a0, a0,
      Operand(kNoCodeAgeSequenceLength - Assembler::kInstrSize));

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   a0 - contains return address (beginning of patch sequence)
  //   a1 - isolate
  //   a3 - new target
  RegList saved_regs =
      (a0.bit() | a1.bit() | a3.bit() | ra.bit() | fp.bit()) & ~sp.bit();
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush(saved_regs);
  __ PrepareCallCFunction(2, 0, a2);
  __ li(a1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  __ MultiPop(saved_regs);
  __ Jump(a0);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                 \
void Builtins::Generate_Make##C##CodeYoungAgainEvenMarking(  \
    MacroAssembler* masm) {                                  \
  GenerateMakeCodeYoungAgainCommon(masm);                    \
}                                                            \
void Builtins::Generate_Make##C##CodeYoungAgainOddMarking(   \
    MacroAssembler* masm) {                                  \
  GenerateMakeCodeYoungAgainCommon(masm);                    \
}
CODE_AGE_LIST(DEFINE_CODE_AGE_BUILTIN_GENERATOR)
#undef DEFINE_CODE_AGE_BUILTIN_GENERATOR


void Builtins::Generate_MarkCodeAsExecutedOnce(MacroAssembler* masm) {
  // For now, as in GenerateMakeCodeYoungAgainCommon, we are relying on the fact
  // that make_code_young doesn't do any garbage collection which allows us to
  // save/restore the registers without worrying about which of them contain
  // pointers.

  // Set a0 to point to the head of the PlatformCodeAge sequence.
  __ Dsubu(a0, a0,
      Operand(kNoCodeAgeSequenceLength - Assembler::kInstrSize));

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   a0 - contains return address (beginning of patch sequence)
  //   a1 - isolate
  //   a3 - new target
  RegList saved_regs =
      (a0.bit() | a1.bit() | a3.bit() | ra.bit() | fp.bit()) & ~sp.bit();
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush(saved_regs);
  __ PrepareCallCFunction(2, 0, a2);
  __ li(a1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
      2);
  __ MultiPop(saved_regs);

  // Perform prologue operations usually performed by the young code stub.
  __ PushStandardFrame(a1);

  // Jump to point after the code-age stub.
  __ Daddu(a0, a0, Operand((kNoCodeAgeSequenceLength)));
  __ Jump(a0);
}


void Builtins::Generate_MarkCodeAsExecutedTwice(MacroAssembler* masm) {
  GenerateMakeCodeYoungAgainCommon(masm);
}


void Builtins::Generate_MarkCodeAsToBeExecutedOnce(MacroAssembler* masm) {
  Generate_MarkCodeAsExecutedOnce(masm);
}


static void Generate_NotifyStubFailureHelper(MacroAssembler* masm,
                                             SaveFPRegsMode save_doubles) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Preserve registers across notification, this is important for compiled
    // stubs that tail call the runtime on deopts passing their parameters in
    // registers.
    __ MultiPush(kJSCallerSaved | kCalleeSaved);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kNotifyStubFailure, save_doubles);
    __ MultiPop(kJSCallerSaved | kCalleeSaved);
  }

  __ Daddu(sp, sp, Operand(kPointerSize));  // Ignore state
  __ Jump(ra);  // Jump to miss handler
}


void Builtins::Generate_NotifyStubFailure(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kDontSaveFPRegs);
}


void Builtins::Generate_NotifyStubFailureSaveDoubles(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kSaveFPRegs);
}


static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass the function and deoptimization type to the runtime system.
    __ li(a0, Operand(Smi::FromInt(static_cast<int>(type))));
    __ push(a0);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  // Get the full codegen state from the stack and untag it -> a6.
  __ ld(a6, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(a6);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ Branch(&with_tos_register,
            ne, a6, Operand(FullCodeGenerator::NO_REGISTERS));
  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Daddu(sp, sp, Operand(1 * kPointerSize));  // Remove state.

  __ bind(&with_tos_register);
  __ ld(v0, MemOperand(sp, 1 * kPointerSize));
  __ Branch(&unknown_state, ne, a6, Operand(FullCodeGenerator::TOS_REG));

  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Daddu(sp, sp, Operand(2 * kPointerSize));  // Remove state.

  __ bind(&unknown_state);
  __ stop("no cases left");
}


void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_NotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}


void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}


// Clobbers {t2, t3, a4, a5}.
static void CompatibleReceiverCheck(MacroAssembler* masm, Register receiver,
                                    Register function_template_info,
                                    Label* receiver_check_failed) {
  Register signature = t2;
  Register map = t3;
  Register constructor = a4;
  Register scratch = a5;

  // If there is no signature, return the holder.
  __ ld(signature, FieldMemOperand(function_template_info,
                                   FunctionTemplateInfo::kSignatureOffset));
  Label receiver_check_passed;
  __ JumpIfRoot(signature, Heap::kUndefinedValueRootIndex,
                &receiver_check_passed);

  // Walk the prototype chain.
  __ ld(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  Label prototype_loop_start;
  __ bind(&prototype_loop_start);

  // Get the constructor, if any.
  __ GetMapConstructor(constructor, map, scratch, scratch);
  Label next_prototype;
  __ Branch(&next_prototype, ne, scratch, Operand(JS_FUNCTION_TYPE));
  Register type = constructor;
  __ ld(type,
        FieldMemOperand(constructor, JSFunction::kSharedFunctionInfoOffset));
  __ ld(type, FieldMemOperand(type, SharedFunctionInfo::kFunctionDataOffset));

  // Loop through the chain of inheriting function templates.
  Label function_template_loop;
  __ bind(&function_template_loop);

  // If the signatures match, we have a compatible receiver.
  __ Branch(&receiver_check_passed, eq, signature, Operand(type),
            USE_DELAY_SLOT);

  // If the current type is not a FunctionTemplateInfo, load the next prototype
  // in the chain.
  __ JumpIfSmi(type, &next_prototype);
  __ GetObjectType(type, scratch, scratch);
  __ Branch(&next_prototype, ne, scratch, Operand(FUNCTION_TEMPLATE_INFO_TYPE));

  // Otherwise load the parent function template and iterate.
  __ ld(type,
        FieldMemOperand(type, FunctionTemplateInfo::kParentTemplateOffset));
  __ Branch(&function_template_loop);

  // Load the next prototype.
  __ bind(&next_prototype);
  __ lwu(scratch, FieldMemOperand(map, Map::kBitField3Offset));
  __ DecodeField<Map::HasHiddenPrototype>(scratch);
  __ Branch(receiver_check_failed, eq, scratch, Operand(zero_reg));

  __ ld(receiver, FieldMemOperand(map, Map::kPrototypeOffset));
  __ ld(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Iterate.
  __ Branch(&prototype_loop_start);

  __ bind(&receiver_check_passed);
}


void Builtins::Generate_HandleFastApiCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                 : number of arguments excluding receiver
  //  -- a1                 : callee
  //  -- ra                 : return address
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[8 * (argc - 1)] : first argument
  //  -- sp[8 * argc]       : receiver
  // -----------------------------------

  // Load the FunctionTemplateInfo.
  __ ld(t1, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(t1, FieldMemOperand(t1, SharedFunctionInfo::kFunctionDataOffset));

  // Do the compatible receiver check
  Label receiver_check_failed;
  __ Dlsa(t8, sp, a0, kPointerSizeLog2);
  __ ld(t0, MemOperand(t8));
  CompatibleReceiverCheck(masm, t0, t1, &receiver_check_failed);

  // Get the callback offset from the FunctionTemplateInfo, and jump to the
  // beginning of the code.
  __ ld(t2, FieldMemOperand(t1, FunctionTemplateInfo::kCallCodeOffset));
  __ ld(t2, FieldMemOperand(t2, CallHandlerInfo::kFastHandlerOffset));
  __ Daddu(t2, t2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t2);

  // Compatible receiver check failed: throw an Illegal Invocation exception.
  __ bind(&receiver_check_failed);
  // Drop the arguments (including the receiver);
  __ Daddu(t8, t8, Operand(kPointerSize));
  __ daddu(sp, t8, zero_reg);
  __ TailCallRuntime(Runtime::kThrowIllegalInvocation);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ ld(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(a0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the unoptimized code.
  __ Ret(eq, v0, Operand(Smi::FromInt(0)));

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ ld(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ ld(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
      DeoptimizationInputData::kOsrPcOffsetIndex) - kHeapObjectTag));
  __ SmiUntag(a1);

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ daddu(v0, v0, a1);
  __ daddiu(ra, v0, Code::kHeaderSize - kHeapObjectTag);

  // And "return" to the OSR entry point of the function.
  __ Ret();
}


// static
void Builtins::Generate_DatePrototype_GetField(MacroAssembler* masm,
                                               int field_index) {
  // ----------- S t a t e -------------
  //  -- sp[0] : receiver
  // -----------------------------------

  // 1. Pop receiver into a0 and check that it's actually a JSDate object.
  Label receiver_not_date;
  {
    __ Pop(a0);
    __ JumpIfSmi(a0, &receiver_not_date);
    __ GetObjectType(a0, t0, t0);
    __ Branch(&receiver_not_date, ne, t0, Operand(JS_DATE_TYPE));
  }

  // 2. Load the specified date field, falling back to the runtime as necessary.
  if (field_index == JSDate::kDateValue) {
    __ Ret(USE_DELAY_SLOT);
    __ ld(v0, FieldMemOperand(a0, JSDate::kValueOffset));  // In delay slot.
  } else {
    if (field_index < JSDate::kFirstUncachedField) {
      Label stamp_mismatch;
      __ li(a1, Operand(ExternalReference::date_cache_stamp(masm->isolate())));
      __ ld(a1, MemOperand(a1));
      __ ld(t0, FieldMemOperand(a0, JSDate::kCacheStampOffset));
      __ Branch(&stamp_mismatch, ne, t0, Operand(a1));
      __ Ret(USE_DELAY_SLOT);
      __ ld(v0, FieldMemOperand(
                    a0, JSDate::kValueOffset +
                            field_index * kPointerSize));  // In delay slot.
      __ bind(&stamp_mismatch);
    }
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ PrepareCallCFunction(2, t0);
    __ li(a1, Operand(Smi::FromInt(field_index)));
    __ CallCFunction(
        ExternalReference::get_date_field_function(masm->isolate()), 2);
  }
  __ Ret();

  // 3. Raise a TypeError if the receiver is not a date.
  __ bind(&receiver_not_date);
  __ TailCallRuntime(Runtime::kThrowNotDateError);
}

// static
void Builtins::Generate_FunctionHasInstance(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argc
  //  -- sp[0] : first argument (left-hand side)
  //  -- sp[8] : receiver (right-hand side)
  // -----------------------------------

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ ld(InstanceOfDescriptor::LeftRegister(),
          MemOperand(fp, 2 * kPointerSize));  // Load left-hand side.
    __ ld(InstanceOfDescriptor::RightRegister(),
          MemOperand(fp, 3 * kPointerSize));  // Load right-hand side.
    InstanceOfStub stub(masm->isolate(), true);
    __ CallStub(&stub);
  }

  // Pop the argument and the receiver.
  __ DropAndRet(2);
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into a1, argArray into a0 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg;
    Register scratch = a4;
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ mov(a3, a2);
    // Dlsa() cannot be used hare as scratch value used later.
    __ dsll(scratch, a0, kPointerSizeLog2);
    __ Daddu(a0, sp, Operand(scratch));
    __ ld(a1, MemOperand(a0));  // receiver
    __ Dsubu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ ld(a2, MemOperand(a0));  // thisArg
    __ Dsubu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ ld(a3, MemOperand(a0));  // argArray
    __ bind(&no_arg);
    __ Daddu(sp, sp, Operand(scratch));
    __ sd(a2, MemOperand(sp));
    __ mov(a0, a3);
  }

  // ----------- S t a t e -------------
  //  -- a0    : argArray
  //  -- a1    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(a1, &receiver_not_callable);
  __ ld(a4, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(a4, FieldMemOperand(a4, Map::kBitFieldOffset));
  __ And(a4, a4, Operand(1 << Map::kIsCallable));
  __ Branch(&receiver_not_callable, eq, a4, Operand(zero_reg));

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(a0, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(a0, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ mov(a0, zero_reg);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    __ sd(a1, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}


// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // a0: actual number of arguments
  {
    Label done;
    __ Branch(&done, ne, a0, Operand(zero_reg));
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ Daddu(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack.
  // a0: actual number of arguments
  __ Dlsa(at, sp, a0, kPointerSizeLog2);
  __ ld(a1, MemOperand(at));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ Dlsa(a2, sp, a0, kPointerSizeLog2);

    __ bind(&loop);
    __ ld(at, MemOperand(a2, -kPointerSize));
    __ sd(at, MemOperand(a2));
    __ Dsubu(a2, a2, Operand(kPointerSize));
    __ Branch(&loop, ne, a2, Operand(sp));
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ Dsubu(a0, a0, Operand(1));
    __ Pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label no_arg;
    Register scratch = a4;
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ mov(a2, a1);
    __ mov(a3, a1);
    __ dsll(scratch, a0, kPointerSizeLog2);
    __ mov(a0, scratch);
    __ Dsubu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(zero_reg));
    __ Daddu(a0, sp, Operand(a0));
    __ ld(a1, MemOperand(a0));  // target
    __ Dsubu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ ld(a2, MemOperand(a0));  // thisArgument
    __ Dsubu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ ld(a3, MemOperand(a0));  // argumentsList
    __ bind(&no_arg);
    __ Daddu(sp, sp, Operand(scratch));
    __ sd(a2, MemOperand(sp));
    __ mov(a0, a3);
  }

  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a1    : target
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(a1, &target_not_callable);
  __ ld(a4, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(a4, FieldMemOperand(a4, Map::kBitFieldOffset));
  __ And(a4, a4, Operand(1 << Map::kIsCallable));
  __ Branch(&target_not_callable, eq, a4, Operand(zero_reg));

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    __ sd(a1, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}


void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // new.target into a3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label no_arg;
    Register scratch = a4;
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ mov(a2, a1);
    // Dlsa() cannot be used hare as scratch value used later.
    __ dsll(scratch, a0, kPointerSizeLog2);
    __ Daddu(a0, sp, Operand(scratch));
    __ sd(a2, MemOperand(a0));  // receiver
    __ Dsubu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ ld(a1, MemOperand(a0));  // target
    __ mov(a3, a1);             // new.target defaults to target
    __ Dsubu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ ld(a2, MemOperand(a0));  // argumentsList
    __ Dsubu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ ld(a3, MemOperand(a0));  // new.target
    __ bind(&no_arg);
    __ Daddu(sp, sp, Operand(scratch));
    __ mov(a0, a2);
  }

  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a3    : new.target
  //  -- a1    : target
  //  -- sp[0] : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(a1, &target_not_constructor);
  __ ld(a4, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(a4, FieldMemOperand(a4, Map::kBitFieldOffset));
  __ And(a4, a4, Operand(1 << Map::kIsConstructor));
  __ Branch(&target_not_constructor, eq, a4, Operand(zero_reg));

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(a3, &new_target_not_constructor);
  __ ld(a4, FieldMemOperand(a3, HeapObject::kMapOffset));
  __ lbu(a4, FieldMemOperand(a4, Map::kBitFieldOffset));
  __ And(a4, a4, Operand(1 << Map::kIsConstructor));
  __ Branch(&new_target_not_constructor, eq, a4, Operand(zero_reg));

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    __ sd(a1, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    __ sd(a3, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }
}


static void ArgumentAdaptorStackCheck(MacroAssembler* masm,
                                      Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- a0 : actual number of arguments
  //  -- a1 : function (passed through to callee)
  //  -- a2 : expected number of arguments
  //  -- a3 : new target (passed through to callee)
  // -----------------------------------
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(a5, Heap::kRealStackLimitRootIndex);
  // Make a5 the space we have left. The stack might already be overflowed
  // here which will cause a5 to become negative.
  __ dsubu(a5, sp, a5);
  // Check if the arguments will overflow the stack.
  __ dsll(at, a2, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(stack_overflow, le, a5, Operand(at));
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  // __ sll(a0, a0, kSmiTagSize);
  __ dsll32(a0, a0, 0);
  __ li(a4, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ MultiPush(a0.bit() | a1.bit() | a4.bit() | fp.bit() | ra.bit());
  __ Daddu(fp, sp,
      Operand(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ ld(a1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                             kPointerSize)));
  __ mov(sp, fp);
  __ MultiPop(fp.bit() | ra.bit());
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  // Adjust for the receiver.
  __ Daddu(sp, sp, Operand(kPointerSize));
}


// static
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a1    : target
  //  -- a3    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_runtime, done_create;
    __ JumpIfSmi(a0, &create_runtime);

    // Load the map of argumentsList into a2.
    __ ld(a2, FieldMemOperand(a0, HeapObject::kMapOffset));

    // Load native context into a4.
    __ ld(a4, NativeContextMemOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ ld(at, ContextMemOperand(a4, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ Branch(&create_arguments, eq, a2, Operand(at));
    __ ld(at, ContextMemOperand(a4, Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ Branch(&create_arguments, eq, a2, Operand(at));

    // Check if argumentsList is a fast JSArray.
    __ ld(v0, FieldMemOperand(a2, HeapObject::kMapOffset));
    __ lbu(v0, FieldMemOperand(v0, Map::kInstanceTypeOffset));
    __ Branch(&create_array, eq, v0, Operand(JS_ARRAY_TYPE));

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(a1, a3, a0);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ mov(a0, v0);
      __ Pop(a1, a3);
      __ ld(a2, FieldMemOperand(v0, FixedArray::kLengthOffset));
      __ SmiUntag(a2);
    }
    __ Branch(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ ld(a2, FieldMemOperand(a0, JSArgumentsObject::kLengthOffset));
    __ ld(a4, FieldMemOperand(a0, JSObject::kElementsOffset));
    __ ld(at, FieldMemOperand(a4, FixedArray::kLengthOffset));
    __ Branch(&create_runtime, ne, a2, Operand(at));
    __ SmiUntag(a2);
    __ mov(a0, a4);
    __ Branch(&done_create);

    // Try to create the list from a JSArray object.
    __ bind(&create_array);
    __ ld(a2, FieldMemOperand(a2, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(a2);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    __ Branch(&create_runtime, hi, a2, Operand(FAST_ELEMENTS));
    __ Branch(&create_runtime, eq, a2, Operand(FAST_HOLEY_SMI_ELEMENTS));
    __ ld(a2, FieldMemOperand(a0, JSArray::kLengthOffset));
    __ ld(a0, FieldMemOperand(a0, JSArray::kElementsOffset));
    __ SmiUntag(a2);

    __ bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(a4, Heap::kRealStackLimitRootIndex);
    // Make ip the space we have left. The stack might already be overflowed
    // here which will cause ip to become negative.
    __ Dsubu(a4, sp, a4);
    // Check if the arguments will overflow the stack.
    __ dsll(at, a2, kPointerSizeLog2);
    __ Branch(&done, gt, a4, Operand(at));  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- a1    : target
  //  -- a0    : args (a FixedArray built from argumentsList)
  //  -- a2    : len (number of elements to push from args)
  //  -- a3    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ mov(a4, zero_reg);
    Label done, loop;
    __ bind(&loop);
    __ Branch(&done, eq, a4, Operand(a2));
    __ Dlsa(at, a0, a4, kPointerSizeLog2);
    __ ld(at, FieldMemOperand(at, FixedArray::kHeaderSize));
    __ Push(at);
    __ Daddu(a4, a4, Operand(1));
    __ Branch(&loop);
    __ bind(&done);
    __ Move(a0, a4);
  }

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    Label construct;
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Branch(&construct, ne, a3, Operand(at));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
    __ bind(&construct);
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }
}

namespace {

// Drops top JavaScript frame and an arguments adaptor frame below it (if
// present) preserving all the arguments prepared for current call.
// Does nothing if debugger is currently active.
// ES6 14.6.3. PrepareForTailCall
//
// Stack structure for the function g() tail calling f():
//
// ------- Caller frame: -------
// |  ...
// |  g()'s arg M
// |  ...
// |  g()'s arg 1
// |  g()'s receiver arg
// |  g()'s caller pc
// ------- g()'s frame: -------
// |  g()'s caller fp      <- fp
// |  g()'s context
// |  function pointer: g
// |  -------------------------
// |  ...
// |  ...
// |  f()'s arg N
// |  ...
// |  f()'s arg 1
// |  f()'s receiver arg   <- sp (f()'s caller pc is not on the stack yet!)
// ----------------------
//
void PrepareForTailCall(MacroAssembler* masm, Register args_reg,
                        Register scratch1, Register scratch2,
                        Register scratch3) {
  DCHECK(!AreAliased(args_reg, scratch1, scratch2, scratch3));
  Comment cmnt(masm, "[ PrepareForTailCall");

  // Prepare for tail call only if ES2015 tail call elimination is enabled.
  Label done;
  ExternalReference is_tail_call_elimination_enabled =
      ExternalReference::is_tail_call_elimination_enabled_address(
          masm->isolate());
  __ li(at, Operand(is_tail_call_elimination_enabled));
  __ lb(scratch1, MemOperand(at));
  __ Branch(&done, eq, scratch1, Operand(zero_reg));

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ ld(scratch3,
          MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ Branch(&no_interpreter_frame, ne, scratch3,
              Operand(Smi::FromInt(StackFrame::STUB)));
    __ ld(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ ld(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ld(scratch3,
        MemOperand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&no_arguments_adaptor, ne, scratch3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mov(fp, scratch2);
  __ ld(caller_args_count_reg,
        MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ Branch(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ ld(scratch1,
        MemOperand(fp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  __ ld(scratch1,
        FieldMemOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(caller_args_count_reg,
        FieldMemOperand(scratch1,
                        SharedFunctionInfo::kFormalParameterCountOffset));

  __ bind(&formal_parameter_count_loaded);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3);
  __ bind(&done);
}
}  // namespace

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode,
                                     TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(a1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that function is not a "classConstructor".
  Label class_constructor;
  __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kFunctionKindByteOffset));
  __ And(at, a3, Operand(SharedFunctionInfo::kClassConstructorBitsWithinByte));
  __ Branch(&class_constructor, ne, at, Operand(zero_reg));

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kNativeByteOffset));
  __ And(at, a3, Operand((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                         (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ Branch(&done_convert, ne, at, Operand(zero_reg));
  {
    // ----------- S t a t e -------------
    //  -- a0 : the number of arguments (not including the receiver)
    //  -- a1 : the function to call (checked to be a JSFunction)
    //  -- a2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(a3);
    } else {
      Label convert_to_object, convert_receiver;
      __ Dlsa(at, sp, a0, kPointerSizeLog2);
      __ ld(a3, MemOperand(at));
      __ JumpIfSmi(a3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ GetObjectType(a3, a4, a4);
      __ Branch(&done_convert, hs, a4, Operand(FIRST_JS_RECEIVER_TYPE));
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(a3, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(a3, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(a3);
        }
        __ Branch(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(a0);
        __ Push(a0, a1);
        __ mov(a0, a3);
        ToObjectStub stub(masm->isolate());
        __ CallStub(&stub);
        __ mov(a3, v0);
        __ Pop(a0, a1);
        __ SmiUntag(a0);
      }
      __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ Dlsa(at, sp, a0, kPointerSizeLog2);
    __ sd(a3, MemOperand(at));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  //  -- a2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, a0, t0, t1, t2);
  }

  __ lw(a2,
        FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(a0);
  ParameterCount expected(a2);
  __ InvokeFunctionCode(a1, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}


// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, a0, t0, t1, t2);
  }

  // Patch the receiver to [[BoundThis]].
  {
    __ ld(at, FieldMemOperand(a1, JSBoundFunction::kBoundThisOffset));
    __ Dlsa(a4, sp, a0, kPointerSizeLog2);
    __ sd(at, MemOperand(a4));
  }

  // Load [[BoundArguments]] into a2 and length of that into a4.
  __ ld(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ ld(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ SmiUntag(a4);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a4 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ dsll(a5, a4, kPointerSizeLog2);
    __ Dsubu(sp, sp, Operand(a5));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
    __ Branch(&done, gt, sp, Operand(at));  // Signed comparison.
    // Restore the stack pointer.
    __ Daddu(sp, sp, Operand(a5));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowStackOverflow);
    }
    __ bind(&done);
  }

  // Relocate arguments down the stack.
  {
    Label loop, done_loop;
    __ mov(a5, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, gt, a5, Operand(a0));
    __ Dlsa(a6, sp, a4, kPointerSizeLog2);
    __ ld(at, MemOperand(a6));
    __ Dlsa(a6, sp, a5, kPointerSizeLog2);
    __ sd(at, MemOperand(a6));
    __ Daddu(a4, a4, Operand(1));
    __ Daddu(a5, a5, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ ld(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ SmiUntag(a4);
    __ Daddu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Dsubu(a4, a4, Operand(1));
    __ Branch(&done_loop, lt, a4, Operand(zero_reg));
    __ Dlsa(a5, a2, a4, kPointerSizeLog2);
    __ ld(at, MemOperand(a5));
    __ Dlsa(a5, sp, a0, kPointerSizeLog2);
    __ sd(at, MemOperand(a5));
    __ Daddu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ ld(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ li(at, Operand(ExternalReference(Builtins::kCall_ReceiverIsAny,
                                      masm->isolate())));
  __ ld(at, MemOperand(at));
  __ Daddu(at, at, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(a1, &non_callable);
  __ bind(&non_smi);
  __ GetObjectType(a1, t1, t2);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Check if target has a [[Call]] internal method.
  __ lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t1, t1, Operand(1 << Map::kIsCallable));
  __ Branch(&non_callable, eq, t1, Operand(zero_reg));

  __ Branch(&non_function, ne, t2, Operand(JS_PROXY_TYPE));

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, a0, t0, t1, t2);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ Push(a1);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ Daddu(a0, a0, 2);
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ Dlsa(at, sp, a0, kPointerSizeLog2);
  __ sd(a1, MemOperand(at));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, a1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}


void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(a1);

  // Calling convention for function specific ConstructStubs require
  // a2 to contain either an AllocationSite or undefined.
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(a4, FieldMemOperand(a4, SharedFunctionInfo::kConstructStubOffset));
  __ Daddu(at, a4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  // Load [[BoundArguments]] into a2 and length of that into a4.
  __ ld(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ ld(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ SmiUntag(a4);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a3 : the new target (checked to be a constructor)
  //  -- a4 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ dsll(a5, a4, kPointerSizeLog2);
    __ Dsubu(sp, sp, Operand(a5));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
    __ Branch(&done, gt, sp, Operand(at));  // Signed comparison.
    // Restore the stack pointer.
    __ Daddu(sp, sp, Operand(a5));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowStackOverflow);
    }
    __ bind(&done);
  }

  // Relocate arguments down the stack.
  {
    Label loop, done_loop;
    __ mov(a5, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, ge, a5, Operand(a0));
    __ Dlsa(a6, sp, a4, kPointerSizeLog2);
    __ ld(at, MemOperand(a6));
    __ Dlsa(a6, sp, a5, kPointerSizeLog2);
    __ sd(at, MemOperand(a6));
    __ Daddu(a4, a4, Operand(1));
    __ Daddu(a5, a5, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ ld(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ SmiUntag(a4);
    __ Daddu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Dsubu(a4, a4, Operand(1));
    __ Branch(&done_loop, lt, a4, Operand(zero_reg));
    __ Dlsa(a5, a2, a4, kPointerSizeLog2);
    __ ld(at, MemOperand(a5));
    __ Dlsa(a5, sp, a0, kPointerSizeLog2);
    __ sd(at, MemOperand(a5));
    __ Daddu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label skip_load;
    __ Branch(&skip_load, ne, a1, Operand(a3));
    __ ld(a3, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&skip_load);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ ld(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ li(at, Operand(ExternalReference(Builtins::kConstruct, masm->isolate())));
  __ ld(at, MemOperand(at));
  __ Daddu(at, at, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSProxy)
  //  -- a3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ Push(a1, a3);
  // Include the pushed new_target, constructor and the receiver.
  __ Daddu(a0, a0, Operand(3));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
}


// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (can be any Object)
  //  -- a3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(a1, &non_constructor);

  // Dispatch based on instance type.
  __ ld(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(t2, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));

  // Check if target has a [[Construct]] internal method.
  __ lbu(t3, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t3, t3, Operand(1 << Map::kIsConstructor));
  __ Branch(&non_constructor, eq, t3, Operand(zero_reg));

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Jump(masm->isolate()->builtins()->ConstructBoundFunction(),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Only dispatch to proxies after checking whether they are constructors.
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq, t2, Operand(JS_PROXY_TYPE));

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ Dlsa(at, sp, a0, kPointerSizeLog2);
    __ sd(a1, MemOperand(at));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, a1);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(masm->isolate()->builtins()->ConstructedNonConstructable(),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_AllocateInNewSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : requested object size (untagged)
  //  -- ra : return address
  // -----------------------------------
  Label runtime;
  __ Allocate(a0, v0, a1, a2, &runtime, NO_ALLOCATION_FLAGS);
  __ Ret();

  __ bind(&runtime);
  __ SmiTag(a0);
  __ Push(a0);
  __ Move(cp, Smi::FromInt(0));
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : requested object size (untagged)
  //  -- ra : return address
  // -----------------------------------
  Label runtime;
  __ Allocate(a0, v0, a1, a2, &runtime, PRETENURE);
  __ Ret();

  __ bind(&runtime);
  __ SmiTag(a0);
  __ Move(a1, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(a0, a1);
  __ Move(cp, Smi::FromInt(0));
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // State setup as expected by MacroAssembler::InvokePrologue.
  // ----------- S t a t e -------------
  //  -- a0: actual arguments count
  //  -- a1: function (passed through to callee)
  //  -- a2: expected arguments count
  //  -- a3: new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ Branch(&dont_adapt_arguments, eq,
      a2, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  // We use Uless as the number of argument should always be greater than 0.
  __ Branch(&too_few, Uless, a0, Operand(a2));

  {  // Enough parameters: actual >= expected.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    ArgumentAdaptorStackCheck(masm, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into a4.
    __ SmiScale(a0, a0, kPointerSizeLog2);
    __ Daddu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Daddu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address.
    __ dsll(a4, a2, kPointerSizeLog2);
    __ dsubu(a4, a0, a4);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // a4: copy end address

    Label copy;
    __ bind(&copy);
    __ ld(a5, MemOperand(a0));
    __ push(a5);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a4));
    __ daddiu(a0, a0, -kPointerSize);  // In delay slot.

    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    ArgumentAdaptorStackCheck(masm, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into a7.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ SmiScale(a0, a0, kPointerSizeLog2);
    __ Daddu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Daddu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address. Also adjust for return address.
    __ Daddu(a7, fp, kPointerSize);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // a7: copy end address
    Label copy;
    __ bind(&copy);
    __ ld(a4, MemOperand(a0));  // Adjusted above for return addr and receiver.
    __ Dsubu(sp, sp, kPointerSize);
    __ Dsubu(a0, a0, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a7));
    __ sd(a4, MemOperand(sp));  // In the delay slot.

    // Fill the remaining expected arguments with undefined.
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ LoadRoot(a5, Heap::kUndefinedValueRootIndex);
    __ dsll(a6, a2, kPointerSizeLog2);
    __ Dsubu(a4, fp, Operand(a6));
    // Adjust for frame.
    __ Dsubu(a4, a4, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                             2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ Dsubu(sp, sp, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &fill, ne, sp, Operand(a4));
    __ sd(a5, MemOperand(sp));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(a0, a2);
  // a0 : expected number of arguments
  // a1 : function (passed through to callee)
  // a3: new target (passed through to callee)
  __ ld(a4, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ Call(a4);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();


  // -------------------------------------------
  // Don't adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ ld(a4, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ Jump(a4);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);
  }
}


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64
