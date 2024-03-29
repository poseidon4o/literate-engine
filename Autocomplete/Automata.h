// based on http://lml.bas.bg/~stoyan/J00-1002.pdf

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <list>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <string>

typedef char symbol;

#ifdef _DEBUG
#define AC_ASSERT_ENABLED 1
#define ac_assert(test) (!(test) ? __debugbreak() : (void)0)
#else
#define AC_ASSERT_ENABLED (0)
#define ac_assert(test) ((void)0)
#endif

#define HASH_STRATEGY_XOR 1
#define HASH_STRATEGY_SUM 2
#define HASH_STRATEGY_SORT 3

#define HASH_STRATEGY HASH_STRATEGY_SUM

/// Utility to check if a set contains an element
template <typename T>
inline bool contains(const std::unordered_set<T> &set, const T &value) {
	return set.find(value) != set.end();
}


/// Interface used to dump the contents of the Automata's internal graph
/// Edges are added explicitly and vertices are implicitly guessed from the edges, there are no unconnected vertices
struct GraphDump {
	/// Called before adding any edges of the graph
	virtual void start() = 0;

	/// Add an edge between two vertices, will be called multiple times for the same edge
	/// @param from - label of the start vertex
	/// @param to - label of the end vertex
	/// @param label - the name of the edge (one symbol)
	virtual void addEdge(const std::string &from, const std::string &to, const std::string &label) = 0;

	/// Called when all data is dumped
	virtual void done() = 0;

	~GraphDump() {}
};

/// The implementation of the automata recognizing the prefix/suffixes
struct Automata {
	/// List of words, used to initialize the automata
	typedef std::vector<std::string> WordList;

	/// Initialize empty automata, ready to call buildFromWordList on
	Automata();

	/// Clear all the memory associated with recognizing words
	void clear() {
		initEmpty();
	}

	/// Initialize empty automata, called by default ctor
	/// Needs to be called if word list will be changed
	void initEmpty();

	/// Build the automata to from a word list, the list's contents will be stolen
	void buildFromWordList(WordList &&wordList) {
		words = std::move(wordList);
		build();
	}

	/// Build the automata to from a word list, the list will be copied
	void buildFromWordList(const WordList &wordList) {
		words = wordList;
		build();
	}

	/// Get word by index from the list of recognized words
	/// @param index - the index of the word, no bounds checking is performed
	/// @retunr ref to the string
	const std::string &getWord(int index) const;

	/// Get all suffixes for a given prefix
	/// @param prefix - the prefix to search for
	/// @param suffixes[out] - insert all suffixes for the prefix in the set
	/// @return false if the prefix is not recognized, false otherwise
	bool getSuffixes(const std::string &prefix, WordList &suffixes) const;

	/// Get the default implementation of GraphDump that will write the data in graph-viz format
	/// @param filePath - the file path where the file will be created
	/// @return pointer to the implementation or nullptr if it fails to init with filePath
	GraphDump *getDefaultGraphDump(const std::string &filePath);

	/// Dump the internal graph structure using GraphDump interface
	/// @param graphDump - pointer to implementation of GraphDump
	/// @return true if graphDump was not nullptr, false otherwise
	bool dumpGraph(GraphDump *graphDump) const;

	/// Get the number of states with in the internal graph
	/// @return - the number of states, at least 1
	int getNumberOfStates() const {
		return allStates.size() - freeStates.size();
	}

	/// Number of words recognized by the automata
	int getNumberOfWords() const {
		return words.size();
	}

	/// Number of all symbols in all words
	int getNumberOfTotalSymbols() const {
		return totalSymbols;
	}

	/// Slow check for all prefixes and all suffixes in the automata
	/// NOTE: Does nothing in Release
	bool runVerify() const;

	/// Disable copy
	Automata(const Automata &) = delete;
	/// Disable operator=
	Automata& operator=(const Automata &) = delete;

	/// Performance stat for building the collisions
	int64_t getBuildCollisions() const {
		return collisions;
	}
private:
	/// Internal structure that holds a single state of the automata
	struct State {
		/// Both need to be ordered so that their hash depends on contents only and not on order of insertion
		/// TODO: Change to hash map and last symbol
		typedef std::map<symbol, State *> ConnectionMap;

		/// Maps word index to offset in that word in the Automata word list, thus avoiding storing actual words in each state
		struct Suffix {
			int wordIndex = -1;
			int offset = -1;

			Suffix(int wordIndex, int offset)
				: wordIndex(wordIndex)
				, offset(offset)
			{}
		};
		typedef std::vector<Suffix> SuffixList;

