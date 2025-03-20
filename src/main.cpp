// PTXprofiler
// Copyright (c) Moritz Lehmann

// unsupported matrix instructions: ldmatrix, mma, movmatrix, stmatrix, wgmma, wmma
// unsupported texture/surface instructions: istypep, suld, suq, sured, sust, tex, tld4, txq
// unsupported video instructions: vabsdiff, vabsdiff2, vabsdiff4, vadd, vadd2, vadd4, vavrg2, vavrg4, vmad, vmax, vmax2, vmax4, vmin, vmin2, vmin4, vset, vset2, vset4, vshl, vshr, vsub, vsub2, vsub4
// unsupported synchronization/communication instructions: activemask, atom, bar, barrier, cp.async, elect, griddepcontrol, match, mbarrier, membar, red, redux, vote
// other unsupported instructions: alloca, applypriority, brkpt, call, createpolicy, discard, exit, fence, getctarank, isspacep, nanosleep, pmevent, prefetch, prefetchu, ret, setmaxnreg, stackrestore, stacksave, trap

#include <iostream>
#include <regex>
#include <fstream>
using std::string;
using std::vector;
typedef unsigned int uint;

void println(const string& s="") {
	std::cout << s+'\n';
}
void wait() {
	std::cin.get();
}
uint min(const uint x, const uint y) {
	return x<y?x:y;
}
uint max(const uint x, const uint y) {
	return x>y?x:y;
}
vector<string> get_main_arguments(int argc, char* argv[]) {
	return argc>1 ? vector<string>(argv+1, argv+argc) : vector<string>();
}
string to_string(uint x) {
	string r = "";
	do {
		r = (char)(x%10u+48u)+r;
		x /= 10u;
	} while(x);
	return r;
}
string alignl(const uint n, const string& x="") { // converts x to string with spaces behind such that length is n if x is not longer than n
	string s = x;
	for(uint i=0u; i<n; i++) s += " ";
	return s.substr(0, max(n, (uint)x.length()));
}
string alignr(const uint n, const string& x="") { // converts x to string with spaces in front such that length is n if x is not longer than n
	string s = "";
	for(uint i=0u; i<n; i++) s += " ";
	s += x;
	return s.substr((uint)min((int)s.length()-(int)n, (int)n), s.length());
}
string alignl(const uint n, const uint x) { // converts x to string with spaces behind such that length is n if x does not have more digits than n
	return alignl(n, to_string(x));
}
string alignr(const uint n, const uint x) { // converts x to string with spaces in front such that length is n if x does not have more digits than n
	return alignr(n, to_string(x));
}
vector<string> split_regex(const string& s, const string& separator="\\s+") {
	vector<string> r;
	const std::regex rgx(separator);
	std::sregex_token_iterator token(s.begin(), s.end(), rgx, -1), end;
	while(token!=end) {
		r.push_back(*token);
		token++;
	}
	return r;
}
uint matches_regex(const string& s, const string& match) { // counts number of matches
	std::regex words_regex(match);
	auto words_begin = std::sregex_iterator(s.begin(), s.end(), words_regex);
	auto words_end = std::sregex_iterator();
	return (uint)std::distance(words_begin, words_end);
}
string read_file(const string& filename) {
	std::ifstream file(filename, std::ios::in);
	if(file.fail()) println("\rError: File \""+filename+"\" does not exist!");
	const string r((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	return r;
}

uint count_flops(const string& ptxcode) { // count floating point operations
	uint n = matches_regex(ptxcode, "\\s(abs|add|copysign|cos|div|ex2|lg2|max|min|mul|neg|rcp|rsqrt|sin|sqrt|sub|tanh|testp)(\\.(approx|ftz|full|rm|rn|rp|rz|sat))*\\.f(16|32|64)");
	n +=  2u*matches_regex(ptxcode, "\\s(fma|mad)(\\.(approx|ftz|full|rm|rn|rp|rz|sat))*\\.f(16|32|64)"); // fma/mad count double
	n +=  2u*matches_regex(ptxcode, "\\s(abs|add|copysign|cos|div|ex2|lg2|max|min|mul|neg|rcp|rsqrt|sin|sqrt|sub|tanh|testp)(\\.(approx|ftz|full|rm|rn|rp|rz|sat))*\\.f16x2");
	n +=  4u*matches_regex(ptxcode, "\\s(fma|mad)(\\.(approx|ftz|full|rm|rn|rp|rz|sat))*\\.f16x2"); // fma/mad count double
	return n;
}
uint count_iops(const string& ptxcode) { // count integer operations
	uint n = matches_regex(ptxcode, "\\s(abs|add|addc|bfe|bfi|bfind|brev|clz|div|fns|max|min|mul|mul24|neg|popc|rem|sad|sub|subc|szext\\.(clamp|wrap))(\\.(cc|hi|lo|sat|wide))*\\.(s|u)");
	n +=  2u*matches_regex(ptxcode, "\\s(mad|mad24|madc)(\\.(cc|hi|lo|sat|wide))*\\.(s|u)"); // mad counts double
	n +=  4u*matches_regex(ptxcode, "\\sdp2a(\\.(cc|hi|lo|sat|wide))*\\.(s|u)"); // dot product accumulate counts as 4 iops
	n +=  8u*matches_regex(ptxcode, "\\sdp4a(\\.(cc|hi|lo|sat|wide))*\\.(s|u)"); // dot product accumulate counts as 8 iops
	return n;
}
uint count_bops(const string& ptxcode) { // count bit operations
	uint n = matches_regex(ptxcode, "\\s(and|bmsk|cnot|lop3|not|or|prmt|set|shf|shl|shr|xor)\\.");
	n +=     matches_regex(ptxcode, "\\s(selp|setp|slct)\\."); // count comparison operations as bit operations
	return n;
}
uint count_cops(const string& ptxcode) { // count register copy operations
	return   matches_regex(ptxcode, "\\s(ld\\.param|cvt|cvta|mapa|mov|shfl)\\.");
}
uint count_branches(const string& ptxcode) { // count branch operations
	return   matches_regex(ptxcode, "\\s(bra|brx)");
}
uint count_memory_accesses(const string& ptxcode, const string& prequel) { // count cache transfers in byte
	uint n = matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.(b|f|s|u)8"); // scalar
	n +=  2u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.(b|f|s|u)16");
	n +=  4u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.(b|f|s|u)(32|16x2)");
	n +=  8u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.(b|f|s|u)64");
	n +=  2u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.v2\\.(b|f|s|u)8"); // vector v2
	n +=  4u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.v2\\.(b|f|s|u)16");
	n +=  8u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.v2\\.(b|f|s|u)(32|16x2)");
	n += 16u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.v2\\.(b|f|s|u)64");
	n +=  4u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.v4\\.(b|f|s|u)8"); // vector v4
	n +=  8u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.v4\\.(b|f|s|u)16");
	n += 16u*matches_regex(ptxcode, "\\s"+prequel+"(\\.(ca|cg|cs|cv|lu|wb|wt))?\\.v4\\.(b|f|s|u)(32|16x2)");
	return n;
}
uint count_cache_loads(const string& ptxcode) { // count cache transfers in byte
	return count_memory_accesses(ptxcode, "ld\\.shared");
}
uint count_cache_stores(const string& ptxcode) { // count cache transfers in byte
	return count_memory_accesses(ptxcode, "st\\.shared");
}
uint count_memory_loads(const string& ptxcode) { // count memory transfers in byte
	return count_memory_accesses(ptxcode, "ld\\.global");
}
uint count_memory_loads_cached(const string& ptxcode) { // count memory transfers in byte
	return count_memory_accesses(ptxcode, "(ld\\.(global\\.nc|const)|ldu\\.global)");
}
uint count_memory_stores(const string& ptxcode) { // count memory transfers in byte
	return count_memory_accesses(ptxcode, "st\\.global");
}

int main(int argc, char* argv[]) {
	const vector<string> main_arguments = get_main_arguments(argc, argv);
	const string path = (uint)main_arguments.size()==1u ? main_arguments[0] : "./kernel.ptx";
	string ptxcode = read_file(path);
	if(ptxcode.length()>0) {
		vector<string> kernel = split_regex(ptxcode, "\\s// \\.globl\t"); // split ptx file in individual kernels
		kernel.erase(kernel.begin()); // first element is not a kernel, so delete it
		vector<string> names;
		for(uint i=0u; i<(uint)kernel.size(); i++) names.push_back(split_regex(kernel[i])[0]);
		println("kernel name                     |flops  (float int    bit  )|copy  |branch|cache  (load  store)|memory (load  cached store)");
		println("--------------------------------|---------------------------|------|------|--------------------|---------------------------");
		for(uint i=0; i<(uint)kernel.size(); i++) {
			const uint flops = count_flops(kernel[i]); // count everything only once
			const uint ipos = count_iops(kernel[i]);
			const uint bops = count_bops(kernel[i]);
			const uint cops = count_cops(kernel[i]);
			const uint branches = count_branches(kernel[i]);
			const uint cache_loads = count_cache_loads(kernel[i]);
			const uint cache_stores = count_cache_stores(kernel[i]);
			const uint memory_loads = count_memory_loads(kernel[i]);
			const uint memory_loads_cached = count_memory_loads_cached(kernel[i]);
			const uint memory_stores = count_memory_stores(kernel[i]);
			println(
				alignl(32, names[i])+"|"+
				alignr(6, flops+ipos+bops)+" "+alignr(6, flops)+" "+alignr(6, ipos)+" "+alignr(6, bops)+"|"+
				alignr(6, cops)+"|"+alignr(6, branches)+"|"+
				alignr(6, cache_loads+cache_stores)+" "+alignr(6, cache_loads)+" "+alignr(6, cache_stores)+"|"+
				alignr(6, memory_loads+memory_loads_cached+memory_stores)+" "+alignr(6, memory_loads)+" "+alignr(6, memory_loads_cached)+" "+alignr(6, memory_stores)
			);
			//println(kernel[i]);
		}
	}
	wait();
	return 0;
}