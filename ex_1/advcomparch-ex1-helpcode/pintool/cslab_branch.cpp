#include "pin.H"

#include <iostream>
#include <fstream>
#include <cassert>

using namespace std;

#include "branch_predictor.h"
#include "pentium_m_predictor/pentium_m_branch_predictor.h"
#include "ras.h"

/* ===================================================================== */
/* Commandline Switches                                                  */
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,    "pintool",
    "o", "cslab_branch.out", "specify output file name");
/* ===================================================================== */

/* ===================================================================== */
/* Global Variables                                                      */
/* ===================================================================== */
std::vector<BranchPredictor *> branch_predictors;
typedef std::vector<BranchPredictor *>::iterator bp_iterator_t;

//> BTBs have slightly different interface (they also have target predictions)
//  so we need to have different vector for them.
std::vector<BTBPredictor *> btb_predictors;
typedef std::vector<BTBPredictor *>::iterator btb_iterator_t;

std::vector<RAS *> ras_vec;
typedef std::vector<RAS *>::iterator ras_vec_iterator_t;

UINT64 total_instructions;
std::ofstream outFile;

/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool simulates various branch predictors.\n\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

/* ===================================================================== */

VOID count_instruction()
{
    total_instructions++;
}

VOID call_instruction(ADDRINT ip, ADDRINT target, UINT32 ins_size)
{
    ras_vec_iterator_t ras_it;

    for (ras_it = ras_vec.begin(); ras_it != ras_vec.end(); ++ras_it) {
        RAS *ras = *ras_it;
        ras->push_addr(ip + ins_size);
    }
}

VOID ret_instruction(ADDRINT ip, ADDRINT target)
{
    ras_vec_iterator_t ras_it;

    for (ras_it = ras_vec.begin(); ras_it != ras_vec.end(); ++ras_it) {
        RAS *ras = *ras_it;
        ras->pop_addr(target);
    }
}

VOID cond_branch_instruction(ADDRINT ip, ADDRINT target, BOOL taken)
{
    bp_iterator_t bp_it;
    BOOL pred;

    for (bp_it = branch_predictors.begin(); bp_it != branch_predictors.end(); ++bp_it) {
        BranchPredictor *curr_predictor = *bp_it;
        pred = curr_predictor->predict(ip, target);
        curr_predictor->update(pred, taken, ip, target);
    }
}

VOID branch_instruction(ADDRINT ip, ADDRINT target, BOOL taken)
{
    btb_iterator_t btb_it;
    BOOL pred;

    for (btb_it = btb_predictors.begin(); btb_it != btb_predictors.end(); ++btb_it) {
        BTBPredictor *curr_predictor = *btb_it;
        pred = curr_predictor->predict(ip, target);
        curr_predictor->update(pred, taken, ip, target);
    }
}

VOID Instruction(INS ins, void * v)
{
    if (INS_Category(ins) == XED_CATEGORY_COND_BR)
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)cond_branch_instruction,
                       IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN,
                       IARG_END);
    else if (INS_IsCall(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)call_instruction,
                       IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR,
                       IARG_UINT32, INS_Size(ins), IARG_END);
    else if (INS_IsRet(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ret_instruction,
                       IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);

    // For BTB we instrument all branches except returns
    if (INS_IsBranch(ins) && !INS_IsRet(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)branch_instruction,
                   IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN,
                   IARG_END);

    // Count each and every instruction
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_instruction, IARG_END);
}

/* ===================================================================== */

VOID Fini(int code, VOID * v)
{
    bp_iterator_t bp_it;
    btb_iterator_t btb_it;
    ras_vec_iterator_t ras_it;

    // Report total instructions and total cycles
    outFile << "Total Instructions: " << total_instructions << "\n";
    outFile << "\n";

    outFile <<"RAS: (Correct - Incorrect)\n";
    for (ras_it = ras_vec.begin(); ras_it != ras_vec.end(); ++ras_it) {
        RAS *ras = *ras_it;
        outFile << ras->getNameAndStats() << "\n";
    }
    outFile << "\n";

    outFile <<"Branch Predictors: (Name - Correct - Incorrect)\n";
    for (bp_it = branch_predictors.begin(); bp_it != branch_predictors.end(); ++bp_it) {
        BranchPredictor *curr_predictor = *bp_it;
        outFile << "  " << curr_predictor->getName() << ": "
                << curr_predictor->getNumCorrectPredictions() << " "
                << curr_predictor->getNumIncorrectPredictions() << "\n";
    }
    outFile << "\n";

    outFile <<"BTB Predictors: (Name - Correct - Incorrect - TargetCorrect)\n";
    for (btb_it = btb_predictors.begin(); btb_it != btb_predictors.end(); ++btb_it) {
        BTBPredictor *curr_predictor = *btb_it;
        outFile << "  " << curr_predictor->getName() << ": "
                << curr_predictor->getNumCorrectPredictions() << " "
                << curr_predictor->getNumIncorrectPredictions() << " "
                << curr_predictor->getNumCorrectTargetPredictions() << "\n";
    }

    outFile.close();
}

/* ===================================================================== */

