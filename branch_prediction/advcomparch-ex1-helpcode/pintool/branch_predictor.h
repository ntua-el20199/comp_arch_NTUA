#ifndef BRANCH_PREDICTOR_H
#define BRANCH_PREDICTOR_H

#include <sstream> // std::ostringstream
#include <cmath>   // pow(), floor
#include <cstring> // memset()
#include <vector>
#include <list>
#include <cstdint>

/**
 * A generic BranchPredictor base class.
 * All predictors can be subclasses with overloaded predict() and update()
 * methods.
 **/
class BranchPredictor
{
public:
    BranchPredictor() : correct_predictions(0), incorrect_predictions(0) {};
    virtual ~BranchPredictor() {};

    virtual bool predict(ADDRINT ip, ADDRINT target) = 0;
    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) = 0;
    virtual string getName() = 0;

    UINT64 getNumCorrectPredictions() { return correct_predictions; }
    UINT64 getNumIncorrectPredictions() { return incorrect_predictions; }

   void resetCounters() { correct_predictions = incorrect_predictions = 0; };

protected:
    void updateCounters(bool predicted, bool actual) {
        if (predicted == actual)
            correct_predictions++;
        else
            incorrect_predictions++;
    };

private:
    UINT64 correct_predictions;
    UINT64 incorrect_predictions;
};

class AlwaysTakenPredictor : public BranchPredictor {
public:
	AlwaysTakenPredictor() { }
	~AlwaysTakenPredictor() { }

	virtual bool predict(ADDRINT ip, ADDRINT target) {
		return true;
	}	

	virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		updateCounters(predicted, actual);
	}

	virtual string getName() {
		std::ostringstream stream;
		stream << "Static AlwaysTaken";
		return stream.str();
	}
};

class BTFNTPredictor : public BranchPredictor {
public:
	BTFNTPredictor() { }
	~BTFNTPredictor() { }

	virtual bool predict(ADDRINT ip, ADDRINT target) {
		return ip > target;
	}	

	virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		updateCounters(predicted, actual);
	}

	virtual string getName() {
		std::ostringstream stream;
		stream << "Static BTFNT";
		return stream.str();
	}
};

class NbitPredictor : public BranchPredictor
{
public:
    NbitPredictor(unsigned index_bits_, unsigned cntr_bits_, int type_ = 1)
        : BranchPredictor(), index_bits(index_bits_), cntr_bits(cntr_bits_), type(type_) {
        table_entries = 1 << index_bits;
        TABLE = new unsigned long long[table_entries];
        memset(TABLE, 0, table_entries * sizeof(*TABLE));
        
	// Enable different type only when cntr_bits == 2 (N=2)
	if(cntr_bits != 2) type = 1;

        COUNTER_MAX = (1 << cntr_bits) - 1;
    };
    ~NbitPredictor() { delete TABLE; };

