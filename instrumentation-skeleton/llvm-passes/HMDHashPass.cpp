#define DEBUG_TYPE "HMDHashPass"

#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/GlobalObject.h>
#include <llvm/ADT/StringRef.h>
#include "builtin/Common.h"
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

//#define SENTINEL ((void*) 0)

namespace {
    class DlibTestPass : public ModulePass {
    public:
        static char ID;
      	DlibTestPass() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) override;

    private:
        bool instrumentFunction(Module &M, Function &F);

        bool shouldInstrumentGlobal(GlobalVariable *G);
    };
}

// Not all global variables should be instrumented, i.e., variables not created by the program.
bool shouldInstrumentGlobal(GlobalVariable *G){
    Type *globalType = G->getValueType();

    if(globalType->isSized() || G->hasInitializer()){
        return false;
    }

    if(G->getLinkage() != GlobalVariable::ExternalLinkage && G->getLinkage() != GlobalVariable::InternalLinkage
        && G->getLinkage() != GlobalVariable::PrivateLinkage){
        return false;
    }

    if(G->isThreadLocal()){
        return false;
    }

    return true;
}

bool DlibTestPass::instrumentFunction(Module &M, Function &F) {
    // TO-DO:
    // - For every alloca, put the allocated variable/array/struct inside another struct
    //  with two arrays inside it as well (front and rear red-zones). Make sure the new struct
    //  is called whenever the original variable/array is used.
    // - For every use of the allocated array (variable), change the use to the pointer inside the new struct.
    // - Loads and stores are instrumented already to be checked, however can we ensure that
    //  some loads/stores are not instrumented (e.g., for some calling/return variables, etc)?

    bool IRModify = false;

    //For stack instrumentation:
    //Type* Int8Ty = IntegerType::getInt8Ty(M.getContext());
    //auto num = 128;
    //ArrayType *redZoneType = ArrayType::get(Int8Ty, num);
    
    LLVMContext &C = M.getContext();
    Type *Int32Ty = Type::getInt32Ty(C);
    Type *VoidPtrTy = Type::getInt8Ty(C)->getPointerTo();
 /*   Type *VoidTy = Type::getVoidTy(C);
    Type *Int64Ty = Type::getInt64Ty(C); */

    // Does a fast check for the magic pattern, or a slow check in the shadow memory/hash table. 
//    Function *checkAccessFunc = cast<Function>(M.getOrInsertFunction("checkMemoryAccess", Int32Ty, VoidPtrTy, Int32Ty, SENTINEL));
	Function *checkAccessFunc = getNoInstrumentFunction(M, "checkMemoryAccess");

    // All dynamic memory allocation wrapper functions. Might need something for posix_memalign
    // and potentially alloca.
    //Function *newMalloc = cast<Function>(M.getOrInsertFunction("Dlib_malloc", VoidPtrTy, Int64Ty, SENTINEL));
	Function *newMalloc = getNoInstrumentFunction(M, "Dlib_malloc");
    Function *newRealloc = getNoInstrumentFunction(M, "Dlib_realloc");
    Function *newCalloc = getNoInstrumentFunction(M, "Dlib_calloc");
    Function *newMemalign = getNoInstrumentFunction(M, "Dlib_memalign");
    
    // The wrapped free function.
    Function *newFree = getNoInstrumentFunction(M, "Dlib_free");

    DataLayout DL = M.getDataLayout();

    SmallVector<Instruction*, 16> WorkList;

    for(Instruction &I : instructions(F)){
        if(isa<LoadInst>(&I) || isa<StoreInst>(&I)){
            WorkList.push_back(&I);
        }

        // Do something special if calls to either these functions are called in
        // library functions, or something?
        if(CallInst *CI = dyn_cast<CallInst>(&I)){
            Function *func = CI->getCalledFunction();
            if(func){
                if(func->getName().startswith("malloc")){
                    WorkList.push_back(CI);
                }else if(func->getName().startswith("realloc")){
                    WorkList.push_back(CI);
                }else if(func->getName().startswith("calloc")){
                    WorkList.push_back(CI);
                }else if(func->getName().startswith("free")){
                    WorkList.push_back(CI);
                }else if(func->getName().startswith("memalign")){
                    WorkList.push_back(CI);
                }
            }else{
                // Indirect call. Do nothing?
            }
        }
    }

    IRBuilder<> B(F.getContext());
    for(Instruction *I : WorkList){
        B.SetInsertPoint(I);

        if(LoadInst *LI = dyn_cast<LoadInst>(I)){
            IRBuilder<> B(LI);

            Value *ptr = B.CreateBitCast(LI->getPointerOperand(), VoidPtrTy);
	    Value *newint = ConstantInt::get(Int32Ty, DL.getTypeStoreSize(LI->getType()));

            B.CreateCall(checkAccessFunc, {ptr, newint});
        }else if(StoreInst *SI = dyn_cast<StoreInst>(I)){
            IRBuilder<> B(SI);

            Value *ptr = B.CreateBitCast(SI->getPointerOperand(), VoidPtrTy);
	    Value *newint = ConstantInt::get(Int32Ty, DL.getTypeStoreSize(SI->getOperand(0)->getType()));

            B.CreateCall(checkAccessFunc, {ptr, newint});
        }else if(CallInst* CI = dyn_cast<CallInst>(I)){
            Function *func = CI->getCalledFunction();

            CallSite CS(CI);
            SmallVector<Value*, 8> arguments (CS.arg_begin(), CS.arg_end());

            if(func->getName() == "malloc"){
                CallInst *MI = CallInst::Create(newMalloc, arguments);
                MI->setCallingConv(newMalloc->getCallingConv());

                if(!CI->use_empty()){
                    CI->replaceAllUsesWith(MI);
                }

                ReplaceInstWithInst(CI, MI);
            }else if(func->getName() == "realloc"){
                CallInst *MI = CallInst::Create(newRealloc, arguments);
                MI->setCallingConv(newRealloc->getCallingConv());

                if(!CI->use_empty()){
                    CI->replaceAllUsesWith(MI);
                }

                ReplaceInstWithInst(CI, MI);
            }else if(func->getName() == "calloc"){
                CallInst *MI = CallInst::Create(newCalloc, arguments);
                MI->setCallingConv(newCalloc->getCallingConv());

                if(!CI->use_empty()){
                    CI->replaceAllUsesWith(MI);
                }

                ReplaceInstWithInst(CI, MI);
            }else if(func->getName() =="free"){
	//	LOG_LINE("Original call started with free, *CI is: " << *CI);

                CallInst *MI = CallInst::Create(newFree, arguments);
                MI->setCallingConv(newFree->getCallingConv());

                if(!CI->use_empty()){
                    CI->replaceAllUsesWith(MI);
                }

                ReplaceInstWithInst(CI, MI);
            }else if(func->getName() == "memalign"){
                CallInst *MI = CallInst::Create(newMemalign, arguments);
                MI->setCallingConv(newMemalign->getCallingConv());

                if(!CI->use_empty()){
                    CI->replaceAllUsesWith(MI);
                }

                ReplaceInstWithInst(CI, MI);
            }
        }
    }

    return IRModify;
}

