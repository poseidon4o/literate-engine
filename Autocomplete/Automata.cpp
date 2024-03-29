#include "Automata.h"

#include <stack>

namespace
{

/// Combine two 64bit hash values and produce new hash
inline size_t hashCombine(size_t a, size_t b) {
	return a ^ (b + 0x9e3779b9 + (a<<6) + (a>>2));
}

/// Combine two 32bit hash values to produce new 64bit hash
inline size_t hashCombine(int a, int b) {
	return a | (size_t(b) << 32);
}

/// Hash a block of memory
/// @param data - pointer to the start
/// @param size - number of bytes to hash
/// @return the hash of the block
inline size_t hash(const char *data, int size) {
	size_t hash = 0xcbf29ce484222325ull;
	const size_t prime = 0x100000001b3ull;

	for (int c = 0; c < size; c++) {
		hash ^= data[c];
		hash *= prime;
	}

	return hash;
}

/// Check if a string is a prefix of another
/// @param prefix - the needled
/// @param string - the haystack
/// @return true if string begins with prefix
bool isPrefix(const std::string &prefix, const std::string &string) {
	if (prefix.size() > string.size()) {
		return false;
	}
	for (int c = 0; c < prefix.size(); c++) {
		if (string[c] != prefix[c]) {
			return false;
		}
	}
	return true;
}

}

Automata::Automata() {
	initEmpty();
}

void Automata::initEmpty() {
	allStates.resize(1);
	allStates.front() = State();
	rootState = &allStates.front();
	registry.clear();
	words.clear();
	totalSymbols = 0;
}

void Automata::build() {
	std::sort(words.begin(), words.end());
	const WordList::iterator it = std::unique(words.begin(), words.end());
	words.erase(it, words.end());

	State *start = nullptr;
	int steps = 0;
	for (int c = 0; c < words.size(); c++) {
		totalSymbols += words[c].size();
		if (words[c].empty()) {
			continue;
		}

		steps = 0;
		start = addWordPrefix(c, steps);
		ac_assert(start && "Must never be null");

		const bool isFinal = steps == words[c].size();
		if (isFinal) {
			start->setIsFinalState();
		}

		if (start && c > 0) {
			minimize(start, c - 1, steps);
		}

		if (!isFinal) {
			createNodes(start, c, steps);
		}
	}

	if (start && steps < words.back().size()) {
		minimize(start, words.size() - 1, steps);
	}

	registry.clear();
}

const std::string &Automata::getWord(int index) const {
	return words[index];
}

bool Automata::getSuffixes(const std::string &prefix, WordList &suffixes) const {
	const State *start = findState(prefix);
	if (!start) {
		return false;
	}
	if (start->isFinalState()) {
		suffixes.push_back("");
	}
	start->buildSuffixes(*this, suffixes);
	return true;
}


GraphDump *Automata::getDefaultGraphDump(const std::string &filePath) {
	if (!dotGraphViz.init(filePath)) {
		return nullptr;
	}
	return &dotGraphViz;
}

bool Automata::dumpGraph(GraphDump *graphDump) const {
	if (!graphDump) {
		return false;
	}
	graphDump->start();
	rootState->dumpGraph(*graphDump);
	graphDump->done();
}

bool Automata::verifyPrefix(int start, const std::string &prefix) const {
#if AC_ASSERT_ENABLED
	while (start > 0 && isPrefix(prefix, words[start - 1])) {
		--start;
	}

	WordList suffixes;
	(void)getSuffixes(prefix, suffixes);
	std::sort(suffixes.begin(), suffixes.end());

	for (const std::string &suffix : suffixes) {
		if (prefix + suffix != words[start]) {
			ac_assert(false);
			return false;
		}
		++start;
	}

#endif
	return true;
}

bool Automata::runVerify() const {
#if AC_ASSERT_ENABLED
	std::unordered_set<size_t> ranTests;
	const std::hash<std::string> hasher;

	for (int r = 0; r < words.size(); r++) {
		for (int c = 1; c < words[r].size(); c++) {
			const std::string &prefix = words[r].substr(0, c);
			const size_t hash = hasher(prefix);
			if (!contains(ranTests, hash)) {
				if (!verifyPrefix(r, prefix)) {
					return false;
				}
				ranTests.insert(hash);
			}
		}
	}

	std::unordered_set<const State *> visited;
	rootState->verifyAcyclicity(visited);
#endif
	return true;
}