		/// Find a child connection for a symbol, can be nullptr if not found
		/// @param transition - the symbol trying to find in the connections
		/// @return pointer to the child state or nullptr if there is no such transition
		State *findConnection(symbol transition) const;

		/// Sets the final flag to true for this state
		void setIsFinalState();

		/// Check if this state is final
		/// @return true if there is some word ending on this state, false otherwise
		bool isFinalState() const;

		/// Add new child connection
		/// @param transition - the symbol used to reach the child
		/// @param child - pointer to the child state
		void addConnection(symbol transition, State *child);

		/// Append suffix to this state
		/// @param automata - used to rebuild the hash of the suffixes when they change
		/// @param wordIndex - the word that the suffix is in
		/// @param offset - the offset in the word where the suffix starts
		void appendSuffix(const Automata &automata, int wordIndex, int offset);

		/// Init own suffixes list from a "parent" state and transition symbol to this
		/// @param automata - the automata is used to obtain the value for the actual suffixes
		/// @param parent - the parent state to read suffixes from
		/// @param transition - the symbol used to reach this from parent
		void initSuffixesFrom(const Automata &automata, const State &parent, symbol transition);

		/// Replace an already inserted connection with new state, used when new child state is already in registry
		/// @param newChild - the new value for the transition
		/// @param transition - the symbol that is used to reach the old state
		void replaceChild(State *newChild, symbol transition);

		/// Slow check to determine if a given state is direct child of this one
		/// @param state - pointer to the state to check
		/// @return true if there is some symbol by which @state is reached, false otherwise
		bool hasChild(const State *state) const;

		/// Get the hash of this state's connections, suffixes, and final flag
		/// @return the hash
		size_t getHash(const Automata &automata) const;

		/// Get the number of connections starting from this state
		/// @return - the number of connections
		int getNumChildren() const;

		/// Get all suffixes starting from this state
		/// @param automata - the automata as suffixes need to be obtained from the word list
		/// @param stringSuffixes[out] - set where all suffixes will be inserted
		void buildSuffixes(const Automata &automata, WordList &stringSuffixes) const;

		/// Clear all internal data for this state
		void clear();

		/// Equality check between two states, slow as it performs deep check and not just hash compare
		/// @param automata - used to get the actual values for the suffixes
		/// @param other - the state to compare to
		/// @return true if both this and other are equal, false otherwise
		bool slowEqual(const Automata &automata, const State &other) const;

		/// Dump the graph starting at this state to a GraphDump, calls the same method for all connections
		/// @param graphDump - implementation of GraphDump
		void dumpGraph(GraphDump &graphDump) const;

		/// Verify that the graph starting at this state is acyclic
		/// when called on root state will check that there are no cycles in the whole automata
		/// @param visited - set of all states visited to reach this one
		/// @return true if no cycle found, false otherwise
		bool verifyAcyclicity(std::unordered_set<const State *> &visited) const;

		State() {
			suffixes.reserve(32);
		}

	private:
		/// All transitions for this state, maps symbol to State *
		ConnectionMap connections;
		/// List of all suffixes for this state, the right language for the prefix ending in this state
		/// Suffixes are not stored as strings, but instead as a pair of indices:
		/// key = index in the Automata::words member
		/// value = the offset in this word where the suffix starts
		SuffixList suffixes;
		/// Flag set to true if some word ends with this state
		bool isFinal = false;
		/// Hash of this state's connections, used for de-duplication of states in Automata::registry
		mutable size_t hashConnections = 42;
		/// Hash of this state's suffixes, used for de-duplication of states in Automata::registry
		mutable size_t hashSuffixes = 42;

		/// Build unique string for this State used for GraphDump
		std::string buildIdString() const;

		/// Re-compute the hashConnections member, needs to be called when connections change
		void rebuildConnectionsHash() const;

		/// Re-compute the hashSuffixes member, needs to be called when suffixes change
		/// @param automata - the automata is needed because suffixes are stored as indices in the word list
		void rebuildSuffixesHash(const Automata &automata) const;
	};

	/// Default implementation dumping the data into graph-viz format
	struct DotGraphViz : GraphDump {
		typedef std::unordered_map<std::string, std::string> EdgeToNode;
		typedef std::unordered_map<std::string, EdgeToNode> Graph;

		/// The file stream where the data will be written
		std::fstream file;
		/// The graph data stored to make sure not to write duplicate edges
		Graph graph;