/*
    Type* Int8Ty = IntegerType::getInt8Ty(M.getContext());
    auto num = 32;

    ArrayType* arraytype = ArrayType::get(Int8Ty, num);

    LOG_LINE("Inserting...");
    AllocaInst* ai = new AllocaInst(arraytype, 0, "x", AI);
    LOG_LINE("Insertion successful.");

    LOG_LINE("Done: ") << ai->getName();

*/

/*

// Actually use function for this, but this does not work for now.
    Value *size = B.getInt64((M.getDataLayout()).getTypeAllocSize(AI->getAllocatedType()));

    if(AI->isArrayAllocation()){
        size = B.CreateMul(size, AI->getArraySize());
    }

    // Concern: is the size correct now, if used as a Value? When converted to a constant,
    // the size is about 4x as big (because of type i32 size).

    if(size == NULL){
        LOG_LINE("Failed to retrieve array size!");

        continue;         
    }

*/

/*   for (BasicBlock &BB : F){
        for(BasicBlock::iterator i = BB.begin(), end = BB.end(); i != end;){
            if(Instruction *I = dyn_cast<Instruction>(i++)){
                LOG_LINE("Start: ") << I->getName();

               // if(AllocaInst *AI = dyn_cast<AllocaInst>(I)){
                 //   StructType *structNew = StructType::create(M.getContext(), "new_struct");

                  //  Type* allocType = AI->getAllocatedType();
                 //   structNew->setBody({redZoneType, allocType, redZoneType}, false);

                //    new AllocaInst(structNew, 0, "struct_new", AI);

                    //AI->replaceAllUsesWith(pointerTo);

                    // AI->eraseFromParent();

                    // Insert function call to redZonePattern setting function with
                    // reference to both red-zones in new struct.

                    // Insert function call, to record the left and 
                    // right red-zone with the addRZAddr();

                    // At the end of the function, insert function to remove red-zones from
                    // red-zone register.

               // }

                if(LoadInst *LI = dyn_cast<LoadInst>(I)){
                    // Check if load address is addressable or not.
                    IRBuilder<> B(LI);
                    Value *ptr = B.CreateBitCast(LI->getPointerOperand(), voidPtrTy);
                    //Value *size = voidPtrTy->get
                    auto *size = 8;
                    //CallInst::Create(checkAccessFunc, loadAddr)->insertBefore(LI);
                    //Value *castAddr = B.CreatePtrToInt(loadAddr, VoidPtrTy, "");
                    B.CreateCall(checkAccessFUnc, {ptr, size});

                    IRModify = true;
                }else if(StoreInst *SI = dyn_cast<StoreInst>(I)){
                    // Check if the store address is addressable or not.
                    Value *storeAddr = SI->getOperand(1);
                    //CallInst::Create(checkAccessFunc, storeAddr)->insertBefore(SI);
                    //Value *castAddr = B.CreatePtrToInt(storeAddr, VoidPtrTy, "");
                    B.CreateCall(checkAccessFUnc, {ptr, size});

                    IRModify = true;
                } 
            }
        }
    }   */

bool DlibTestPass::runOnModule(Module &M) {
    bool IRModify = false;

    for (Function &F : M) {
        if (!F.isDeclaration() && !isNoInstrument(&F)){
            IRModify |= instrumentFunction(M, F);
        }
    }

    return IRModify;
}

char DlibTestPass::ID = 0;
static RegisterPass<DlibTestPass> X("hmboundsdhashpass", "A memory bounds checking pass used by the HM-BoundsChecking and DHash frameworks.");