const Automata::State *Automata::findState(const std::string &prefix) const {
	const State *iterator = rootState;
	int c = 0;
	while (iterator && c < prefix.size()) {
		iterator = iterator->findConnection(prefix[c]);
		c++;
	}

	return c < prefix.size() ? nullptr : iterator;
}

Automata::State *Automata::addWordPrefix(int wordIndex, int &steps) {
	steps = 0;
	
	State *iterator = rootState;
	State *parent = iterator;
	const symbol *wordIterator = words[wordIndex].c_str();

	while (iterator && *wordIterator) {
		iterator->appendSuffix(*this, wordIndex, steps);
		parent = iterator;
		iterator = iterator->findConnection(*wordIterator);
		++wordIterator;
		++steps;
	}

	--steps;
	return parent;
}

void Automata::createNodes(State *start, int wordIndex, int offset) {
	const std::string &word = words[wordIndex];
	for (int c = offset; c < word.size(); c++) {
		State *newState = nullptr;
		if (freeStates.empty()) {
			allStates.emplace_back();
			newState = &(allStates.back());
		} else {
			newState = freeStates.front();
			freeStates.pop();
		}

		newState->initSuffixesFrom(*this, *start, word[c]);
		start->addConnection(word[c], newState);
		start = newState;
	}
	start->setIsFinalState();
}

bool Automata::isDetached(State *state) {
	for (State &iter : allStates) {
		if (iter.hasChild(state)) {
			return false;
		}
	}
	return true;
}

void Automata::minimize(State *start, int wordIndex, int offset) {
	ac_assert(start);
	const symbol transition = words[wordIndex][offset];
	State *lastChild = start->findConnection(transition);
	if (!lastChild) {
		// states can't self minimize themselves, only parent can
		return;
	}

	// first recurse
	minimize(lastChild, wordIndex, offset + 1);

	const StatePtr ptr{this, lastChild};
	const Registry::iterator it = registry.find(ptr);
	if (it != registry.end()) {
		start->replaceChild(it->state, transition);
		// Those two checks make building the Automata in debug very slow
		// ac_assert(lastChild->slowEqual(*this, *it->state));
		// ac_assert(isDetached(lastChild));
		ac_assert(lastChild->isFinalState() == it->state->isFinalState());

		lastChild->clear();
		freeStates.push(lastChild);
	} else {
		registry.insert(ptr);
	}
}

///////////////////////////////
/// Automata::state methods ///
///////////////////////////////

Automata::State *Automata::State::findConnection(symbol transition) const {
	const ConnectionMap::const_iterator it = connections.find(transition);
	if (it == connections.end()) {
		return nullptr;
	}

	return it->second;
}

void Automata::State::setIsFinalState() {
	isFinal = true;
}

bool Automata::State::isFinalState() const {
	return isFinal;
}

void Automata::State::addConnection(symbol transition, State *child) {
	ac_assert(connections.find(transition) == connections.end() && "Already exists");
	connections[transition] = child;
	hashConnections = 42;
}

void Automata::State::appendSuffix(const Automata &automata, int wordIndex, int offset) {
#if AC_ASSERT_ENABLED
	const SuffixList::const_iterator it = std::find_if(suffixes.begin(), suffixes.end(), [wordIndex](const Suffix &suf) { return suf.wordIndex == wordIndex; });
	ac_assert(it == suffixes.end() && "Can't duplicate suffixes");
#endif
	suffixes.emplace_back(wordIndex, offset);
	hashSuffixes = 42;
}

void Automata::State::initSuffixesFrom(const Automata &automata, const State &parent, symbol transition) {
	suffixes.clear();
	for (const auto &item : parent.suffixes) {
		const std::string &word = automata.getWord(item.wordIndex);
		if (word[item.offset] == transition && item.offset + 1 < word.length()) {
			suffixes.emplace_back(item.wordIndex, item.offset + 1);
		}
	}
	hashSuffixes = 42;
}

void Automata::State::replaceChild(State *newChild, symbol transition) {
	ac_assert(connections.find(transition) != connections.end());
	connections[transition] = newChild;
	hashConnections = 42;
}

bool Automata::State::hasChild(const State *state) const {
	for (auto &connection : connections) {
		if (connection.second == state) {
			return true;
		}
	}
	return false;
}