		void start() override {}

		void done() override {
			flush();
		}

		bool init(const std::string &path) {
			graph.clear();
			file.open(path, std::ios::out);
			if (!file) {
				std::cout << "Failed to write viz";
				return false;
			}
			file << "digraph G {" << std::endl;
			return true;
		}

		void flush() {
			if (file) {
				file << "}";
				file.flush();
				file.close();
			}
		}

		bool addConnection(const std::string& from, const std::string& to, const std::string& label) {
			const Graph::iterator it = graph.find(from);
			if (it == graph.end()) {
				graph[from][to] = label;
				return true;
			}
			const EdgeToNode::iterator edge = it->second.find(to);
			if (edge == it->second.end()) {
				it->second[to] = label;
				return true;
			}

			return false;
		}

		void addEdge(const std::string& from, const std::string& to, const std::string& label) override {
			if (addConnection(from, to, label)) {
				file << "\"" << from << "\" -> \"" << to << "\" [ label = \"" << label<< "\" ] " << std::endl;
			}
		}
	};

	/// Checks if the automata will find all suffixes for a given prefix comparing the list of recognized words
	/// NOTE: Does nothing in Release
	/// @param start - the index of the word that the prefix is taken from
	/// @param prefix - the prefix
	/// @return true if all suffixes in the word list are correctly returned by getSuffixes
	bool verifyPrefix(int start, const std::string &prefix) const;

	/// Wrapper over State* to provide custom hash and operator==
	struct StatePtr {
		Automata *automata = nullptr;
		State *state = nullptr;

		bool operator==(const StatePtr &other) const {
			ac_assert(state);
			ac_assert(other.state);
			++(automata->collisions);
			return state->slowEqual(*automata, *other.state);
		}

		struct Hasher {
			size_t operator()(const StatePtr &statePtr) const {
				return statePtr.state->getHash(*statePtr.automata);
			}
		};
	};

	/// Hash set of all unique states
	typedef std::unordered_set<StatePtr, StatePtr::Hasher> Registry;

	/// Instead of allocating new states directly on the heap, they are added to this list
	/// This allows de-allocation to be easier to implement (the isn't any)
	/// TODO: this could be deuque but to utilize freeStates there needs to be a way to obtain index/iterator in deque from pointer to element
	std::list<State> allStates;
	/// A queue of states that were replaced and are therefor available to be used again
	/// When "allocating" new state, first freeStates is checked and if empty then new state is added to allStates
	std::queue<State*> freeStates;
	/// The starting point for automata traversal, contains all words, the first item in allStates
	State *rootState = nullptr;
	/// Set of all unique states in the automata, if new state is created and is already "in" the registry,
	/// then the new state is discarded and replaced by the one in the registry
	Registry registry;
	/// Stores all the words this automata recognizes, used to minimize memory in the states
	WordList words;
	/// Default implementation of GraphDump to save the internal representation in graph-viz format
	DotGraphViz dotGraphViz;
	/// The total number of symbols in all words
	int totalSymbols = 0;
	/// The number of times the has of State::getHash collided
	/// Performance stats collected while building the automata
	int64_t collisions = 0;

	/// Builds the automata from the word list
	void build();

	/// Find the last state for a given prefix
	/// @param prefix - some string to find state for
	/// @return pointer to the state where all suffixes for @prefix start or nullptr if prefix is not recognized
	const State *findState(const std::string &prefix) const;

	/// Find the last state for the longest prefix of a word and update all states of the prefix for this word
	/// @param wordIndex - the global index of the word
	/// @param steps[out] - the length of the prefix that is already in the automata
	/// @return - pointer to the last state of the prefix, contained in the automata
	///           never nullptr, can be the root state (steps will be 0)
	State *addWordPrefix(int wordIndex, int &steps);

	/// Create suffix nodes for a given word starting from some offset\
	/// @param start - the last state of the prefix of the word
	/// @param wordIndex - the global index of the word
	/// @param offset - the start of the suffix to create nodes
	void createNodes(State *start, int wordIndex, int offset);

	/// Check if the given state is detached from the automata, used for debug
	/// @param state - the state to check
	/// @return true if there is no other state pointing to this one, false otherwise
	bool isDetached(State *state);

	/// Minimize the part of the automata starting from given state and for a given word (last one added)
	/// @param start - the start of the potentially non minimized part
	/// @param wordIndex - the global index of the word last added
	/// @param offset - the offset in the word that start corresponds to
	void minimize(State *start, int wordIndex, int offset);
};