VOID InitPredictors()
{
    // N-bit predictors
    
//    // 16K
//    for (int i=1; i <= 4; i++) {
//        NbitPredictor *nbitPred = new NbitPredictor(14, i);
//        branch_predictors.push_back(nbitPred);
//    }
//
//    NbitPredictor *nbitPred = new NbitPredictor(14, 2);
//    branch_predictors.push_back(nbitPred);

    // Alternative FSMs
//    TwobitPredictor_FSM1 *nbitPredFSM1 = new TwobitPredictor_FSM1();
//    branch_predictors.push_back(nbitPredFSM1);
//    TwobitPredictor_FSM2 *nbitPredFSM2 = new TwobitPredictor_FSM2();
//    branch_predictors.push_back(nbitPredFSM2);
//    TwobitPredictor_FSM3 *nbitPredFSM3 = new TwobitPredictor_FSM3();
//    branch_predictors.push_back(nbitPredFSM3);
//    TwobitPredictor_FSM4 *nbitPredFSM4 = new TwobitPredictor_FSM4();
//    branch_predictors.push_back(nbitPredFSM4);
//    TwobitPredictor_FSM5 *nbitPredFSM5 = new TwobitPredictor_FSM5();
//    branch_predictors.push_back(nbitPredFSM5);
//
//    // 32K hardware
//    NbitPredictor *onebitPred = new NbitPredictor(15, 1);
//    branch_predictors.push_back(onebitPred);
//    NbitPredictor *fourbitPred = new NbitPredictor(13, 4);
//    branch_predictors.push_back(fourbitPred);    
//    // Pentium-M predictor

// 5_6
branch_predictors.push_back(new AlwaysTakenPredictor());
    
    // 2) BTFNT
    branch_predictors.push_back(new BTFNTPredictor());
    
    // 3) n-bit predictor
    NbitPredictor *fourbitPred = new NbitPredictor(13, 4);
    branch_predictors.push_back(fourbitPred);
    // 4) Pentium-M 
    branch_predictors.push_back(new PentiumMBranchPredictor());    

    // 5, 6, 7) Local History Two Level
    branch_predictors.push_back(new LocalHistoryPredictor(11, 8));
    branch_predictors.push_back(new LocalHistoryPredictor(12, 4));
    branch_predictors.push_back(new LocalHistoryPredictor(13, 2));

    // 8, 9) Global History Two Level
    branch_predictors.push_back(new GlobalHistoryPredictor(14, 2));
    branch_predictors.push_back(new GlobalHistoryPredictor(13, 4));
    
    // 10) Alpha21264
    branch_predictors.push_back(new Alpha21264());
    
    // 11, ..., 16) Tournament Hybrid Predictors
    branch_predictors.push_back(new TournamentHybridPredictor(10, 
	new NbitPredictor(13, 2), // 8K entries, 2bit predictor
	new NbitPredictor(12, 4)  // 4K entries, 4bit predictor
    ));
    branch_predictors.push_back(new TournamentHybridPredictor(11,
	new NbitPredictor(13, 2), // 8K entries, 2bit predictor
	new GlobalHistoryPredictor(13, 2) // 8K entries, 2bit global history predictor
    ));
    branch_predictors.push_back(new TournamentHybridPredictor(11,
	new NbitPredictor(13, 2), // 8K entries, 2bit predictor
	new LocalHistoryPredictor(12, 2, 12, 2) // local history predictor, BHT and PHT:4K entries 2bit each
    ));
    branch_predictors.push_back(new TournamentHybridPredictor(11,
	new LocalHistoryPredictor(12, 2, 12, 2),
	new GlobalHistoryPredictor(13, 2)
    ));
    branch_predictors.push_back(new TournamentHybridPredictor(11,
	new GlobalHistoryPredictor(13, 2),
	new GlobalHistoryPredictor(12, 4) // 4K entries, 4bit global history predictor
    ));
    branch_predictors.push_back(new TournamentHybridPredictor(11,
	new LocalHistoryPredictor(12, 2, 12, 2),
	new LocalHistoryPredictor(11, 4, 12, 2) // local history predictor,BHT:2K entries,4bit,PHT:4K entries,2bit
    ));

}

VOID BTB()
{
    btb_predictors.push_back(new BTBPredictor(512, 1));
    btb_predictors.push_back(new BTBPredictor(512, 2));
    btb_predictors.push_back(new BTBPredictor(256, 2));
    btb_predictors.push_back(new BTBPredictor(256, 4));
    btb_predictors.push_back(new BTBPredictor(128, 2));
    btb_predictors.push_back(new BTBPredictor(128, 4));
    btb_predictors.push_back(new BTBPredictor(64, 4));
    btb_predictors.push_back(new BTBPredictor(64, 8));
}

VOID InitRas()
{
    for (UINT32 i = 4; i <= 64; i*=2) {
        ras_vec.push_back(new RAS(i));
        if (i == 32)
            ras_vec.push_back(new RAS(48));

        }
    }

int main(int argc, char *argv[])
{
    PIN_InitSymbols();

    if(PIN_Init(argc,argv))
        return Usage();

    // Open output file
    outFile.open(KnobOutputFile.Value().c_str());

    // Initialize predictors and RAS vector
    //InitPredictors();
    //BTB();
    InitRas();

    // Instrument function calls in order to catch __parsec_roi_{begin,end}
    INS_AddInstrumentFunction(Instruction, 0);

    // Called when the instrumented application finishes its execution
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