size_t Automata::State::getHash(const Automata &automata) const {
	if (hashConnections == 42) {
		rebuildConnectionsHash();
	}
	if (hashSuffixes == 42) {
		rebuildSuffixesHash(automata);
	}
	return hashCombine(hashConnections, hashCombine(size_t(isFinal), hashSuffixes));
}

int Automata::State::getNumChildren() const {
	return connections.size();
}

void Automata::State::buildSuffixes(const Automata &automata, WordList &stringSuffixes) const {
	int c = stringSuffixes.size();
	stringSuffixes.resize(stringSuffixes.size() + suffixes.size());
	for (const auto &suffix : suffixes) {
		const std::string &word = automata.getWord(suffix.wordIndex);
		word.substr(suffix.offset).swap(stringSuffixes[c++]);
	}
}

void Automata::State::clear() {
	connections.clear();
	suffixes.clear();
	hashConnections = hashSuffixes = 42;
	isFinal = false;
}

bool Automata::State::slowEqual(const Automata &automata, const State &other) const {
	if (this == &other) {
		return true;
	}

	if (getHash(automata) != other.getHash(automata)) {
		return false;
	}

	if (isFinal != other.isFinal) {
		return false;
	}

	if (suffixes.size() != other.suffixes.size()) {
		return false;
	}

	if (connections != other.connections) {
		return false;
	}

	static WordList mine, others;
	buildSuffixes(automata, mine);
	other.buildSuffixes(automata, others);

	std::sort(mine.begin(), mine.end());
	std::sort(others.begin(), others.end());

	const bool result = mine == others;
	mine.resize(0);
	others.resize(0);

	return result;
}

std::string Automata::State::buildIdString() const {
	char buff[128];
	typedef unsigned long long llu;
	snprintf(buff, sizeof(buff), "%llu | %llu | %d \\n", llu(hashConnections), llu(hashSuffixes), int(isFinal));
	return buff;
}

void Automata::State::dumpGraph(GraphDump &graphDump) const {
	const std::string mine = buildIdString();
	for (const auto &child : connections) {

		graphDump.addEdge(mine, child.second->buildIdString(), std::string(1, child.first));
		child.second->dumpGraph(graphDump);
	}
}

bool Automata::State::verifyAcyclicity(std::unordered_set<const State *> &visited) const {
	visited.insert(this);
	for (const auto &item : connections) {
		if (contains<const State *>(visited, item.second)) {
			ac_assert(false && "Cycle detected");
			return false;
		}
		if (!item.second->verifyAcyclicity(visited)) {
			return false;
		}
	}
	visited.erase(visited.find(this));
	return true;
}

void Automata::State::rebuildConnectionsHash() const {
	hashConnections = 42;
	const std::hash<symbol> symbolHasher;

	for (auto &item : connections) {
		hashConnections = hashCombine(
			hashConnections, 
			hashCombine(
				symbolHasher(item.first),
				reinterpret_cast<uintptr_t>(item.second)
			)
		);
	}
}

void Automata::State::rebuildSuffixesHash(const Automata &automata) const {
	hashSuffixes = 42;

#if HASH_STRATEGY == HASH_STRATEGY_SUM
	for (const auto &suffix : suffixes) {
		const std::string &word = automata.getWord(suffix.wordIndex);
		for (int c = suffix.offset; c < word.size(); c++) {
			hashSuffixes += word[c];
		}
	}
#endif

#if HASH_STRATEGY == HASH_STRATEGY_XOR
	for (const auto &suffix : suffixes) {
		const std::string &word = automata.getWord(suffix.wordIndex);
		hashSuffixes ^= hash(word.data() + suffix.offset, word.size() - suffix.offset);
	}
#endif

#if HASH_STRATEGY == HASH_STRATEGY_SORT
	// Suffixes might not be in sorted order, so to compute consistent hashes sort them first
	std::vector<std::string> suffixSet;
	for (const auto &suffix : suffixes) {
		const std::string &word = automata.getWord(suffix.wordIndex);
		suffixSet.emplace_back(word.substr(suffix.offset));
	}
	std::sort(suffixSet.begin(), suffixSet.end());

	for (const std::string &suffix : suffixSet) {
		hashSuffixes = hashCombine(
			hashSuffixes,
			std::hash<std::string>()(suffix)
		);
	}
#endif
}
