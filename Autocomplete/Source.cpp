#include "Automata.h"

#include <chrono>

bool readFileLines(const std::string &path, Automata::WordList &list) {
	list.clear();

	std::ifstream wordFile(path);
	if (!wordFile) {
		return false;
	}

	std::cout << "Reading ..." << std::endl;
	std::string line;
	while (getline(wordFile, line)) {
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
			line.pop_back();
		}
		list.push_back(line);
	}

	return true;
}

struct timer {
	typedef std::chrono::high_resolution_clock clock_t;
	typedef std::chrono::high_resolution_clock::time_point time_point;
	typedef std::chrono::milliseconds ms_t;

	std::string name;
	time_point start;
	bool print = true;

	timer(const std::string &name)
		: name(name)
		, start(clock_t::now())
	{}

	ms_t::rep getElapsed() {
		const ms_t elapsed = std::chrono::duration_cast<ms_t>(clock_t::now() - start);
		print = false;
		return elapsed.count();
	}

	~timer() {
		if (print) {
			const ms_t::rep ms = getElapsed();
			std::cout << "Timer : [" << name << "] " << ms << "ms." << std::endl;
		}
	}
};


int main(int, char *[]) {
	std::string files[] = {
		"lists/1k.txt",
		"lists/3k.txt",
		"lists/58k.txt",
		"lists/naughty.txt"
	};

#if 0
	/*
	 * lists/1k.txt states: 964 / 2.24
	 * lists/3k.txt states: 2621 / 9.54
	 * lists/58k.txt states: 27025 / 228.12
	 * lists/naughty.txt states: 925 / 0
	 */
	for (const std::string &file : files) {
		Automata::WordList words;
		if (!readFileLines(file, words)) {
			std::cout << "Failed to read " << file;
			continue;
		}

		timer::ms_t::rep total = 0;
		const int repeat = 25;
		for (int c = 0; c < repeat; c++) {
			Automata dict;
			{
				timer t("");
				dict.buildFromWordList(words);
				total += t.getElapsed();
			}
		}
		std::cout << "Time for " << file << ": " << (total / double(repeat)) << "ms." << std::endl;
#if AC_ASSERT_ENABLED
		Automata dict;
		dict.buildFromWordList(words);
		std::cout << "Verify: " << dict.runVerify() << std::endl;
		std::cout << file << " states: " << dict.getNumberOfStates() << std::endl;
#endif
	}

	std::cout << "Collisions:" << Automata::collisions << std::endl;

	return 0;
#endif

	std::string &fileName = files[2];
	Automata dict;
	{
		Automata::WordList words;
		if (!readFileLines(fileName, words)) {
			std::cout << "Can't open file";
			return 0;
		}

		std::cout << "Building ..." << std::endl;
		std::vector<std::string> testWords = {
			// "follow", "feast", "fear", "fart", "farting", "pestering", "pester", "testtest",
			// "test", "tests", "testing", "tester", "teaser", "training", "pining", "test", "te",
			// "aAZ", "bAB", "eAB", "eAZ",
		};
		dict.buildFromWordList(std::move(testWords.empty() ? words : testWords));

		std::cout << "Writing graph-viz ..." << std::endl;
		dict.dumpGraph(dict.getDefaultGraphDump("viz.dot"));

		std::cout << "Running tests ..." << std::endl;
		ac_assert(dict.runVerify());

		std::cout << "States in automata: " << dict.getNumberOfStates() << std::endl;
		std::cout << "Words in automata: " << dict.getNumberOfWords() << std::endl;
		std::cout << "Symbols in automata: " << dict.getNumberOfTotalSymbols() << std::endl;
	}

	std::string input;

	std::cout << "Enter prefix: ";
	while (std::cin >> input) {
		std::set<std::string> suffixes;
		dict.getSuffixes(input, suffixes);

		if (suffixes.empty()) {
			std::cout << "> no suffixes" << std::endl;
		} else {
			for (const std::string &suffix : suffixes) {
				std::cout << input << suffix << std::endl;
			}
			std::cout << "> " << suffixes.size() << " suffixes" << std::endl;
		}
	}

	return 0;
}