    virtual bool predict(ADDRINT ip, ADDRINT target) {
        unsigned int ip_table_index = ip % table_entries;
        unsigned long long ip_table_value = TABLE[ip_table_index];
        unsigned long long prediction = ip_table_value >> (cntr_bits - 1);
        return (prediction != 0);
    };

    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
        unsigned int ip_table_index = ip % table_entries;
	switch(type) {
	case 1:
		if (actual) {
		    if (TABLE[ip_table_index] < COUNTER_MAX)
			TABLE[ip_table_index]++;
		} else {
		    if (TABLE[ip_table_index] > 0)
			TABLE[ip_table_index]--;
		}
		break;
	case 2:
		if(actual) {
			// Normal up
			if(TABLE[ip_table_index] < COUNTER_MAX)
				TABLE[ip_table_index]++;
		} else {
			// If state == 2 then go to state 0
			if(TABLE[ip_table_index] == 2) {
				TABLE[ip_table_index] = 0;
			} else { // for the other states normal down
				if(TABLE[ip_table_index] > 0)
					TABLE[ip_table_index]--;
			}
		}
		break;
	case 3:
		if(actual) {
			// If state == 1 then go to state 3
			if(TABLE[ip_table_index] == 1) {
				TABLE[ip_table_index] = 3;
			} else { // for the other states normal up
				if(TABLE[ip_table_index] < COUNTER_MAX)
					TABLE[ip_table_index]++;
			}
		} else { // Normal down
			if(TABLE[ip_table_index] > 0)
				TABLE[ip_table_index]--;
		}
		break;
	case 4:
		if(actual) {
			// If state == 1 then go to state 3
			if(TABLE[ip_table_index] == 1) {
				TABLE[ip_table_index] = 3;
			} else { // for the other states normal up
				if(TABLE[ip_table_index] < COUNTER_MAX)
					TABLE[ip_table_index]++;
			}
			
		} else {
			// If state == 2 then go to state 0
			if(TABLE[ip_table_index] == 2) {
				TABLE[ip_table_index] = 0;
			} else { // for the other states normal down
				if(TABLE[ip_table_index] > 0)
					TABLE[ip_table_index]--;
			}

		}
		break;
	case 5:
		if(actual) {
			// If state == 1 then go to state 3
			if(TABLE[ip_table_index] == 1) {
				TABLE[ip_table_index] = 3;
			} else if(TABLE[ip_table_index] == 3) { // if state == 3 then go to state 2
				TABLE[ip_table_index] = 2;
			} else { // fot the other states normal up
				if(TABLE[ip_table_index] < COUNTER_MAX)
					TABLE[ip_table_index]++;
			}
		} else {
			// Normal down
			if(TABLE[ip_table_index] > 0)
				TABLE[ip_table_index]--;
		}
		break;
	default:
		std::cerr << "Unknown type of NBitPredictor! Valid types: 1,2,3,4,5.\n";
	}
        updateCounters(predicted, actual);
    };

    virtual string getName() {
        std::ostringstream stream;
        stream << "Nbit-" << pow(2.0,double(index_bits)) / 1024.0 << "K-" << cntr_bits;
	if(type > 1)
		stream << " (type=" << type << ")";
        return stream.str();
    }

private:
    unsigned int index_bits, cntr_bits;
    unsigned int COUNTER_MAX;
    
    /* Make this unsigned long long so as to support big numbers of cntr_bits. */
    unsigned long long *TABLE;
    unsigned int table_entries;

    int type;
};
	
class ShiftRegister {
public:
	ShiftRegister(std::uint16_t size) : data(0), size(size) { }

	void shiftRight(bool in_bit) {
		data = ((data >> 1) | (in_bit ? (1 << (size-1)) : 0)) & this->mask();
	}

	std::uint16_t getValue() const {
		return data;
	}

private:
	std::uint16_t data;
	std::uint16_t size;

	std::uint16_t mask() const {
		// e.g. if size = 4 mask = 0b1111
		return (1 << size) - 1;
	}
};

class GlobalHistoryPredictor : public BranchPredictor {
public:
	GlobalHistoryPredictor(int entries_bits, int nbit_length) {
		this->pht_entries = 1 << entries_bits;
		this->nbit_length = nbit_length;
		this->BHR_MAX = (1 << nbit_length) - 1;
		this->bhr = new ShiftRegister(nbit_length);
		this->num_predictors = 1 << nbit_length;
		for(int i = 0; i < this->num_predictors; i++)
			this->predictors.push_back(new NbitPredictor(entries_bits, nbit_length));
	}

	~GlobalHistoryPredictor() {
		delete this->bhr;

		for(int i = 0; i < this->num_predictors; i++)
			delete this->predictors[i];
	}

	virtual bool predict(ADDRINT ip, ADDRINT target) {
		// select the predictor and predict
		return this->predictors[this->bhr->getValue()]->predict(ip, target);	
	}

	virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		// update the selected predictor
		this->predictors[this->bhr->getValue()]->update(predicted, actual, ip, target);

		// update the Branch History Register
		this->bhr->shiftRight(actual);
		
		updateCounters(predicted, actual);
	}

	virtual string getName() {
		std::ostringstream stream;
		stream << "Global History Two Level Predictor (entries=" << this->pht_entries 
		       << ", nbit=" << this->nbit_length << ")";
		return stream.str();
	}

private:
	int pht_entries, nbit_length; // bhr_length = nbit_length
	int num_predictors;
	ShiftRegister* bhr;
	int BHR_MAX;
	std::vector< NbitPredictor* > predictors;
};

