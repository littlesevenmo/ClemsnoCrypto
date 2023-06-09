#ifndef UTILS_H
#define UTILS_H
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm_basics.h"
#include <vector>

struct Globals {
    static std::map<Value *, std::size_t> ValueUidMap;
    static std::set<Function *> DirFuncs;
    static std::string ExportLabel;
    static raw_fd_ostream *ApisReport;
    static raw_fd_ostream *TaintReport;
    static double Threshold;
    static bool IsLib;
    static std::set<std::string> Hotspots;
    static std::set<std::string> SkipFuncs;
};

void initValueUid(Module &M, std::map<Value *, std::size_t> &valueUidMap);

void getSCCTraversalOrder(Function &currF, std::vector<std::vector<BasicBlock*> > &bbTraversalList);


size_t getNumTimesToAnalyze(std::vector<BasicBlock*> &currSCC);


// Parser utilities


inline std::pair<size_t, bool> extractConstantInt(Value *val) {
    auto ci = dyn_cast<ConstantInt>(val);
    if (ci) return std::make_pair(ci->getZExtValue(), true);
    return std::make_pair(0, false);
}


size_t getGEPOffset(GetElementPtrInst &I, const DataLayout &dl);


size_t getGEPOffset(ConstantExpr *cexpr, const DataLayout &dl, LLVMContext &ctx);


struct InitializerWalker {
    struct GlobalPointsTo {
        GlobalObject *target;
        size_t srcoff, dstoff;

        GlobalPointsTo(GlobalObject *t, size_t s, size_t d)
            : target(t), srcoff(s), dstoff(d) { }
    };

    std::vector<GlobalPointsTo> results;
    const DataLayout &layout;
    LLVMContext &ctx;

    InitializerWalker(const DataLayout &dl, LLVMContext &c)
        : layout(dl), ctx(c) { }

    void scan(Constant* init) {
        results.clear();
        scan(init, 0);
    }

    void scan(Constant* init, size_t off);

    void handleNonAgg(Constant* init, size_t off);
};


// Transform utilities


void splitConstExpr(Module &M);


void insertWRPKRU(Module &M, Instruction *I, int perm_val, Value *dep=nullptr);


CallInst* replaceAllocaWithMPKMalloc(Module &M, AllocaInst *I);


CallInst* insertMPKFree(Module &M, Value *target, Instruction *insertbefore);
CallInst* insertMemset(Module &M, Value *target, Instruction *insertbefore);

void replaceRuntime(Module &M);

struct ExpandFuncPtr {
    std::vector<Value*> args;
    CallInst *originst;
    Value *funcptr;
    FunctionType *fptype;
    Function *F;
    Module *M;

    BasicBlock *curblock, *tailblock;
    std::vector<Instruction*> retvals;

    ExpandFuncPtr(CallInst *inst);

    void splitBlock();

    CallInst* addTarget(Function *target);

    CallInst* addFallback();

    PHINode* addPHINode();
};

Function *funcwrap(Function *target, std::string wrapper_name, CallInst *callinst, bool);

struct DbgInfo {
    static std::map<std::size_t, Value *> DbgUidValueMap;
    static Module *DbgM;

    static void load(std::string& dbgbc) {
        SMDiagnostic Err;

        LLVMContext *LLVMCtx = new LLVMContext();
        DbgM = parseIRFile(dbgbc, Err, *LLVMCtx).release();
        std::size_t cnt = 0xdeadbeef00000000;

        splitConstExpr(*DbgM);
        for (auto &F : *DbgM) {
            DbgUidValueMap[cnt] = &F;
            cnt++;
            for (auto &BB : F) {
                DbgUidValueMap[cnt] = &BB;
                cnt++;
                for (auto &II : BB) {
                    if (isa<DbgInfoIntrinsic>(II)) continue;
                    DbgUidValueMap[cnt] = &II;
                    cnt++;
                }
            }
        }
    }

    static int getSrcLine(Instruction *I) {
        auto DI =  dyn_cast<Instruction>(DbgUidValueMap[Globals::ValueUidMap[I]]);
        const DebugLoc &currDC = DI->getDebugLoc();
        if (currDC) {
            return currDC.getLine();
        }
        return -1;
    }

    static std::string getSrcFileName(Instruction *I) {
        auto DI =  dyn_cast<Instruction>(DbgUidValueMap[Globals::ValueUidMap[I]]);
        const DebugLoc &currDC = DI->getDebugLoc();
        if (currDC) {
            auto *Scope = cast<DIScope>(currDC->getScope());
            return (Scope->getDirectory()+"/"+Scope->getFilename()).str();
        }
        return std::string("");
    }

};


#endif  // UTILS_H
