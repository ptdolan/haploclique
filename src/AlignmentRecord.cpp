/* Copyright 2012 Tobias Marschall
 * 
 * This file is part of HaploClique.
 * 
 * HaploClique is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HaploClique is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HaploClique.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <vector>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/compare.hpp>

#include "BamHelper.h"

#include "AlignmentRecord.h"

using namespace std;
using namespace boost;

AlignmentRecord::AlignmentRecord(const string& line, std::map<std::string,std::string> clique_to_reads, ReadGroups* read_groups) {
	this->line = line;
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
	boost::char_separator<char> separator(" \t");
	tokenizer_t tokenizer(line,separator);
	vector<string> tokens(tokenizer.begin(), tokenizer.end());
	this->hcount = 0;
	this->readCount = 0;
	if (tokens.size() == 12) {
		single_end = true;
	} else if (tokens.size() == 21) {
		single_end = false;
	} else {
		throw std::runtime_error("Error parsing alignment pair.");
	}
	try {
		this->name = tokens[0];
		if (clique_to_reads.find(this->name) != clique_to_reads.end()) {
            string complex = clique_to_reads[this->name];
            if (complex.find(",") != std::string::npos) {
                vector<string> fields2;
                boost::split(fields2, complex, is_any_of(","));
                for (int x=0; x<fields2.size();++x) {
                	if (boost::starts_with(fields2[x], "HCOUNT|")) {
                  		vector<string> split;
		                boost::split(split, fields2[x], boost::is_any_of("|"));
                		this->hcount = boost::lexical_cast<int>(split[1]);
                	} else {
                		this->readCount++;
	                	this->readNames.insert(fields2[x]);
	                }
                }
            } else {
            	if (boost::starts_with(complex, "HCOUNT|")) {
              		vector<string> split;
	                boost::split(split, complex, boost::is_any_of("|"));
            		this->hcount = boost::lexical_cast<int>(split[1]);
            	}
            }
        } else {
        	this->readNames.insert(this->name);
            this->readCount = 1;
        }

		this->record_nr = boost::lexical_cast<int>(tokens[1]);
		if (read_groups == 0) {
			this->read_group = -1;
		} else {
			this->read_group = read_groups->getIndex(tokens[2]);
			if (this->read_group == -1) {
				ostringstream oss;
				oss << "Unknown read group \"" << tokens[2] << "\" encountered in read \"" << this->name << "\"";
				throw std::runtime_error(oss.str());
			}
		}
		this->phred_sum1 = boost::lexical_cast<int>(tokens[3]);
		this->chrom1 = tokens[4];
		this->start1 = boost::lexical_cast<int>(tokens[5]);
		this->end1 = boost::lexical_cast<int>(tokens[6]);
		this->strand1 = tokens[7];
		BamHelper::parseCigar(tokens[8], &cigar1);
		this->sequence1 = ShortDnaSequence(tokens[9], tokens[10]);

		this->length_incl_deletions1 = this->sequence1.size();
		this->length_incl_longdeletions1 = this->sequence1.size();
		for (vector<BamTools::CigarOp>::const_iterator it = cigar1.begin(); it != cigar1.end(); ++it) {
            for (int s = 0; s < it->Length; ++s) {
            	this->cigar1_unrolled.push_back(it->Type);
            }
            if (it->Type == 'D') {
        		this->length_incl_deletions1+=it->Length;
        		if (it->Length > 1) {
        			this->length_incl_longdeletions1+=it->Length;
        		}
        	}
        }
		if (single_end) {
			this->aln_prob = boost::lexical_cast<double>(tokens[11]);
		} else {
			this->phred_sum2 = boost::lexical_cast<int>(tokens[11]);
			this->chrom2 = tokens[12];
			this->start2 = boost::lexical_cast<int>(tokens[13]);
			this->end2 = boost::lexical_cast<int>(tokens[14]);
			this->strand2 = tokens[15];
			BamHelper::parseCigar(tokens[16], &cigar2);
			this->sequence2 = ShortDnaSequence(tokens[17], tokens[18]);
			this->aln_prob = boost::lexical_cast<double>(tokens[19]);
			this->aln_pair_prob_ins_length = boost::lexical_cast<double>(tokens[20]);
			this->length_incl_deletions2 = this->sequence2.size();
			this->length_incl_longdeletions2 = this->sequence2.size();
	    	for (vector<BamTools::CigarOp>::const_iterator it = cigar2.begin(); it != cigar2.end(); ++it) {
	            for (int s = 0; s < it->Length; ++s) {
	            	this->cigar2_unrolled.push_back(it->Type);
	            }
                if (it->Type == 'D') {
	        		this->length_incl_deletions2+=it->Length;
	        		if (it->Length > 1) {
	        			this->length_incl_longdeletions2+=it->Length;
	        		}
	        	}
	        }
		}
		this->id = 0;
		

	} catch(boost::bad_lexical_cast &){
		throw std::runtime_error("Error parsing alignment pair.");
	}
}

size_t AlignmentRecord::intersectionLength(const AlignmentRecord& ap) const {
	assert(single_end == ap.single_end);
	int left = max(getIntervalStart(), ap.getIntervalStart());
	int right = min(getIntervalEnd(), ap.getIntervalEnd()) + 1;
	return max(0, right-left);
}

size_t AlignmentRecord::internalSegmentIntersectionLength(const AlignmentRecord& ap) const {
	int left = max(getInsertStart(), ap.getInsertStart());
	int right = min(getInsertEnd(), ap.getInsertEnd()) + 1;
	return max(0, right-left);
}

unsigned int AlignmentRecord::getRecordNr() const {
	return record_nr;
}

int AlignmentRecord::getPhredSum1() const {
	return phred_sum1;
}

int AlignmentRecord::getPhredSum2() const {
	assert(!single_end);
	return phred_sum2;
}

double AlignmentRecord::getProbability() const {
	return aln_prob;
}

double AlignmentRecord::getProbabilityInsertLength() const {
	assert(!single_end);
	return aln_pair_prob_ins_length;
}

std::string AlignmentRecord::getChrom1() const {
	return chrom1;
}

std::string AlignmentRecord::getChrom2() const {
	assert(!single_end);
	return chrom2;
}

std::string AlignmentRecord::getChromosome() const { 
	assert(single_end || (chrom1.compare(chrom2) == 0));
	return chrom1; 
}

unsigned int AlignmentRecord::getEnd1() const {
	return end1;
}

unsigned int AlignmentRecord::getEnd2() const {
	assert(!single_end);
	return end2;
}

std::string AlignmentRecord::getName() const {
	return name;
}

unsigned int AlignmentRecord::getStart1() const {
	return start1;
}

unsigned int AlignmentRecord::getStart2() const {
	assert(!single_end);
	return start2;
}

std::string AlignmentRecord::getStrand1() const {
	return strand1;
}

std::string AlignmentRecord::getStrand2() const {
	assert(!single_end);
	return strand2;
}

const std::vector<BamTools::CigarOp>& AlignmentRecord::getCigar1() const {
	return cigar1;
}

const std::vector<BamTools::CigarOp>& AlignmentRecord::getCigar2() const {
	assert(!single_end);
	return cigar2;
}

const ShortDnaSequence& AlignmentRecord::getSequence1() const {
	return sequence1;
}

const ShortDnaSequence& AlignmentRecord::getSequence2() const {
	assert(!single_end);
	return sequence2;
}

int AlignmentRecord::getReadGroup() const {
	return read_group;
}

double AlignmentRecord::getWeight() const { 
	if (single_end) {
		return aln_prob;
	} else {
		return aln_pair_prob_ins_length;
	}
}

unsigned int AlignmentRecord::getIntervalStart() const {
	return start1;
}

unsigned int AlignmentRecord::getIntervalEnd() const {
	if (single_end) {
		return end1;
	} else {
		return end2;
	}
}

unsigned int AlignmentRecord::getInsertStart() const {
	assert(!single_end);
	return end1 + 1;
}

unsigned int AlignmentRecord::getInsertEnd() const {
	assert(!single_end);
	return start2 - 1;
}

unsigned int AlignmentRecord::getInsertLength() const {
	assert(!single_end);
	return start2 - (end1 + 1);
}

alignment_id_t AlignmentRecord::getID() const {
	return id;
}

void AlignmentRecord::setID(alignment_id_t id) {
	this->id = id;
}

bool AlignmentRecord::isSingleEnd() const {
	return single_end;
}

bool AlignmentRecord::isPairedEnd() const {
	return !single_end;
}

std::string AlignmentRecord::getLine() const {
	return this->line;
}

std::set<std::string> AlignmentRecord::getReadNames() const {
	return this->readNames;
}

int AlignmentRecord::getReadCount() const {
	return this->readCount;
}
int AlignmentRecord::getHCount() const {
	return this->hcount;
}
int AlignmentRecord::getCount() const {
	if (this->hcount+this->readCount < 0) {
		cerr << "GETCOUNT: " << this->hcount << "\t" << this->readCount << endl;
	}
	return this->hcount+this->readCount;
}
const std::vector<char> AlignmentRecord::getCigar1Unrolled() const {
	return this->cigar1_unrolled;
}
const std::vector<char> AlignmentRecord::getCigar2Unrolled() const {
	return this->cigar2_unrolled;
}
int AlignmentRecord::getLengthInclDeletions1() const {
	return this->length_incl_deletions1;
}
int AlignmentRecord::getLengthInclDeletions2() const {
	return this->length_incl_deletions2;
}
int AlignmentRecord::getLengthInclLongDeletions1() const {
	return this->length_incl_longdeletions1;
}
int AlignmentRecord::getLengthInclLongDeletions2() const {
	return this->length_incl_longdeletions2;
}