class LocalHistoryPredictor : public BranchPredictor {
public:
	LocalHistoryPredictor(int bht_entry_bits, int bht_length, int pht_entry_bits=13, int pht_length=2) {
		this->bht_entry_bits = bht_entry_bits;
		this->bht_entries = 1 << bht_entry_bits;
		this->bht_length = bht_length;
		
		this->pht_entry_bits = pht_entry_bits;
		this->pht_length = pht_length;
		pht = new NbitPredictor(pht_entry_bits, pht_length);
		
		for(int i = 0; i < this->bht_entries; i++)
			this->bht.push_back(new ShiftRegister(bht_length));
	}

	~LocalHistoryPredictor() {
		delete pht;
		
		for(std::size_t i = 0; i < this->bht.size(); i++)
			delete this->bht[i];
	}

	virtual bool predict(ADDRINT ip, ADDRINT target) {
		std::uint16_t bht_index = ip & this->bht_mask();
		std::uint16_t bht_value = this->bht[bht_index]->getValue(); // Z bits (bht_length)
		
		std::uint16_t pc_part = ip & this->pc_mask(); // pht_entry_bits - Z bits
		std::uint16_t custom_ip = (pc_part << this->bht_length) | (bht_value); // pht_entry_bits bits

		return this->pht->predict(custom_ip, target);		
	}

	virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		std::uint16_t bht_index = ip & this->bht_mask();
		std::uint16_t bht_value = this->bht[bht_index]->getValue(); // Z bits (bht_length)
		
		std::uint16_t pc_part = ip & this->pc_mask(); // pht_entry_bits - Z bits
		std::uint16_t custom_ip = (pc_part << this->bht_length) | (bht_value); // pht_entry_bits bits
		
		// update the correct ShiftRegister that we used for prediction
		this->bht[bht_index]->shiftRight(actual);

		// update the pht with the correct ip
		this->pht->update(predicted, actual, custom_ip, target);

		updateCounters(predicted, actual);
	}

	virtual string getName() {
		std::ostringstream stream;
		stream << "Local History Two Level Predictor(BHT entries=" << this->bht_entries
		       << ", BHT length=" << this->bht_length << ")";
		return stream.str();
	}
private:
	int bht_entry_bits, bht_entries, bht_length;
	int pht_entry_bits, pht_length;
	NbitPredictor* pht;
	std::vector< ShiftRegister* > bht;

	// mask to get the last bht_entry_bits bits of the PC to index BHT
	std::uint16_t bht_mask() const {
		return (1 << bht_entry_bits) - 1;
	}

	std::uint16_t pc_mask() const {
		return (1 << (pht_entry_bits-bht_length)) -1;
	}
};

class Alpha21264 : public BranchPredictor {
public:
	Alpha21264() {
		// 12bit shift register for global history
		global_history = new ShiftRegister(12);
		
		// Choice Predictor is a 2bit predictor with 4K=2^12 entries
		choice_predictor = new NbitPredictor(12, 2);

		// (Pred0) Local History Two Level with BHT: 1K=2^10 entries, 10bit entries 
		// and PHT: 1K=2^10 entries, 3bit predictors
		lhp = new LocalHistoryPredictor(10, 10, 10, 3);

		// (Pred1) Global History Two Level with PHT: 4K=2^12 entries, 2bit predictors
		ghp = new GlobalHistoryPredictor(12, 2);
	}

	~Alpha21264() {
		delete global_history;
		delete ghp;
		delete lhp;
		delete choice_predictor;
	}

	virtual bool predict(ADDRINT ip, ADDRINT target) {
		std::uint16_t history = this->global_history->getValue();
		bool choice = this->choice_predictor->predict(history, 0); // it does not use the target

		this->pred0 = this->lhp->predict(ip, target);
		this->pred1 = this->ghp->predict(history, target);

		return choice ? pred1 : pred0; 
	}

	virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		if(this->pred0 == actual && this->pred1 != actual)
			this->choice_predictor->update(predicted, false, ip, target);
		if(this->pred0 != actual && this->pred1 == actual)
			this->choice_predictor->update(predicted, true, ip, target);	

		std::uint16_t history = this->global_history->getValue();
		this->lhp->update(predicted, actual, ip, target);
		this->ghp->update(history, actual, ip, target);

		this->global_history->shiftRight(actual);

		updateCounters(predicted, actual);
	}

	virtual string getName() {
		std::ostringstream stream;
		stream << "Alpha 21264";
		return stream.str();
	}
private:
	ShiftRegister* global_history;
	GlobalHistoryPredictor* ghp;
	LocalHistoryPredictor* lhp;
	NbitPredictor* choice_predictor;
	bool pred0, pred1;
};

class TournamentHybridPredictor : public BranchPredictor {
public:
	TournamentHybridPredictor(int meta_entry_bits, BranchPredictor* pred0, BranchPredictor* pred1) {
		this->meta = new NbitPredictor(meta_entry_bits, 2);
		this->pred0 = pred0;
		this->pred1 = pred1;
	}

	~TournamentHybridPredictor() {
		delete meta;
		delete pred0;
		delete pred1;
	}

	virtual bool predict(ADDRINT ip, ADDRINT target) {
		bool choice = this->meta->predict(ip, target);
		this->p0 = this->pred0->predict(ip, target);
		this->p1 = this->pred1->predict(ip, target);

		return choice ? this->p1 : this->p0;
	}

	virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		if(this->p0 == actual && this->p1 != actual)
			this->meta->update(predicted, false, ip, target);
		if(this->p0 != actual && this->p1 == actual)
			this->meta->update(predicted, true, ip, target);

		this->pred0->update(predicted, actual, ip, target);
		this->pred1->update(predicted, actual, ip, target);

		updateCounters(predicted, actual);
	}

	virtual string getName() {
		std::ostringstream stream;
		stream << "Tournament Hyprid Predictor\n"
		       << "| Meta : " << this->meta->getName() << '\n'
		       << "| Pred0: " << this->pred0->getName() << '\n'
		       << "| Pred1: " << this->pred1->getName() << '\n';
		return stream.str();
	}
private:
	NbitPredictor* meta;
	BranchPredictor* pred0; bool p0;
	BranchPredictor* pred1; bool p1;
};

// Fill in the BTB implementation ...
class BTBPredictor : public BranchPredictor
{
public:
	BTBPredictor(int btb_lines, int btb_assoc)
	     : table_lines(btb_lines), table_assoc(btb_assoc), correct_target_predictions(0)
	{
		this->num_sets = btb_lines / btb_assoc;
		sets.resize(this->num_sets);
	}

	~BTBPredictor() { }

	virtual bool predict(ADDRINT ip, ADDRINT target) {
		// find ip's set
		unsigned int index = ip % this->num_sets;
		std::list<BufferEntry>& s = this->sets[index];
		
		for(auto it = s.begin(); it != s.end(); it++) {
			if(it->ip == ip) { // found the instruction address
				BufferEntry be = *it;
				s.erase(it);
				s.push_front(be); // move to the front of the list
						  
				if(be.target == target) {
					correct_target_predictions++;
					return true;
				} else {
					return false;
				}
			}
		}

		return false;
	}

	virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		unsigned int index = ip % this->num_sets;
		std::list<BufferEntry>& s = this->sets[index];
		
		for(auto it = s.begin(); it != s.end(); it++) {
			if(it->ip == ip) {
				if(actual) { // was branch taken?
					BufferEntry be = *it;
					s.erase(it);
					s.push_front(be);
				} else { // branch was not taken
					s.erase(it);
				}

        			updateCounters(predicted, actual);
				return;
			}
		}

		// not found in BTB and he instruction is a taken branch
		if(actual) {
			// if size of the set is max remove LRU element
			if((int)s.size() >= this->table_assoc) s.pop_back();

			// now add the new BufferEntry at the start of the list
			s.push_front({ip, target});
		}
        	
		updateCounters(predicted, actual);
	}

	virtual string getName() { 
        	std::ostringstream stream;
		stream << "BTB-" << table_lines << "-" << table_assoc;
		return stream.str();
	}

    	UINT64 getNumCorrectTargetPredictions() { 
		return this->correct_target_predictions;
	}

private:
	int table_lines, table_assoc, num_sets;

	struct BufferEntry {
		ADDRINT ip;
		ADDRINT target;
	};

	UINT64 correct_target_predictions;
	
	// using vector of lists to implement LRU update
	// each set has a list of BufferEntry with size table_assoc
	std::vector< std::list<BufferEntry> > sets; 
};


#endif